export module cppup.process_runner;

#include <string>
#include <vector>

export class ProcessRunner
{
 public:
  ProcessRunner()          = default;
  virtual ~ProcessRunner() = default;

  virtual int run(const std::string& command, const std::vector<std::string>& args,
                  const std::string& workingDir) = 0;
};