#include "linux_ipc_lab/shared_memory.hpp"
#include "linux_ipc_lab/system.hpp"
#include "linux_ipc_lab/wire_message.hpp"

#include <cerrno>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace
{

[[nodiscard]] linux_ipc_lab::UniqueFd open_with_deadline(
  const std::string & name, const std::uint32_t timeout_ms)
{
  const std::uint64_t deadline =
    linux_ipc_lab::monotonic_now_ns() +
    static_cast<std::uint64_t>(timeout_ms) * 1'000'000ULL;
  do {
    const int fd = ::shm_open(name.c_str(), O_RDWR | O_CLOEXEC, 0);
    if (fd >= 0) {
      return linux_ipc_lab::UniqueFd{fd};
    }
    if (errno != ENOENT && errno != EINTR) {
      linux_ipc_lab::throw_errno("shm_open(reader " + name + ")");
    }
    linux_ipc_lab::sleep_for_ms(10U);
  } while (linux_ipc_lab::monotonic_now_ns() < deadline);

  throw std::runtime_error("timed out waiting for shared-memory writer to create " + name);
}

}  // namespace

int main(int argc, char ** argv)
{
  return linux_ipc_lab::guarded_main(
    [&]() {
      const linux_ipc_lab::Arguments arguments{argc, argv};
      arguments.ensure_only({"--name", "--count", "--timeout-ms"});
      const std::string name = arguments.get_string(
        "--name", linux_ipc_lab::default_shared_memory_name());
      linux_ipc_lab::validate_shared_memory_name(name);
      const std::uint32_t count = arguments.get_u32("--count", 10U, 1U, 100'000U);
      const std::uint32_t timeout_ms =
        arguments.get_u32("--timeout-ms", 5'000U, 1U, 300'000U);

      auto shared_fd = open_with_deadline(name, timeout_ms);
      linux_ipc_lab::MappedRegion mapping;
      const std::uint64_t initialization_deadline =
        linux_ipc_lab::monotonic_now_ns() +
        static_cast<std::uint64_t>(timeout_ms) * 1'000'000ULL;
      while (mapping.get() == MAP_FAILED) {
        {
          linux_ipc_lab::FileLock initialization_lock{shared_fd.get(), LOCK_SH};
          struct stat metadata {};
          if (::fstat(shared_fd.get(), &metadata) != 0) {
            linux_ipc_lab::throw_errno("fstat(shared region)");
          }
          if (metadata.st_size ==
            static_cast<off_t>(sizeof(linux_ipc_lab::SharedRegion)))
          {
            mapping = linux_ipc_lab::map_shared_region(shared_fd.get());
            linux_ipc_lab::validate_shared_region(
              *mapping.as<linux_ipc_lab::SharedRegion>());
          }
        }
        if (mapping.get() == MAP_FAILED) {
          if (linux_ipc_lab::monotonic_now_ns() >= initialization_deadline) {
            throw std::runtime_error(
                    "shared-memory region did not reach the expected initialized size");
          }
          // A reader can open the name in the tiny interval between the
          // creator's shm_open and LOCK_EX. Release LOCK_SH and retry so the
          // creator can complete ftruncate and protocol initialization.
          linux_ipc_lab::sleep_for_ms(1U);
        }
      }

      auto * const region = mapping.as<linux_ipc_lab::SharedRegion>();
      std::cout << "shm_reader name=" << name
                << " mapped_at=" << mapping.get()
                << " count=" << count << '\n';

      {
        linux_ipc_lab::PthreadLock lock{region->mutex};
        region->reader_ready = 1U;
        linux_ipc_lab::notify_all(region->updated);
      }

      std::uint64_t last_generation = 0U;
      std::uint32_t received = 0U;
      while (received < count) {
        linux_ipc_lab::SensorSample local_sample{};
        std::uint64_t local_generation = 0U;
        {
          linux_ipc_lab::PthreadLock lock{region->mutex};
          const timespec deadline = linux_ipc_lab::monotonic_deadline_after(timeout_ms);
          while (region->generation == last_generation && region->shutdown == 0U) {
            if (!linux_ipc_lab::wait_until(region->updated, lock, deadline)) {
              throw std::runtime_error("timed out waiting for shared-memory sample");
            }
          }
          if (region->generation == last_generation && region->shutdown != 0U) {
            break;
          }

          // Copy the small sample while holding the ownership lock, then release
          // it before formatting or writing stdout.
          local_sample = region->sample;
          local_generation = region->generation;
          last_generation = local_generation;
          region->consumed_generation = local_generation;
          linux_ipc_lab::notify_all(region->updated);
        }

        std::cout << "shm_rx generation=" << local_generation << ' '
                  << linux_ipc_lab::describe_sample("sample", local_sample)
                  << '\n';
        ++received;
      }

      if (received != count) {
        throw std::runtime_error(
                "writer shut down after " + std::to_string(received) +
                " samples; reader expected " + std::to_string(count));
      }
      std::cout << "shm_reader complete samples=" << received
                << " skipped_generations=0" << '\n';
    });
}
