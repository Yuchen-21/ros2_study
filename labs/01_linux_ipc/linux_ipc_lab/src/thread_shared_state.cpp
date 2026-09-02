#include "linux_ipc_lab/system.hpp"

#include <cstdint>
#include <iostream>
#include <mutex>
#include <thread>
#include <vector>

#include <unistd.h>

namespace
{

struct SharedState
{
  std::mutex mutex;
  std::uint64_t counter{0U};
};

void run_worker(
  const char * const name, SharedState & state, std::mutex & output_mutex,
  const std::uint32_t iterations, const std::uint32_t hold_ms)
{
  {
    std::lock_guard<std::mutex> output_lock{output_mutex};
    std::cout << "thread name=" << name
              << " pid=" << ::getpid()
              << " tid=" << linux_ipc_lab::linux_thread_id()
              << " object_address=" << static_cast<const void *>(&state.counter)
              << '\n';
  }

  // Holding inside each worker leaves both kernel threads visible to ps -L.
  linux_ipc_lab::sleep_for_ms(hold_ms);

  for (std::uint32_t index = 0U; index < iterations; ++index) {
    std::lock_guard<std::mutex> state_lock{state.mutex};
    ++state.counter;
  }
}

}  // namespace

int main(int argc, char ** argv)
{
  return linux_ipc_lab::guarded_main(
    [&]() {
      const linux_ipc_lab::Arguments arguments{argc, argv};
      arguments.ensure_only({"--iterations", "--hold-ms"});
      const std::uint32_t iterations =
        arguments.get_u32("--iterations", 100'000U, 1U, 10'000'000U);
      const std::uint32_t hold_ms =
        arguments.get_u32("--hold-ms", 0U, 0U, 60'000U);

      SharedState state;
      std::mutex output_mutex;
      std::vector<std::thread> workers;
      workers.emplace_back(
        run_worker, "A", std::ref(state), std::ref(output_mutex), iterations, hold_ms);
      workers.emplace_back(
        run_worker, "B", std::ref(state), std::ref(output_mutex), iterations, hold_ms);

      for (auto & worker : workers) {
        worker.join();
      }

      const std::uint64_t expected =
        static_cast<std::uint64_t>(iterations) *
        static_cast<std::uint64_t>(workers.size());
      std::cout << "final_counter=" << state.counter
                << " expected=" << expected
                << " process_pid=" << ::getpid() << '\n';
      if (state.counter != expected) {
        throw std::runtime_error("mutex-protected counter did not reach its invariant");
      }
    });
}
