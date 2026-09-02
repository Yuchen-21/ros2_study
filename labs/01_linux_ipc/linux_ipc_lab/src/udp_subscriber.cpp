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
      arguments.ensure_only({"--count", "--port", "--timeout-ms"});
      const std::uint32_t count = arguments.get_u32("--count", 10U, 1U, 100'000U);
      const std::uint16_t port = static_cast<std::uint16_t>(
        arguments.get_u32("--port", linux_ipc_lab::kDefaultUdpPort, 1U, 65'535U));
      const std::uint32_t timeout_ms =
        arguments.get_u32("--timeout-ms", 5'000U, 1U, 300'000U);

      auto socket = linux_ipc_lab::make_socket(AF_INET, SOCK_DGRAM);
      linux_ipc_lab::set_receive_timeout(socket.get(), timeout_ms);
      const sockaddr_in address = linux_ipc_lab::make_loopback_address(port);
      if (::bind(
          socket.get(), reinterpret_cast<const sockaddr *>(&address),
          static_cast<socklen_t>(sizeof(address))) != 0)
      {
        linux_ipc_lab::throw_errno("bind(UDP loopback)");
      }

      std::cout << "udp_subscriber listening=127.0.0.1:" << port
                << " count=" << count << '\n';

      linux_ipc_lab::SequenceTracker tracker;
      linux_ipc_lab::WireBuffer bytes{};
      for (std::uint32_t received = 0U; received < count; ++received) {
        linux_ipc_lab::receive_exact_datagram(
          socket.get(), bytes.data(), bytes.size());
        const auto sample = linux_ipc_lab::decode_sample(bytes);
        tracker.observe(sample.sequence);
        std::cout << linux_ipc_lab::describe_sample("udp_rx", sample) << '\n';
      }

      std::cout << "udp_subscriber complete samples=" << count
                << " gaps=" << tracker.gaps()
                << " reordered_or_duplicate=" << tracker.reordered_or_duplicate()
                << '\n';
    });
}
