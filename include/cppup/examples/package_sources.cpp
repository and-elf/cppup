/**
 * @file package_sources.cpp
 * @brief Examples of using different package sources in cppup
 *
 * This file demonstrates how to specify packages from various sources:
 * - Git repositories
 * - Local directories
 * - TAR/ZIP archives
 * - Header-only libraries
 * - Different build systems
 */

#include <cppup/configuration.hpp>

using namespace cppup::configuration;

extern "C" BuildConfiguration configure()
{
  BuildConfiguration config;

  // === Git Repository Sources ===

  // Clone from Git repository (latest main/master branch)
  config.packages.push_back(Package::from_git("fmt", "https://github.com/fmtlib/fmt.git"));

  // Clone specific branch
  config.packages.push_back(
      Package::from_git("spdlog", "https://github.com/gabime/spdlog.git", "v1.x"));

  // Clone with specific commit (for reproducible builds)
  auto nlohmann_json = Package::from_git("nlohmann_json", "https://github.com/nlohmann/json.git");
  nlohmann_json.git_commit = "bc889afb4c5bf1c0d8ee29ef35eaaf4c8bef8a5d";  // v3.11.2
  config.packages.push_back(nlohmann_json);

  // === Local Directory Sources ===

  // Use local development version of a library
  config.packages.push_back(Package::from_directory("my_local_lib", "../my_local_lib"));

  // Use library in subdirectory of current project
  config.packages.push_back(Package::from_directory("embedded_lib", "third_party/embedded_lib"));

  // === Archive Sources ===

  // Download and extract TAR archive
  config.packages.push_back(Package::from_tar(
      "boost",
      "https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz"));

  // Download ZIP archive
  Package eigen_zip("eigen", "3.4.0",
                    "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.zip",
                    SourceType::ZIP);
  config.packages.push_back(eigen_zip);

  // === Header-Only Libraries ===

  // Header-only library from Git
  config.packages.push_back(
      Package::header_only("catch2", "https://github.com/catchorg/Catch2.git"));

  // Header-only with specific include directory
  auto header_only_lib =
      Package::header_only("single_header_lib", "https://github.com/user/lib.git");
  header_only_lib.subdirectory = "single_include";  // Headers are in single_include/ subdirectory
  config.packages.push_back(header_only_lib);

  // === Different Build Systems ===

  // CMake-based package
  Package cmake_pkg("opencv", "4.8.0", "https://github.com/opencv/opencv.git", SourceType::GIT,
                    BuildSystem::CMAKE);
  cmake_pkg.build_args = {"-DBUILD_EXAMPLES=OFF", "-DBUILD_TESTS=OFF"};  // CMake options
  config.packages.push_back(cmake_pkg);

  // Meson-based package
  Package meson_pkg("gstreamer", "1.22.0", "https://gitlab.freedesktop.org/gstreamer/gstreamer.git",
                    SourceType::GIT, BuildSystem::MESON);
  meson_pkg.build_args = {"-Dtests=disabled", "-Dexamples=disabled"};
  config.packages.push_back(meson_pkg);

  // Traditional Make-based package
  Package make_pkg("zlib", "1.2.13", "https://github.com/madler/zlib.git", SourceType::GIT,
                   BuildSystem::MAKE);
  config.packages.push_back(make_pkg);

  // Autotools-based package
  Package autotools_pkg("libxml2", "2.10.4", "https://gitlab.gnome.org/GNOME/libxml2.git",
                        SourceType::GIT, BuildSystem::AUTOTOOLS);
  autotools_pkg.build_args = {"--without-python", "--disable-shared"};  // Configure options
  config.packages.push_back(autotools_pkg);

  // === Complex Package with Subdirectory ===

  // Package where the actual library is in a subdirectory
  Package subdir_pkg("protobuf", "3.21.12", "https://github.com/protocolbuffers/protobuf.git",
                     SourceType::GIT, BuildSystem::CMAKE);
  subdir_pkg.subdirectory = "cmake";  // CMakeLists.txt is in cmake/ subdirectory
  subdir_pkg.build_args   = {"-Dprotobuf_BUILD_TESTS=OFF"};
  config.packages.push_back(subdir_pkg);

  // === Registry vs Source Packages ===

  // These packages will be resolved from source (as defined above)
  // vs registry packages which use the default SourceType::REGISTRY:

  // Registry package (traditional package manager style)
  config.packages.push_back(Package{"sqlite3", "3.42.0"});  // Uses package registry

  // Source package (built from source)
  config.packages.push_back(
      Package::from_git("sqlite3_source", "https://github.com/sqlite/sqlite.git")  // Built from Git
  );

  // === Build Configuration ===

  config.sources       = {"src/*.cpp"};
  config.compile_flags = {Flag{"-std=c++23"}, Flag{"-Wall"}, Flag{"-Wextra"}};

  config.binaries = {Binary{"my_app", {"src/main.cpp"}}};

  return config;
}

