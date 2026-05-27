#pragma once

#ifdef _WIN32

// Keep <windows.h> as lean as we can — the runner only needs process,
// pipe, and handle APIs. min/max defines from <windows.h> would otherwise
// collide with std::min / std::max elsewhere in the codebase.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <array>
#include <cstddef>
#include <string>
#include <vector>

#include "ProcessRunner.h"

struct SystemProcessRunner : ProcessRunner
{
  int run(const ProcessRunRequest& request) override
  {
    return run_impl(request, false).exit_code;
  }

  ProcessCaptureResult run_capture(const ProcessRunRequest& request) override
  {
    return run_impl(request, true);
  }

 private:
  // CRT-compatible argv quoting. CreateProcess hands the child a single
  // command-line string and standard CRT startup parses it back into argv
  // using these rules — see MSDN "Parsing C++ Command-Line Arguments".
  static void append_quoted(std::string& cmd, const std::string& arg)
  {
    const bool needs_quote = arg.empty() || arg.find_first_of(" \t\n\v\"") != std::string::npos;
    if (!needs_quote)
    {
      cmd.append(arg);
      return;
    }
    cmd.push_back('"');
    std::size_t backslashes = 0;
    for (const char character : arg)
    {
      if (character == '\\')
      {
        ++backslashes;
        continue;
      }
      if (character == '"')
      {
        cmd.append((2 * backslashes) + 1, '\\');
        cmd.push_back('"');
        backslashes = 0;
        continue;
      }
      cmd.append(backslashes, '\\');
      cmd.push_back(character);
      backslashes = 0;
    }
    cmd.append(2 * backslashes, '\\');
    cmd.push_back('"');
  }

  static std::string build_command_line(const ProcessRunRequest& request)
  {
    std::string cmd;
    append_quoted(cmd, request.command);
    for (const auto& arg : request.args)
    {
      cmd.push_back(' ');
      append_quoted(cmd, arg);
    }
    return cmd;
  }

  static ProcessCaptureResult run_impl(const ProcessRunRequest& request, bool capture_output)
  {
    HANDLE read_end  = nullptr;
    HANDLE write_end = nullptr;

    if (capture_output)
    {
      SECURITY_ATTRIBUTES sa{};
      sa.nLength        = sizeof(sa);
      sa.bInheritHandle = TRUE;
      if (CreatePipe(&read_end, &write_end, &sa, 0) == 0)
      {
        return {.exit_code = -1, .output = {}};
      }
      // The read end stays in the parent; mark it non-inheritable so the
      // child can't keep it open and stall our ReadFile loop.
      SetHandleInformation(read_end, HANDLE_FLAG_INHERIT, 0);
    }

    STARTUPINFOA startup_info{};
    startup_info.cb         = sizeof(startup_info);
    startup_info.dwFlags    = STARTF_USESTDHANDLES;
    startup_info.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = capture_output ? write_end : GetStdHandle(STD_OUTPUT_HANDLE);
    startup_info.hStdError  = capture_output ? write_end : GetStdHandle(STD_ERROR_HANDLE);

    PROCESS_INFORMATION process_info{};

    // CreateProcessA mutates its lpCommandLine, so hand it a writable copy.
    std::string       command_line = build_command_line(request);
    std::vector<char> mutable_cmd(command_line.begin(), command_line.end());
    mutable_cmd.push_back('\0');

    const char* cwd = request.working_dir.empty() ? nullptr : request.working_dir.c_str();

    const BOOL ok = CreateProcessA(nullptr, mutable_cmd.data(), nullptr, nullptr, TRUE, 0, nullptr,
                                   cwd, &startup_info, &process_info);

    // Parent doesn't write to the pipe — close our copy of the write end
    // so the child holds the only one and ReadFile sees EOF on child exit.
    if (capture_output && write_end != nullptr)
    {
      CloseHandle(write_end);
    }

    if (ok == 0)
    {
      const DWORD err = GetLastError();
      if (read_end != nullptr)
      {
        CloseHandle(read_end);
      }
      // Map "image not found" to 127 to mirror POSIX execvp / shell semantics.
      const int exit_code = (err == ERROR_FILE_NOT_FOUND) ? 127 : -1;
      return {.exit_code = exit_code, .output = {}};
    }

    std::string output;
    if (capture_output)
    {
      std::array<char, 4096> buffer{};
      DWORD                  bytes_read = 0;
      while (ReadFile(read_end, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read,
                      nullptr) != 0)
      {
        if (bytes_read == 0)
        {
          break;
        }
        output.append(buffer.data(), bytes_read);
      }
      CloseHandle(read_end);
    }

    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD exit_code = 0;
    if (GetExitCodeProcess(process_info.hProcess, &exit_code) == 0)
    {
      exit_code = static_cast<DWORD>(-1);
    }
    CloseHandle(process_info.hProcess);
    CloseHandle(process_info.hThread);

    return {.exit_code = static_cast<int>(exit_code), .output = std::move(output)};
  }
};

#else  // POSIX

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

#endif  // _WIN32
