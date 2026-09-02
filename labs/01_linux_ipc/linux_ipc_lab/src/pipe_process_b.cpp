#include "linux_ipc_lab/system.hpp"
#include "linux_ipc_lab/wire_message.hpp"

#include <cstdint>
#include <iostream>
#include <limits>

#include <unistd.h>

int main(int argc, char ** argv)
{
  return linux_ipc_lab::guarded_main(
    [&]() {
      const linux_ipc_lab::Arguments arguments{argc, argv};
      arguments.ensure_only({"--fd"});
      const std::uint32_t missing = std::numeric_limits<std::uint32_t>::max();
      const std::uint32_t raw_fd = arguments.get_u32(
        "--fd", missing, 0U,
        static_cast<std::uint32_t>(std::numeric_limits<int>::max()));
      if (raw_fd == missing) {
        throw std::invalid_argument(
                "--fd is required; launch this program through pipe_process_a");
      }
      linux_ipc_lab::UniqueFd read_end{static_cast<int>(raw_fd)};

      std::cout << "pipe_process_b pid=" << ::getpid()
                << " inherited_read_fd=" << read_end.get() << '\n';

      std::uint64_t samples = 0U;
      linux_ipc_lab::WireBuffer bytes{};
      while (linux_ipc_lab::read_exact(read_end.get(), bytes.data(), bytes.size())) {
        const auto sample = linux_ipc_lab::decode_sample(bytes);
        std::cout << linux_ipc_lab::describe_sample("pipe_rx", sample) << '\n';
        ++samples;
      }
      std::cout << "pipe_process_b eof samples=" << samples << '\n';
    });
}
