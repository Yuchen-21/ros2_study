#pragma once

#include "linux_ipc_lab/system.hpp"
#include "linux_ipc_lab/wire_message.hpp"

#include <cctype>
#include <cerrno>
#include <cstdint>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>

#include <pthread.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <time.h>

namespace linux_ipc_lab
{

constexpr std::uint32_t kSharedRegionMagic = 0x5253484dU;  // ASCII "RSHM".
constexpr std::uint16_t kSharedRegionVersion = 1U;

struct alignas(64) SharedRegion
{
  std::uint32_t magic{};
  std::uint16_t version{};
  std::uint16_t reserved{};
  pthread_mutex_t mutex{};
  pthread_cond_t updated{};
  std::uint8_t reader_ready{};
  std::uint8_t shutdown{};
  std::uint8_t reserved_flags[6]{};
  std::uint64_t generation{};
  std::uint64_t consumed_generation{};
  SensorSample sample{};
};

[[nodiscard]] inline std::string default_shared_memory_name()
{
  return "/ros2_comm_lab_sensor_" +
         std::to_string(static_cast<unsigned long>(::getuid()));
}

inline void validate_shared_memory_name(const std::string & name)
{
  if (name.size() < 2U || name.front() != '/') {
    throw std::invalid_argument("POSIX shared-memory name must start with one '/'");
  }
  for (std::size_t index = 1U; index < name.size(); ++index) {
    const unsigned char character = static_cast<unsigned char>(name[index]);
    if (!(std::isalnum(character) != 0 || character == '_' || character == '-')) {
      throw std::invalid_argument(
              "shared-memory name may contain only [A-Za-z0-9_-] after the leading '/'");
    }
  }
}

class FileLock
{
public:
  FileLock(const int fd, const int operation) : fd_(fd)
  {
    while (::flock(fd_, operation) != 0) {
      if (errno == EINTR) {
        continue;
      }
      throw_errno("flock");
    }
  }

  ~FileLock()
  {
    (void)::flock(fd_, LOCK_UN);
  }

  FileLock(const FileLock &) = delete;
  FileLock & operator=(const FileLock &) = delete;

private:
  int fd_;
};

class SharedMemoryNameOwner
{
public:
  explicit SharedMemoryNameOwner(std::string name) : name_(std::move(name)) {}

  ~SharedMemoryNameOwner()
  {
    if (owns_name_) {
      (void)::shm_unlink(name_.c_str());
    }
  }

  SharedMemoryNameOwner(const SharedMemoryNameOwner &) = delete;
  SharedMemoryNameOwner & operator=(const SharedMemoryNameOwner &) = delete;

  void mark_created() noexcept {owns_name_ = true;}

  void unlink_now()
  {
    if (owns_name_ && ::shm_unlink(name_.c_str()) != 0) {
      throw_errno("shm_unlink(" + name_ + ")");
    }
    owns_name_ = false;
  }

private:
  std::string name_;
  bool owns_name_{false};
};

class PthreadLock
{
public:
  explicit PthreadLock(pthread_mutex_t & mutex) : mutex_(&mutex)
  {
    const int result = ::pthread_mutex_lock(mutex_);
    if (result == EOWNERDEAD) {
      // The protected protocol state cannot be trusted after its owner died.
      // Make the robust mutex usable again, release it, then fail explicitly.
      (void)::pthread_mutex_consistent(mutex_);
      (void)::pthread_mutex_unlock(mutex_);
      mutex_ = nullptr;
      throw std::runtime_error("shared-memory mutex owner died; region state is inconsistent");
    }
    check_pthread(result, "pthread_mutex_lock");
  }

  ~PthreadLock()
  {
    if (mutex_ != nullptr) {
      (void)::pthread_mutex_unlock(mutex_);
    }
  }

  PthreadLock(const PthreadLock &) = delete;
  PthreadLock & operator=(const PthreadLock &) = delete;

