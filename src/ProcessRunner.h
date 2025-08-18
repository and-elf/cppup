#include <string>
#include <vector>

class ProcessRunner {
public:
    virtual ProcessRunner() = default;

    virtual int run(
        const std::string& command,
        const std::vector<std::string>& args,
        const std::string& workingDir
    ) = 0;
};