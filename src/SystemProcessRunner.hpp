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
          const std::string& working_dir) override
  {
    namespace fs = std::filesystem;

    fs::path const old_cwd = fs::current_path();
    fs::current_path(working_dir);

    std::ostringstream oss;
    oss << command;
    for (const auto& arg : args)
    {
      oss << " " << arg;
    }

    int const ret = std::system(oss.str().c_str());

    fs::current_path(old_cwd);

    return ret;
  }
};