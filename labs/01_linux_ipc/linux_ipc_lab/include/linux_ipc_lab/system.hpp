#pragma once

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>

#include <fcntl.h>
#include <poll.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

namespace linux_ipc_lab
{

[[noreturn]] inline void throw_errno(const std::string & operation)
{
  throw std::system_error(errno, std::generic_category(), operation);
}

inline void check_pthread(const int error_code, const std::string & operation)
{
  if (error_code != 0) {
    throw std::system_error(error_code, std::generic_category(), operation);
  }
}

class UniqueFd
{
public:
  UniqueFd() noexcept = default;
  explicit UniqueFd(const int fd) noexcept : fd_(fd) {}

  ~UniqueFd()
  {
    reset();
  }

  UniqueFd(const UniqueFd &) = delete;
  UniqueFd & operator=(const UniqueFd &) = delete;

  UniqueFd(UniqueFd && other) noexcept : fd_(other.release()) {}

  UniqueFd & operator=(UniqueFd && other) noexcept
  {
    if (this != &other) {
      reset(other.release());
    }
    return *this;
  }

  [[nodiscard]] int get() const noexcept {return fd_;}
  [[nodiscard]] explicit operator bool() const noexcept {return fd_ >= 0;}

  int release() noexcept
  {
    const int released = fd_;
    fd_ = -1;
    return released;
  }

  void reset(const int replacement = -1) noexcept
  {
    if (fd_ >= 0) {
      // Destructors cannot report close errors safely. Protocol errors are
      // checked at explicit read/write boundaries instead.
      (void)::close(fd_);
    }
    fd_ = replacement;
  }

private:
  int fd_{-1};
};

class MappedRegion
{
public:
  MappedRegion() noexcept = default;

  MappedRegion(void * const address, const std::size_t size) noexcept
  : address_(address), size_(size) {}

  ~MappedRegion()
  {
    reset();
  }

  MappedRegion(const MappedRegion &) = delete;
  MappedRegion & operator=(const MappedRegion &) = delete;

  MappedRegion(MappedRegion && other) noexcept
  : address_(other.address_), size_(other.size_)
  {
    other.address_ = MAP_FAILED;
    other.size_ = 0U;
  }

  MappedRegion & operator=(MappedRegion && other) noexcept
  {
    if (this != &other) {
      reset();
      address_ = other.address_;
      size_ = other.size_;
      other.address_ = MAP_FAILED;
      other.size_ = 0U;
    }
    return *this;
  }

  [[nodiscard]] void * get() const noexcept {return address_;}

  template<typename T>
  [[nodiscard]] T * as() const noexcept
  {
    return static_cast<T *>(address_);
  }

  void reset() noexcept
  {
    if (address_ != MAP_FAILED) {
      (void)::munmap(address_, size_);
      address_ = MAP_FAILED;
      size_ = 0U;
    }
  }

private:
  void * address_{MAP_FAILED};
  std::size_t size_{0U};
};

class Arguments
{
public:
  Arguments(const int argc, char ** argv)
  {
    for (int index = 1; index < argc; index += 2) {
      const std::string key{argv[index]};
      if (key.rfind("--", 0U) != 0U) {
        throw std::invalid_argument("expected --option, got: " + key);
      }
      if ((index + 1) >= argc) {
        throw std::invalid_argument("missing value for " + key);
      }
      const auto inserted = values_.emplace(key, std::string{argv[index + 1]});
      if (!inserted.second) {
        throw std::invalid_argument("duplicate option: " + key);
      }
    }
  }

  [[nodiscard]] std::string get_string(
    const std::string & key, const std::string & fallback) const
  {
    const auto found = values_.find(key);
    return found == values_.end() ? fallback : found->second;
  }

  [[nodiscard]] std::uint32_t get_u32(
    const std::string & key, const std::uint32_t fallback,
    const std::uint32_t minimum = 0U,
    const std::uint32_t maximum = std::numeric_limits<std::uint32_t>::max()) const
  {
    const auto found = values_.find(key);
    if (found == values_.end()) {
      return fallback;
    }

    std::size_t parsed_characters = 0U;
    unsigned long parsed = 0UL;
    try {
      parsed = std::stoul(found->second, &parsed_characters, 10);
    } catch (const std::exception &) {
      throw std::invalid_argument("invalid unsigned integer for " + key + ": " + found->second);
    }
    if (parsed_characters != found->second.size() ||
      parsed > static_cast<unsigned long>(maximum) ||
      parsed < static_cast<unsigned long>(minimum))
    {
      throw std::out_of_range("value out of range for " + key + ": " + found->second);
    }
    return static_cast<std::uint32_t>(parsed);
  }

