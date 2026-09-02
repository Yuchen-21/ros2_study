#pragma once

#include "linux_ipc_lab/system.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace linux_ipc_lab
{

constexpr std::uint16_t kDefaultTcpPort = 39001U;
constexpr std::uint16_t kDefaultUdpPort = 39002U;

[[nodiscard]] inline UniqueFd make_socket(
  const int domain, const int type, const int protocol = 0)
{
  const int fd = ::socket(domain, type | SOCK_CLOEXEC, protocol);
  if (fd < 0) {
    throw_errno("socket");
  }
  return UniqueFd{fd};
}

[[nodiscard]] inline sockaddr_in make_loopback_address(const std::uint16_t port)
{
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  const int conversion = ::inet_pton(AF_INET, "127.0.0.1", &address.sin_addr);
  if (conversion != 1) {
    if (conversion == 0) {
      throw std::runtime_error("127.0.0.1 was unexpectedly rejected by inet_pton");
    }
    throw_errno("inet_pton");
  }
  return address;
}

[[nodiscard]] inline std::string default_unix_socket_path()
{
  return "/tmp/ros2_comm_lab_uds_" +
         std::to_string(static_cast<unsigned long>(::getuid())) + ".sock";
}

[[nodiscard]] inline sockaddr_un make_unix_address(const std::string & path)
{
  if (path.empty()) {
    throw std::invalid_argument("Unix socket path must not be empty");
  }

  sockaddr_un address{};
  address.sun_family = AF_UNIX;
  if (path.size() >= sizeof(address.sun_path)) {
    throw std::invalid_argument(
            "Unix socket path is too long (" + std::to_string(path.size()) +
            " bytes; maximum is " + std::to_string(sizeof(address.sun_path) - 1U) + ")");
  }
  std::copy(path.begin(), path.end(), address.sun_path);
  address.sun_path[path.size()] = '\0';
  return address;
}

class UnixSocketPathOwner
{
public:
  explicit UnixSocketPathOwner(std::string path) : path_(std::move(path)) {}

  ~UnixSocketPathOwner()
  {
    if (owns_path_) {
      (void)::unlink(path_.c_str());
    }
  }

  UnixSocketPathOwner(const UnixSocketPathOwner &) = delete;
  UnixSocketPathOwner & operator=(const UnixSocketPathOwner &) = delete;

  void mark_bound() noexcept {owns_path_ = true;}

private:
  std::string path_;
  bool owns_path_{false};
};

inline void enable_reuse_address(const int fd)
{
  constexpr int enabled = 1;
  if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled)) != 0) {
    throw_errno("setsockopt(SO_REUSEADDR)");
  }
}

}  // namespace linux_ipc_lab
