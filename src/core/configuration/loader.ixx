export module cppup.configuration.loader;

#include <string>
#include <memory>
#include <optional>
#include <filesystem>
#include <iostream>
#include <concepts>

#ifdef _WIN32
    #include <windows.hpp>
#else
    #include <dlfcn.h>
#endif

import cppup.configuration.build_configuration;
import cppup.configuration.compiler;

export namespace cppup::configuration {

/**
 * Result of loading a configuration
 */
export struct LoadResult {
    bool success = false;
    std::optional<BuildConfiguration> configuration;
    std::string error_message;

    // Helper methods
    [[nodiscard]] bool is_success() const noexcept { return success; }
    [[nodiscard]] bool is_failure() const noexcept { return !success; }
    [[nodiscard]] bool has_configuration() const noexcept { return configuration.has_value(); }
};

/**
 * Handle to a loaded shared library
 * Uses RAII to automatically unload the library when destroyed
 */
export class SharedLibraryHandle {
public:
    SharedLibraryHandle() = default;
    explicit SharedLibraryHandle(void* handle) : handle_(handle) {}

    // Move-only type
    SharedLibraryHandle(const SharedLibraryHandle&) = delete;
    SharedLibraryHandle& operator=(const SharedLibraryHandle&) = delete;

    SharedLibraryHandle(SharedLibraryHandle&& other) noexcept
        : handle_(other.handle_) {
        other.handle_ = nullptr;
    }

    SharedLibraryHandle& operator=(SharedLibraryHandle&& other) noexcept {
        if (this != &other) {
            close();
            handle_ = other.handle_;
            other.handle_ = nullptr;
        }
        return *this;
    }

    ~SharedLibraryHandle() {
        close();
    }

    [[nodiscard]] bool is_valid() const noexcept { return handle_ != nullptr; }
    [[nodiscard]] void* get() const noexcept { return handle_; }

    /**
     * Get a function pointer from the shared library
     * @param symbol_name Name of the symbol to resolve
     * @return Function pointer or nullptr if not found
     */
    template<typename FuncType>
    [[nodiscard]] FuncType get_function(const std::string& symbol_name) const;

private:
    void* handle_ = nullptr;

    void close();
};

/**
 * Configuration loader class
 */
export class ConfigurationLoader {
public:
    ConfigurationLoader() = default;

    /**
     * Load configuration from a compiled shared library
     * @param shared_library_path Path to the compiled shared library
     * @return LoadResult with configuration or error details
     */
    [[nodiscard]] LoadResult load_from_library(const std::filesystem::path& shared_library_path);

    /**
     * Load configuration from a build.cpp file (compiles if needed)
     * @param build_cpp_path Path to the build.cpp file
     * @return LoadResult with configuration or error details
     */
    [[nodiscard]] LoadResult load_from_source(const std::filesystem::path& build_cpp_path);

    /**
     * Check if a shared library is valid and contains the configure function
     * @param shared_library_path Path to the shared library
     * @return true if the library is valid
     */
    [[nodiscard]] bool is_valid_library(const std::filesystem::path& shared_library_path) const;

private:
    /**
     * Load a shared library and return a handle
     * @param library_path Path to the shared library
     * @return SharedLibraryHandle or invalid handle on failure
     */
    [[nodiscard]] SharedLibraryHandle load_library(const std::filesystem::path& library_path) const;