  void ensure_only(std::initializer_list<const char *> known_options) const
  {
    for (const auto & entry : values_) {
      bool known = false;
      for (const char * const option : known_options) {
        if (entry.first == option) {
          known = true;
          break;
        }
      }
      if (!known) {
        throw std::invalid_argument("unknown option: " + entry.first);
      }
    }
  }

private:
  std::map<std::string, std::string> values_;
};

[[nodiscard]] inline std::uint64_t monotonic_now_ns()
{
  timespec timestamp{};
  if (::clock_gettime(CLOCK_MONOTONIC, &timestamp) != 0) {
    throw_errno("clock_gettime(CLOCK_MONOTONIC)");
  }
  return static_cast<std::uint64_t>(timestamp.tv_sec) * 1'000'000'000ULL +
         static_cast<std::uint64_t>(timestamp.tv_nsec);
}

inline void sleep_for_ms(const std::uint32_t milliseconds)
{
  std::this_thread::sleep_for(std::chrono::milliseconds{milliseconds});
}

[[nodiscard]] inline long linux_thread_id() noexcept
{
  return static_cast<long>(::syscall(SYS_gettid));
}

inline void write_all(const int fd, const std::uint8_t * data, const std::size_t size)
{
  std::size_t offset = 0U;
  while (offset < size) {
    const ssize_t result = ::write(fd, data + offset, size - offset);
    if (result > 0) {
      offset += static_cast<std::size_t>(result);
      continue;
    }
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result == 0) {
      throw std::runtime_error("write returned zero before buffer was complete");
    }
    throw_errno("write");
  }
}

inline void send_all(const int fd, const std::uint8_t * data, const std::size_t size)
{
  std::size_t offset = 0U;
  while (offset < size) {
    const ssize_t result = ::send(fd, data + offset, size - offset, MSG_NOSIGNAL);
    if (result > 0) {
      offset += static_cast<std::size_t>(result);
      continue;
    }
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result == 0) {
      throw std::runtime_error("send returned zero before buffer was complete");
    }
    throw_errno("send");
  }
}

inline void send_datagram(
  const int fd, const std::uint8_t * data, const std::size_t size,
  const sockaddr * const destination, const socklen_t destination_size)
{
  while (true) {
    const ssize_t result = ::sendto(
      fd, data, size, MSG_NOSIGNAL, destination, destination_size);
    if (result == static_cast<ssize_t>(size)) {
      return;
    }
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result >= 0) {
      throw std::runtime_error(
              "datagram send was unexpectedly partial: " +
              std::to_string(result) + " of " + std::to_string(size) + " bytes");
    }
    throw_errno("sendto");
  }
}

// MSG_TRUNC asks Linux to report the original datagram length, so an oversized
// packet cannot masquerade as one valid fixed-size protocol frame.
inline void receive_exact_datagram(
  const int fd, std::uint8_t * data, const std::size_t expected_size)
{
  while (true) {
    const ssize_t result = ::recv(fd, data, expected_size, MSG_TRUNC);
    if (result == static_cast<ssize_t>(expected_size)) {
      return;
    }
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result >= 0) {
      throw std::runtime_error(
              "datagram size mismatch: received " + std::to_string(result) +
              " bytes, expected " + std::to_string(expected_size));
    }
    throw_errno("recv");
  }
}

// Returns false only for clean EOF before any byte of the next frame.
inline bool read_exact(const int fd, std::uint8_t * data, const std::size_t size)
{
  std::size_t offset = 0U;
  while (offset < size) {
    const ssize_t result = ::read(fd, data + offset, size - offset);
    if (result > 0) {
      offset += static_cast<std::size_t>(result);
      continue;
    }
    if (result == 0) {
      if (offset == 0U) {
        return false;
      }
      throw std::runtime_error("unexpected EOF inside a framed message");
    }
    if (errno == EINTR) {
      continue;
    }
    throw_errno("read");
  }
  return true;
}

inline void set_receive_timeout(const int fd, const std::uint32_t timeout_ms)
{
  timeval timeout{};
  timeout.tv_sec = static_cast<time_t>(timeout_ms / 1000U);
  timeout.tv_usec = static_cast<suseconds_t>((timeout_ms % 1000U) * 1000U);
  if (::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) != 0) {
    throw_errno("setsockopt(SO_RCVTIMEO)");
  }
}

inline void wait_until_readable(const int fd, const std::uint32_t timeout_ms)
{
  pollfd descriptor{};
  descriptor.fd = fd;
  descriptor.events = POLLIN;
  while (true) {
    const int result = ::poll(
      &descriptor, 1U, static_cast<int>(timeout_ms));
    if (result > 0 && (descriptor.revents & POLLIN) != 0) {
      return;
    }
    if (result == 0) {
      throw std::runtime_error("timed out waiting for a readable file descriptor");
    }
    if (result < 0 && errno == EINTR) {
      continue;
    }
    if (result > 0) {
      throw std::runtime_error(
              "file descriptor became unusable while waiting; poll revents=" +
              std::to_string(descriptor.revents));
    }
    throw_errno("poll");
  }
}

template<typename Function>
int guarded_main(Function && function)
{
  try {
    std::forward<Function>(function)();
    return 0;
  } catch (const std::exception & error) {
    std::cerr << "error: " << error.what() << '\n';
    return 1;
  }
}

}  // namespace linux_ipc_lab
