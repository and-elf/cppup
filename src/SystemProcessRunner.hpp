#pragma once

#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <string>
#include <vector>

#include "ProcessRunner.h"

struct SystemProcessRunner : ProcessRunner
{
  int run(const ProcessRunRequest& request) override
  {
    const auto result = run_impl(request, false);
    return result.exit_code;
  }

  ProcessCaptureResult run_capture(const ProcessRunRequest& request) override
  {
    return run_impl(request, true);
  }

 private:
  static void close_fd(int file_descriptor)
  {
    if (file_descriptor >= 0)
    {
      ::close(file_descriptor);
    }
  }

  static bool setup_capture_pipe(bool capture_output, std::array<int, 2>& pipefds)
  {
    if (!capture_output)
    {
      return true;
    }
    return ::pipe(pipefds.data()) == 0;
  }

  static bool setup_child_output_capture(bool capture_output, const std::array<int, 2>& pipefds)
  {
    if (!capture_output)
    {
      return true;
    }
    close_fd(pipefds[0]);
    if (::dup2(pipefds[1], STDOUT_FILENO) < 0 || ::dup2(pipefds[1], STDERR_FILENO) < 0)
    {
      return false;
    }
    close_fd(pipefds[1]);
    return true;
  }

  static std::string read_captured_output(bool capture_output, const std::array<int, 2>& pipefds)
  {
    if (!capture_output)
    {
      return {};
    }

    close_fd(pipefds[1]);
    std::string            output;
    std::array<char, 4096> buffer{};
    while (true)
    {
      const ssize_t bytes_read = ::read(pipefds[0], buffer.data(), buffer.size());
      if (bytes_read > 0)
      {
        output.append(buffer.data(), static_cast<std::size_t>(bytes_read));
        continue;
      }
      if (bytes_read == 0)
      {
        break;
      }
      if (errno != EINTR)
      {
        break;
      }
    }
    close_fd(pipefds[0]);
    return output;
  }

  static int wait_for_exit_code(pid_t pid)
  {
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0)
    {
      if (errno != EINTR)
      {
        return -1;
      }
    }

    if (WIFEXITED(status))
    {
      return WEXITSTATUS(status);
    }
    if (WIFSIGNALED(status))
    {
      return 128 + WTERMSIG(status);
    }
    return -1;
  }

  static ProcessCaptureResult run_impl(const ProcessRunRequest& request, bool capture_output)
  {
    std::vector<std::string> argv_storage;
    argv_storage.reserve(request.args.size() + 1);
    argv_storage.push_back(request.command);
    for (const auto& arg : request.args)
    {
      argv_storage.push_back(arg);
    }

    std::vector<char*> argv;
    argv.reserve(argv_storage.size() + 1);
    for (auto& arg : argv_storage)
    {
      argv.push_back(arg.data());
    }
    argv.push_back(nullptr);

    std::array<int, 2> capture_pipe{-1, -1};
    if (!setup_capture_pipe(capture_output, capture_pipe))
    {
      return {.exit_code = -1, .output = {}};
    }

    const pid_t pid = ::fork();
    if (pid < 0)
    {
      close_fd(capture_pipe[0]);
      close_fd(capture_pipe[1]);
      return {.exit_code = -1, .output = {}};
    }

    if (pid == 0)
    {
      if (!setup_child_output_capture(capture_output, capture_pipe))
      {
        ::_exit(126);
      }

      if (!request.working_dir.empty() && ::chdir(request.working_dir.c_str()) != 0)
      {
        ::_exit(126);
      }

      ::execvp(request.command.c_str(), argv.data());
      ::_exit(errno == ENOENT ? 127 : 126);
    }

    ProcessCaptureResult result;
    result.output    = read_captured_output(capture_output, capture_pipe);
    result.exit_code = wait_for_exit_code(pid);
    return result;
  }
};