  [[nodiscard]] pthread_mutex_t * native_handle() const noexcept {return mutex_;}

private:
  pthread_mutex_t * mutex_;
};

inline void initialize_shared_region(SharedRegion * const region)
{
  (void)new (region) SharedRegion{};

  pthread_mutexattr_t mutex_attributes{};
  check_pthread(::pthread_mutexattr_init(&mutex_attributes), "pthread_mutexattr_init");
  bool mutex_initialized = false;
  try {
    check_pthread(
      ::pthread_mutexattr_setpshared(&mutex_attributes, PTHREAD_PROCESS_SHARED),
      "pthread_mutexattr_setpshared");
    check_pthread(
      ::pthread_mutexattr_setrobust(&mutex_attributes, PTHREAD_MUTEX_ROBUST),
      "pthread_mutexattr_setrobust");
    check_pthread(
      ::pthread_mutex_init(&region->mutex, &mutex_attributes),
      "pthread_mutex_init");
    mutex_initialized = true;
  } catch (...) {
    (void)::pthread_mutexattr_destroy(&mutex_attributes);
    throw;
  }
  (void)::pthread_mutexattr_destroy(&mutex_attributes);

  pthread_condattr_t condition_attributes{};
  bool condition_attributes_initialized = false;
  try {
    check_pthread(::pthread_condattr_init(&condition_attributes), "pthread_condattr_init");
    condition_attributes_initialized = true;
    check_pthread(
      ::pthread_condattr_setpshared(&condition_attributes, PTHREAD_PROCESS_SHARED),
      "pthread_condattr_setpshared");
    check_pthread(
      ::pthread_condattr_setclock(&condition_attributes, CLOCK_MONOTONIC),
      "pthread_condattr_setclock");
    check_pthread(
      ::pthread_cond_init(&region->updated, &condition_attributes),
      "pthread_cond_init");
  } catch (...) {
    if (condition_attributes_initialized) {
      (void)::pthread_condattr_destroy(&condition_attributes);
    }
    if (mutex_initialized) {
      (void)::pthread_mutex_destroy(&region->mutex);
    }
    throw;
  }
  (void)::pthread_condattr_destroy(&condition_attributes);

  region->version = kSharedRegionVersion;
  // Set magic last: it is the commit marker after every field and pthread
  // primitive has been initialized under the file initialization lock.
  region->magic = kSharedRegionMagic;
}

inline void validate_shared_region(const SharedRegion & region)
{
  if (region.magic != kSharedRegionMagic) {
    throw std::runtime_error("shared-memory region magic is invalid or initialization is incomplete");
  }
  if (region.version != kSharedRegionVersion) {
    throw std::runtime_error("shared-memory region version is unsupported");
  }
}

[[nodiscard]] inline timespec monotonic_deadline_after(const std::uint32_t timeout_ms)
{
  timespec deadline{};
  if (::clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
    throw_errno("clock_gettime(CLOCK_MONOTONIC)");
  }
  const std::uint64_t nanoseconds =
    static_cast<std::uint64_t>(deadline.tv_nsec) +
    static_cast<std::uint64_t>(timeout_ms % 1000U) * 1'000'000ULL;
  deadline.tv_sec += static_cast<time_t>(
    timeout_ms / 1000U + nanoseconds / 1'000'000'000ULL);
  deadline.tv_nsec = static_cast<long>(nanoseconds % 1'000'000'000ULL);
  return deadline;
}

// Returns false on timeout. The caller must always re-check its predicate.
inline bool wait_until(
  pthread_cond_t & condition, PthreadLock & lock, const timespec & deadline)
{
  const int result = ::pthread_cond_timedwait(
    &condition, lock.native_handle(), &deadline);
  if (result == 0) {
    return true;
  }
  if (result == ETIMEDOUT) {
    return false;
  }
  if (result == EOWNERDEAD) {
    (void)::pthread_mutex_consistent(lock.native_handle());
    throw std::runtime_error("shared-memory mutex owner died while waiting");
  }
  check_pthread(result, "pthread_cond_timedwait");
  return false;
}

inline void notify_all(pthread_cond_t & condition)
{
  check_pthread(::pthread_cond_broadcast(&condition), "pthread_cond_broadcast");
}

[[nodiscard]] inline MappedRegion map_shared_region(const int fd)
{
  void * const address = ::mmap(
    nullptr, sizeof(SharedRegion), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (address == MAP_FAILED) {
    throw_errno("mmap(shared region)");
  }
  return MappedRegion{address, sizeof(SharedRegion)};
}

}  // namespace linux_ipc_lab
