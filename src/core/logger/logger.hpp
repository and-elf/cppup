#include "logging.hpp"
#include <fstream>

LogConfig Logger::globalConfig_;
std::ostream* Logger::fileStream_ = nullptr;
std::mutex Logger::logMutex_;

Logger::Logger(std::string category)
    : category_(std::move(category)) {}

void Logger::setGlobalConfig(LogConfig config) {
    globalConfig_ = std::move(config);
}

void Logger::setLogFile(std::string path) {
    static std::ofstream file(path, std::ios::app);
    fileStream_ = &file;
}

void Logger::log(LogLevel level, std::string_view message) const {
    std::lock_guard<std::mutex> lock(logMutex_);

    LogLevel minLevel = globalConfig_.defaultLevel;
    if (auto it = globalConfig_.categoryOverrides.find(category_);
        it != globalConfig_.categoryOverrides.end()) {
        minLevel = it->second;
    }

    if (level < minLevel) return;

    auto& out = (level >= LogLevel::Warning) ? std::cerr : std::cout;
    out << levelToColor(level)
        << "[" << category_ << "] "
        << levelToString(level)
        << "\033[0m: " << message << "\n";

    if (fileStream_) {
        (*fileStream_) << "[" << category_ << "] "
                       << levelToString(level) << ": "
                       << message << "\n";
    }
}

std::string_view Logger::levelToString(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Debug:   return "DEBUG";
        case LogLevel::Info:    return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error:   return "ERROR";
    }
    return "UNKNOWN";
}

const char* Logger::levelToColor(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::Debug:   return "\033[36m"; // Cyan
        case LogLevel::Info:    return "\033[32m"; // Green
        case LogLevel::Warning: return "\033[33m"; // Yellow
        case LogLevel::Error:   return "\033[31m"; // Red
    }
    return "\033[0m";
}
