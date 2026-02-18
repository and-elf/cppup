#include <string>
#include <tuple>
#include <vector>

import cppup.process_runner;

class MockProcessRunner : public ProcessRunner
{
 public:
  struct Call
  {
    std::string              command;
    std::vector<std::string> args;
    std::string              workingDir;
  };

  std::vector<Call> calls;
  int               returnCode = 0;

  int run(const std::string& command, const std::vector<std::string>& args,
          const std::string& workingDir) override
  {
    calls.push_back({command, args, workingDir});
    return returnCode;
  }
};
