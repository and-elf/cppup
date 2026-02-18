export module cppup.cli.logger;

#include <string>
#include <print>

export namespace cppup::cli {

/**
 * Simple logger interface
 */
export class Logger {
public:
    virtual ~Logger() = default;

    virtual void info(const std::string& message) = 0;
    virtual void warning(const std::string& message) = 0;
    virtual void error(const std::string& message) = 0;
    virtual void debug(const std::string& message) = 0;
};

/**
 * Console logger implementation
 */
export class ConsoleLogger : public Logger {
public:
    void info(const std::string& message) override;
    void warning(const std::string& message) override;
    void error(const std::string& message) override;
    void debug(const std::string& message) override;
};

// Implementation

void ConsoleLogger::info(const std::string& message)
{
  std::print("[INFO] {}\n", message);
}

void ConsoleLogger::warning(const std::string& message)
{
  std::print("[WARN] {}\n", message);
}

void ConsoleLogger::error(const std::string& message)
{
  std::print("[ERROR] {}\n", message);
}

void ConsoleLogger::debug(const std::string& message)
{
  std::print("[DEBUG] {}\n", message);
}

} // namespace cppup::cli