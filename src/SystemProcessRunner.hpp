#pragma once

#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "ProcessRunner.h"

class SystemProcessRunner : public ProcessRunner
{
 public:
  int run(const std::string& command, const std::vector<std::string>& args,
          const std::string& workingDir) override
  {
    namespace fs = std::filesystem;

    fs::path const oldCwd = fs::current_path();
    fs::current_path(workingDir);

    std::ostringstream oss;
    oss << command;
    for (auto& arg : args)
    {
      oss << " " << arg;
    }

    int const ret = std::system(oss.str().c_str());

    fs::current_path(oldCwd);

    return ret;
  }
};