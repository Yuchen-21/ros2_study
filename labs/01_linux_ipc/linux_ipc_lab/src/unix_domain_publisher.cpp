#include "linux_ipc_lab/socket_helpers.hpp"
#include "linux_ipc_lab/system.hpp"
#include "linux_ipc_lab/wire_message.hpp"

#include <cstdint>
#include <iostream>
#include <string>

#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char ** argv)
{
  return linux_ipc_lab::guarded_main(
    [&]() {
      const linux_ipc_lab::Arguments arguments{argc, argv};
      arguments.ensure_only({"--count", "--interval-ms", "--path"});
      const std::uint32_t count = arguments.get_u32("--count", 10U, 1U, 100'000U);
      const std::uint32_t interval_ms =
        arguments.get_u32("--interval-ms", 20U, 0U, 60'000U);
      const std::string path = arguments.get_string(
        "--path", linux_ipc_lab::default_unix_socket_path());

      auto socket = linux_ipc_lab::make_socket(AF_UNIX, SOCK_DGRAM);
      const sockaddr_un destination = linux_ipc_lab::make_unix_address(path);
      std::cout << "uds_publisher destination=" << path
                << " count=" << count << '\n';

      for (std::uint32_t sequence = 0U; sequence < count; ++sequence) {
        const auto bytes = linux_ipc_lab::encode_sample(
          linux_ipc_lab::make_sample(sequence));
        linux_ipc_lab::send_datagram(
          socket.get(), bytes.data(), bytes.size(),
          reinterpret_cast<const sockaddr *>(&destination),
          static_cast<socklen_t>(sizeof(destination)));
        linux_ipc_lab::sleep_for_ms(interval_ms);
      }

      std::cout << "uds_publisher complete samples=" << count << '\n';
    });
}
