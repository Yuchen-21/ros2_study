#include "linux_ipc_lab/socket_helpers.hpp"
#include "linux_ipc_lab/system.hpp"
#include "linux_ipc_lab/wire_message.hpp"

#include <cerrno>
#include <cstdint>
#include <iostream>
#include <stdexcept>

#include <sys/socket.h>

namespace
{

[[nodiscard]] linux_ipc_lab::UniqueFd connect_with_deadline(
  const sockaddr_in & destination, const std::uint32_t timeout_ms)
{
  const std::uint64_t deadline =
    linux_ipc_lab::monotonic_now_ns() +
    static_cast<std::uint64_t>(timeout_ms) * 1'000'000ULL;
  int final_error = ECONNREFUSED;

  do {
    auto socket = linux_ipc_lab::make_socket(AF_INET, SOCK_STREAM);
    if (::connect(
        socket.get(), reinterpret_cast<const sockaddr *>(&destination),
        static_cast<socklen_t>(sizeof(destination))) == 0)
    {
      return socket;
    }
    final_error = errno;
    if (final_error != ECONNREFUSED && final_error != EINTR) {
      errno = final_error;
      linux_ipc_lab::throw_errno("connect(TCP loopback)");
    }
    linux_ipc_lab::sleep_for_ms(25U);
  } while (linux_ipc_lab::monotonic_now_ns() < deadline);

  throw std::system_error(
          final_error, std::generic_category(),
          "connect(TCP loopback) timed out waiting for subscriber");
}

}  // namespace

int main(int argc, char ** argv)
{
  return linux_ipc_lab::guarded_main(
    [&]() {
      const linux_ipc_lab::Arguments arguments{argc, argv};
      arguments.ensure_only({"--count", "--interval-ms", "--port", "--connect-timeout-ms"});
      const std::uint32_t count = arguments.get_u32("--count", 10U, 1U, 100'000U);
      const std::uint32_t interval_ms =
        arguments.get_u32("--interval-ms", 20U, 0U, 60'000U);
      const std::uint16_t port = static_cast<std::uint16_t>(
        arguments.get_u32("--port", linux_ipc_lab::kDefaultTcpPort, 1U, 65'535U));
      const std::uint32_t connect_timeout_ms =
        arguments.get_u32("--connect-timeout-ms", 5'000U, 1U, 300'000U);

      const sockaddr_in destination = linux_ipc_lab::make_loopback_address(port);
      auto connection = connect_with_deadline(destination, connect_timeout_ms);
      std::cout << "tcp_publisher connected=127.0.0.1:" << port
                << " count=" << count << '\n';

      for (std::uint32_t sequence = 0U; sequence < count; ++sequence) {
        const auto bytes = linux_ipc_lab::encode_sample(
          linux_ipc_lab::make_sample(sequence));
        linux_ipc_lab::send_all(connection.get(), bytes.data(), bytes.size());
        linux_ipc_lab::sleep_for_ms(interval_ms);
      }

      std::cout << "tcp_publisher complete samples=" << count << '\n';
    });
}
