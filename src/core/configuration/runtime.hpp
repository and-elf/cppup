#pragma once

#include "build_configuration.hpp"
#include <string>
#include <optional>

namespace cppup::configuration {

// Runtime feature and environment queries
[[nodiscard]] inline bool has_feature(const BuildConfiguration& config, const std::string& feature) noexcept {
    return config.features.contains(feature);
}

[[nodiscard]] inline std::optional<std::string> get_env(const BuildConfiguration& config, const std::string& var) noexcept {
    auto it = config.environment.find(var);
    return it != config.environment.end() ? std::make_optional(it->second) : std::nullopt;
}

// Runtime conditional helpers for features
template<typename Func>
void when_feature(const BuildConfiguration& config, const std::string& feature, Func&& func) {
    if (has_feature(config, feature)) {
        func();
    }
}

template<typename Func>
void when_env(const BuildConfiguration& config, const std::string& var, const std::string& value, Func&& func) {
    if (auto env_val = get_env(config, var); env_val && *env_val == value) {
        func();
    }
}

// Overload for when_env that just checks if the environment variable exists (regardless of value)
template<typename Func>
void when_env_exists(const BuildConfiguration& config, const std::string& var, Func&& func) {
    if (get_env(config, var).has_value()) {
        func();
    }
}

// Helper to get environment variable with default value
[[nodiscard]] inline std::string get_env_or(const BuildConfiguration& config, const std::string& var, const std::string& default_value) noexcept {
    return get_env(config, var).value_or(default_value);
}

// Helper to check if multiple features are present
[[nodiscard]] inline bool has_all_features(const BuildConfiguration& config, const std::vector<std::string>& features) noexcept {
    for (const auto& feature : features) {
        if (!has_feature(config, feature)) {
            return false;
        }
    }
    return true;
}

// Helper to check if any of the features are present
[[nodiscard]] inline bool has_any_feature(const BuildConfiguration& config, const std::vector<std::string>& features) noexcept {
    for (const auto& feature : features) {
        if (has_feature(config, feature)) {
            return true;
        }
    }
    return false;
}

} // namespace cppup::configuration