// === Alternative: Conditional Package Sources ===

extern "C" BuildConfiguration configure_conditional()
{
  BuildConfiguration config;

  // Use different package sources based on environment
  if (get_env("CPPUP_USE_LOCAL_DEPS").has_value())
  {
    // Development mode: use local directories
    config.packages.push_back(Package::from_directory("fmt", "../fmt"));
    config.packages.push_back(Package::from_directory("spdlog", "../spdlog"));
  }
  else
  {
    // Production mode: use specific Git commits for reproducibility
    auto fmt_pkg       = Package::from_git("fmt", "https://github.com/fmtlib/fmt.git");
    fmt_pkg.git_commit = "a33701196adfad74917046096bf5a2aa0ab0bb50";  // v9.1.0
    config.packages.push_back(fmt_pkg);

    auto spdlog_pkg       = Package::from_git("spdlog", "https://github.com/gabime/spdlog.git");
    spdlog_pkg.git_commit = "76fb40d95455f249bd70824ecfcae7a8f0930fa3";  // v1.11.0
    config.packages.push_back(spdlog_pkg);
  }

  // Platform-specific packages
  when_linux(
      [&]()
      {
        config.packages.push_back(
            Package::from_git("linux_specific", "https://github.com/user/linux-lib.git"));
      });

  when_windows(
      [&]()
      {
        config.packages.push_back(
            Package::from_git("windows_specific", "https://github.com/user/windows-lib.git"));
      });

  config.sources  = {"src/*.cpp"};
  config.binaries = {Binary{"conditional_app", {"src/main.cpp"}}};

  return config;
}

// === Example: Monorepo with Multiple Packages ===

extern "C" BuildConfiguration configure_monorepo()
{
  BuildConfiguration config;

  // Large monorepo with multiple packages in subdirectories
  Package llvm_pkg("llvm", "16.0.0", "https://github.com/llvm/llvm-project.git", SourceType::GIT,
                   BuildSystem::CMAKE);
  llvm_pkg.subdirectory = "llvm";  // LLVM is in llvm/ subdirectory
  llvm_pkg.build_args   = {"-DLLVM_ENABLE_PROJECTS=clang;lld",
                           "-DLLVM_TARGETS_TO_BUILD=X86;ARM;AArch64", "-DCMAKE_BUILD_TYPE=Release"};
  config.packages.push_back(llvm_pkg);

  // Qt with specific modules
  Package qt_pkg("qt", "6.5.0", "https://code.qt.io/qt/qt5.git", SourceType::GIT,
                 BuildSystem::CMAKE);
  qt_pkg.build_args = {"-DQT_BUILD_EXAMPLES=OFF", "-DQT_BUILD_TESTS=OFF", "-DFEATURE_gui=ON",
                       "-DFEATURE_widgets=ON"};
  config.packages.push_back(qt_pkg);

  config.sources  = {"src/*.cpp"};
  config.binaries = {Binary{"monorepo_app", {"src/main.cpp"}}};

  return config;
}