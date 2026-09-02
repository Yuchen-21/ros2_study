#include "linux_ipc_lab/shared_memory.hpp"
#include "linux_ipc_lab/system.hpp"
#include "linux_ipc_lab/wire_message.hpp"

#include <cerrno>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <system_error>

#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <unistd.h>

int main(int argc, char ** argv)
{
  return linux_ipc_lab::guarded_main(
    [&]() {
      const linux_ipc_lab::Arguments arguments{argc, argv};
      arguments.ensure_only({"--name", "--count", "--interval-ms", "--timeout-ms"});
      const std::string name = arguments.get_string(
        "--name", linux_ipc_lab::default_shared_memory_name());
      linux_ipc_lab::validate_shared_memory_name(name);
      const std::uint32_t count = arguments.get_u32("--count", 10U, 1U, 100'000U);
      const std::uint32_t interval_ms =
        arguments.get_u32("--interval-ms", 20U, 0U, 60'000U);
      const std::uint32_t timeout_ms =
        arguments.get_u32("--timeout-ms", 5'000U, 1U, 300'000U);

      const int raw_fd = ::shm_open(
        name.c_str(), O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC,
        static_cast<mode_t>(0600));
      if (raw_fd < 0) {
        if (errno == EEXIST) {
          throw std::runtime_error(
                  "shared-memory object already exists: " + name +
                  "; verify no owner is alive before removing the stale object");
        }
        linux_ipc_lab::throw_errno("shm_open(create " + name + ")");
      }
      linux_ipc_lab::UniqueFd shared_fd{raw_fd};
      linux_ipc_lab::SharedMemoryNameOwner name_owner{name};
      name_owner.mark_created();

      linux_ipc_lab::MappedRegion mapping;
      {
        // Readers take LOCK_SH before inspecting size/layout, so they cannot map
        // the object between shm_open and ftruncate/initialization.
        linux_ipc_lab::FileLock initialization_lock{shared_fd.get(), LOCK_EX};
        if (::ftruncate(
            shared_fd.get(), static_cast<off_t>(sizeof(linux_ipc_lab::SharedRegion))) != 0)
        {
          linux_ipc_lab::throw_errno("ftruncate(shared region)");
        }
        mapping = linux_ipc_lab::map_shared_region(shared_fd.get());
        linux_ipc_lab::initialize_shared_region(
          mapping.as<linux_ipc_lab::SharedRegion>());
      }

      auto * const region = mapping.as<linux_ipc_lab::SharedRegion>();
      std::cout << "shm_writer name=" << name
                << " mapped_at=" << mapping.get()
                << " waiting_for_reader=true"
                << " count=" << count << '\n';

      {
        linux_ipc_lab::PthreadLock lock{region->mutex};
        const timespec deadline = linux_ipc_lab::monotonic_deadline_after(timeout_ms);
        while (region->reader_ready == 0U) {
          if (!linux_ipc_lab::wait_until(region->updated, lock, deadline)) {
            throw std::runtime_error("timed out waiting for shared-memory reader");
          }
        }
      }

      for (std::uint32_t sequence = 0U; sequence < count; ++sequence) {
        {
          linux_ipc_lab::PthreadLock lock{region->mutex};
          const timespec deadline = linux_ipc_lab::monotonic_deadline_after(timeout_ms);
          while (region->consumed_generation != region->generation) {
            if (!linux_ipc_lab::wait_until(region->updated, lock, deadline)) {
              throw std::runtime_error("timed out waiting for reader to release the shared slot");
            }
          }
          region->sample = linux_ipc_lab::make_sample(sequence);
          ++region->generation;
          linux_ipc_lab::notify_all(region->updated);
        }
        linux_ipc_lab::sleep_for_ms(interval_ms);
      }

      {
        linux_ipc_lab::PthreadLock lock{region->mutex};
        const timespec deadline = linux_ipc_lab::monotonic_deadline_after(timeout_ms);
        while (region->consumed_generation != region->generation) {
          if (!linux_ipc_lab::wait_until(region->updated, lock, deadline)) {
            throw std::runtime_error("timed out waiting for final sample acknowledgement");
          }
        }
        region->shutdown = 1U;
        linux_ipc_lab::notify_all(region->updated);
      }

      // Removing the name does not invalidate mappings already held by the reader.
      name_owner.unlink_now();
      std::cout << "shm_writer complete samples=" << count
                << " unlinked=true" << '\n';
    });
}
