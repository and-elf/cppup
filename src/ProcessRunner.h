#pragma once

#include <string>
#include <vector>

struct ProcessRunRequest
{
  std::string              command;
  std::vector<std::string> args;
  std::string              working_dir;
};

struct ProcessCaptureResult
{
  int         exit_code = -1;
  std::string output;
};

class ProcessRunner
{
 public:
  ProcessRunner()                                = default;
  virtual ~ProcessRunner()                       = default;
  ProcessRunner(const ProcessRunner&)            = delete;
  ProcessRunner& operator=(const ProcessRunner&) = delete;
  ProcessRunner(ProcessRunner&&)                 = delete;
  ProcessRunner& operator=(ProcessRunner&&)      = delete;

  virtual int                  run(const ProcessRunRequest& request)         = 0;
  virtual ProcessCaptureResult run_capture(const ProcessRunRequest& request) = 0;
};