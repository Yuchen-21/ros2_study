#include "linux_ipc_lab/system.hpp"
#include "linux_ipc_lab/wire_message.hpp"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace
{

[[nodiscard]] std::string default_child_path(const char * const argv_zero)
{
  const std::filesystem::path own_path{argv_zero};
  const std::filesystem::path parent = own_path.has_parent_path() ?
    own_path.parent_path() : std::filesystem::path{"."};
  return (parent / "pipe_process_b").string();
}

}  // namespace

int main(int argc, char ** argv)
{
  return linux_ipc_lab::guarded_main(
    [&]() {
      const linux_ipc_lab::Arguments arguments{argc, argv};
      arguments.ensure_only({"--count", "--interval-ms", "--child"});
      const std::uint32_t count = arguments.get_u32("--count", 5U, 1U, 100'000U);
      const std::uint32_t interval_ms =
        arguments.get_u32("--interval-ms", 20U, 0U, 60'000U);
      const std::string child_path =
        arguments.get_string("--child", default_child_path(argv[0]));

      // Report EPIPE as a normal exception instead of terminating before cleanup.
      if (::signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
        linux_ipc_lab::throw_errno("signal(SIGPIPE)");
      }

      int raw_pipe[2]{-1, -1};
      if (::pipe2(raw_pipe, O_CLOEXEC) != 0) {
        linux_ipc_lab::throw_errno("pipe2");
      }
      linux_ipc_lab::UniqueFd read_end{raw_pipe[0]};
      linux_ipc_lab::UniqueFd write_end{raw_pipe[1]};

      const pid_t child_pid = ::fork();
      if (child_pid < 0) {
        linux_ipc_lab::throw_errno("fork");
      }

      if (child_pid == 0) {
        (void)::close(write_end.get());
        const int descriptor_flags = ::fcntl(read_end.get(), F_GETFD);
        if (descriptor_flags < 0 ||
          ::fcntl(read_end.get(), F_SETFD, descriptor_flags & ~FD_CLOEXEC) != 0)
        {
          (void)::dprintf(
            STDERR_FILENO, "pipe child: failed to clear FD_CLOEXEC: %s\n",
            std::strerror(errno));
          ::_exit(126);
        }

        const std::string fd_text = std::to_string(read_end.get());
        ::execl(
          child_path.c_str(), child_path.c_str(),
          "--fd", fd_text.c_str(), static_cast<char *>(nullptr));
        (void)::dprintf(
          STDERR_FILENO, "pipe child: exec %s failed: %s\n",
          child_path.c_str(), std::strerror(errno));
        ::_exit(127);
      }

      read_end.reset();
      std::cout << "pipe_process_a pid=" << ::getpid()
                << " child_pid=" << child_pid
                << " write_fd=" << write_end.get()
                << " count=" << count << '\n';

      for (std::uint32_t sequence = 0U; sequence < count; ++sequence) {
        const auto bytes = linux_ipc_lab::encode_sample(
          linux_ipc_lab::make_sample(sequence));
        linux_ipc_lab::write_all(write_end.get(), bytes.data(), bytes.size());
        linux_ipc_lab::sleep_for_ms(interval_ms);
      }

      // EOF is a lifetime signal: it becomes visible only after every writer closes.
      write_end.reset();

      int child_status = 0;
      while (::waitpid(child_pid, &child_status, 0) < 0) {
        if (errno == EINTR) {
          continue;
        }
        linux_ipc_lab::throw_errno("waitpid");
      }
      if (!WIFEXITED(child_status)) {
        throw std::runtime_error("pipe_process_b terminated abnormally");
      }
      const int exit_code = WEXITSTATUS(child_status);
      std::cout << "pipe_process_a child_exit=" << exit_code << '\n';
      if (exit_code != 0) {
        throw std::runtime_error("pipe_process_b reported failure");
      }
    });
}
