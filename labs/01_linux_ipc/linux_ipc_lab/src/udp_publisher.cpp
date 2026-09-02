#include "linux_ipc_lab/socket_helpers.hpp"
#include "linux_ipc_lab/system.hpp"
#include "linux_ipc_lab/wire_message.hpp"

#include <cstdint>
#include <iostream>

#include <sys/socket.h>

int main(int argc, char ** argv)
{
  return linux_ipc_lab::guarded_main(
    [&]() {
      const linux_ipc_lab::Arguments arguments{argc, argv};
      arguments.ensure_only({"--count", "--interval-ms", "--port"});
      const std::uint32_t count = arguments.get_u32("--count", 10U, 1U, 100'000U);
      const std::uint32_t interval_ms =
        arguments.get_u32("--interval-ms", 20U, 0U, 60'000U);
      const std::uint16_t port = static_cast<std::uint16_t>(
        arguments.get_u32("--port", linux_ipc_lab::kDefaultUdpPort, 1U, 65'535U));

      auto socket = linux_ipc_lab::make_socket(AF_INET, SOCK_DGRAM);
      const sockaddr_in destination = linux_ipc_lab::make_loopback_address(port);
      std::cout << "udp_publisher destination=127.0.0.1:" << port
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

      std::cout << "udp_publisher complete samples=" << count << '\n';
    });
}
