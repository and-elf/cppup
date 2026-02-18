#include "package_concept.hpp"
#include <algorithm>

namespace cppup::package {

namespace utils {

std::expected<void, std::string> execute_command(
    const CommandExecutor& executor,
    const std::string& command, 
    const std::filesystem::path& working_dir
) {
    return executor.execute(command, working_dir);
}

std::expected<std::string, std::string> execute_command_with_output(
    const CommandExecutor& executor,
    const std::string& command, 
    const std::filesystem::path& working_dir
) {
    return executor.execute_with_output(command, working_dir);
}

std::filesystem::path get_actual_source_path(
    const std::filesystem::path& source_path, 
    const cppup::configuration::PackageInfo& info
) {
    if (info.subdirectory.has_value()) {
        return source_path / info.subdirectory.value();
    }
    return source_path;
}

bool download_file(
    const CommandExecutor& executor,
    const std::string& url, 
    const std::filesystem::path& destination
) {
    // Try curl first
    auto curl_result = execute_command_with_output(executor, "curl --version", destination.parent_path());
    if (curl_result && curl_result.value().find("curl") != std::string::npos) {
        std::string download_command = "curl -L -o \"" + destination.string() + "\" \"" + url + "\"";
        auto result = execute_command(executor, download_command, destination.parent_path());
        return result.has_value() && std::filesystem::exists(destination);
    }
    
    // Try wget
    auto wget_result = execute_command_with_output(executor, "wget --version", destination.parent_path());
    if (wget_result && wget_result.value().find("wget") != std::string::npos) {
        std::string download_command = "wget -O \"" + destination.string() + "\" \"" + url + "\"";
        auto result = execute_command(executor, download_command, destination.parent_path());
        return result.has_value() && std::filesystem::exists(destination);
    }
    
    return false; // Neither curl nor wget available
}

bool extract_archive(
    const CommandExecutor& executor,
    const std::filesystem::path& archive_path, 
    const std::filesystem::path& destination
) {
    std::filesystem::create_directories(destination);
    
    std::string extension = archive_path.extension().string();
    std::string extract_command;
    
    if (extension == ".tar" || extension == ".gz" || extension == ".tgz") {
        extract_command = "tar -xzf \"" + archive_path.string() + "\" -C \"" + destination.string() + "\"";
    } else if (extension == ".zip") {
        extract_command = "unzip \"" + archive_path.string() + "\" -d \"" + destination.string() + "\"";
    } else if (extension == ".7z") {
        extract_command = "7z x \"" + archive_path.string() + "\" -o\"" + destination.string() + "\"";
    } else {
        return false; // Unsupported archive format
    }
    
    auto result = execute_command(executor, extract_command, destination.parent_path());
    return result.has_value();
}

} // namespace utils

// PackageCacheInterface is pure virtual - no implementation needed here

} // namespace cppup::package