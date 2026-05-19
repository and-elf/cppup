#pragma once

#include <string>
#include <vector>

class ProcessRunner
{
 public:
  ProcessRunner()                                = default;
  virtual ~ProcessRunner()                       = default;
  ProcessRunner(const ProcessRunner&)            = delete;
  ProcessRunner& operator=(const ProcessRunner&) = delete;
  ProcessRunner(ProcessRunner&&)                 = delete;
  ProcessRunner& operator=(ProcessRunner&&)      = delete;

  virtual int run(const std::string& command, const std::vector<std::string>& args,
                  const std::string& working_dir) = 0;
};