    /**
     * Get the last system error message
     */
    [[nodiscard]] std::string get_last_error() const;
};

// Type definition for the configure function that must be exported by build.cpp
export using ConfigureFunction = BuildConfiguration(*)();

// Implementation

// Concept to ensure we're dealing with function pointer types
template<typename T>
concept FunctionPointer = std::is_function_v<std::remove_pointer_t<T>> && std::is_pointer_v<T>;

// SharedLibraryHandle implementation

template<typename FuncType>
FuncType SharedLibraryHandle::get_function(const std::string& symbol_name) const {
    static_assert(FunctionPointer<FuncType>, "FuncType must be a function pointer type");

    if (!is_valid()) {
        return nullptr;
    }

#ifdef _WIN32
    if (auto proc = GetProcAddress(static_cast<HMODULE>(handle_), symbol_name.c_str())) {
        return reinterpret_cast<FuncType>(proc);
    }
    return nullptr;
#else
    if (auto symbol = dlsym(handle_, symbol_name.c_str())) {
        return reinterpret_cast<FuncType>(symbol);
    }
    return nullptr;
#endif
}

void SharedLibraryHandle::close() {
    if (handle_ != nullptr) {
#ifdef _WIN32
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif
        handle_ = nullptr;
    }
}

// ConfigurationLoader implementation

LoadResult ConfigurationLoader::load_from_library(const std::filesystem::path& shared_library_path) {
    LoadResult result;

    // Check if the library file exists
    if (!std::filesystem::exists(shared_library_path)) {
        result.error_message = "Shared library not found: " + shared_library_path.string();
        return result;
    }

    // Load the shared library
    auto library_handle = load_library(shared_library_path);
    if (!library_handle.is_valid()) {
        result.error_message = "Failed to load shared library: " + shared_library_path.string() +
                              " - " + get_last_error();
        return result;
    }

    // Get the configure function using modern auto and the type alias
    if (auto configure_func = library_handle.get_function<ConfigureFunction>("configure")) {
        // Call the configure function
        try {
            auto config = configure_func();
            result.configuration = std::move(config);
            result.success = true;
        } catch (const std::exception& e) {
            result.error_message = "Exception thrown by configure function: " + std::string(e.what());
        } catch (...) {
            result.error_message = "Unknown exception thrown by configure function";
        }
    } else {
        result.error_message = "Configure function not found in shared library: " + shared_library_path.string() +
                              " - Make sure the library exports 'extern \"C\" BuildConfiguration configure()'";
    }

    return result;
}

LoadResult ConfigurationLoader::load_from_source(const std::filesystem::path& build_cpp_path) {
    LoadResult result;

    // Check if the build.cpp file exists
    if (!std::filesystem::exists(build_cpp_path)) {
        result.error_message = "Build configuration file not found: " + build_cpp_path.string();
        return result;
    }

    // Compile the build.cpp file
    ConfigurationCompiler compiler;
    auto compilation_result = compiler.compile(build_cpp_path);

    if (!compilation_result.is_success()) {
        result.error_message = "Failed to compile configuration: " + compilation_result.error_message;
        return result;
    }

    // Load the compiled shared library
    return load_from_library(compilation_result.shared_library_path);
}

bool ConfigurationLoader::is_valid_library(const std::filesystem::path& shared_library_path) const {
    if (!std::filesystem::exists(shared_library_path)) {
        return false;
    }

    if (auto library_handle = load_library(shared_library_path); library_handle.is_valid()) {
        // Check if the configure function exists using modern auto
        return library_handle.get_function<ConfigureFunction>("configure") != nullptr;
    }

    return false;
}

SharedLibraryHandle ConfigurationLoader::load_library(const std::filesystem::path& library_path) const {
#ifdef _WIN32
    if (auto handle = LoadLibraryA(library_path.string().c_str())) {
        return SharedLibraryHandle(static_cast<void*>(handle));
    }
    return SharedLibraryHandle{};
#else
    if (auto handle = dlopen(library_path.c_str(), RTLD_LAZY)) {
        return SharedLibraryHandle(handle);
    }
    return SharedLibraryHandle{};
#endif
}

std::string ConfigurationLoader::get_last_error() const {
#ifdef _WIN32
    if (auto error_code = GetLastError(); error_code != 0) {
        LPSTR message_buffer = nullptr;
        if (auto size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            error_code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&message_buffer),
            0,
            nullptr
        ); size > 0) {
            std::string message(message_buffer, size);
            LocalFree(message_buffer);
            return message;
        }
    }
    return "No error";
#else
    if (auto error = dlerror()) {
        return std::string(error);
    }
    return "No error";
#endif
}

// Explicit template instantiation for the configure function type
template ConfigureFunction SharedLibraryHandle::get_function<ConfigureFunction>(const std::string&) const;

} // namespace cppup::configuration