#include "linux_ipc_lab/system.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char ** argv)
{
  return linux_ipc_lab::guarded_main(
    [&]() {
      const linux_ipc_lab::Arguments arguments{argc, argv};
      arguments.ensure_only({"--hold-ms"});
      const std::uint32_t hold_ms =
        arguments.get_u32("--hold-ms", 0U, 0U, 60'000U);

      int process_local_value = 42;
      std::cout << "before_fork pid=" << ::getpid()
                << " value=" << process_local_value
                << " address=" << static_cast<const void *>(&process_local_value)
                << std::endl;

      const pid_t child_pid = ::fork();
      if (child_pid < 0) {
        linux_ipc_lab::throw_errno("fork");
      }

      if (child_pid == 0) {
        linux_ipc_lab::sleep_for_ms(hold_ms);
        process_local_value = 99;
        std::cout << "child pid=" << ::getpid()
                  << " parent_pid=" << ::getppid()
                  << " changed_value=" << process_local_value
                  << " address=" << static_cast<const void *>(&process_local_value)
                  << std::endl;
        // Do not run copied parent-side C++ cleanup after fork in this small child.
        ::_exit(EXIT_SUCCESS);
      }

      linux_ipc_lab::sleep_for_ms(hold_ms);
      int child_status = 0;
      while (::waitpid(child_pid, &child_status, 0) < 0) {
        if (errno == EINTR) {
          continue;
        }
        linux_ipc_lab::throw_errno("waitpid");
      }

      if (!WIFEXITED(child_status) || WEXITSTATUS(child_status) != EXIT_SUCCESS) {
        throw std::runtime_error("child process did not exit successfully");
      }

      std::cout << "parent pid=" << ::getpid()
                << " child_pid=" << child_pid
                << " value_after_child_exit=" << process_local_value
                << " address=" << static_cast<const void *>(&process_local_value)
                << '\n';
      if (process_local_value != 42) {
        throw std::runtime_error("parent value unexpectedly changed across process boundary");
      }
    });
}
