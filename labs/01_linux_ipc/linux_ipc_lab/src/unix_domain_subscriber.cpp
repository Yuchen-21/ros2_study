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
      arguments.ensure_only({"--count", "--path", "--timeout-ms"});
      const std::uint32_t count = arguments.get_u32("--count", 10U, 1U, 100'000U);
      const std::uint32_t timeout_ms =
        arguments.get_u32("--timeout-ms", 5'000U, 1U, 300'000U);
      const std::string path = arguments.get_string(
        "--path", linux_ipc_lab::default_unix_socket_path());

      auto socket = linux_ipc_lab::make_socket(AF_UNIX, SOCK_DGRAM);
      linux_ipc_lab::set_receive_timeout(socket.get(), timeout_ms);
      const sockaddr_un address = linux_ipc_lab::make_unix_address(path);
      linux_ipc_lab::UnixSocketPathOwner path_owner{path};
      if (::bind(
          socket.get(), reinterpret_cast<const sockaddr *>(&address),
          static_cast<socklen_t>(sizeof(address))) != 0)
      {
        linux_ipc_lab::throw_errno(
          "bind(" + path + "); remove it only after confirming it is stale");
      }
      path_owner.mark_bound();

      std::cout << "uds_subscriber path=" << path
                << " count=" << count << '\n';

      linux_ipc_lab::SequenceTracker tracker;
      linux_ipc_lab::WireBuffer bytes{};
      for (std::uint32_t received = 0U; received < count; ++received) {
        linux_ipc_lab::receive_exact_datagram(
          socket.get(), bytes.data(), bytes.size());
        const auto sample = linux_ipc_lab::decode_sample(bytes);
        tracker.observe(sample.sequence);
        std::cout << linux_ipc_lab::describe_sample("uds_rx", sample) << '\n';
      }

      std::cout << "uds_subscriber complete samples=" << count
                << " gaps=" << tracker.gaps()
                << " reordered_or_duplicate=" << tracker.reordered_or_duplicate()
                << '\n';
    });
}
