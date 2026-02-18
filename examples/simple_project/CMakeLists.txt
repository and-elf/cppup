# Example project using cppup Configuration API

cmake_minimum_required(VERSION 3.20)
project(simple_project VERSION 1.0.0)

# Find the cppup configuration library
find_package(cppup_config REQUIRED)

# Validate the build configuration
cppup_validate_config("${CMAKE_CURRENT_SOURCE_DIR}/build.cpp")

# Create a target to compile the build configuration
cppup_add_config_target(simple_project "${CMAKE_CURRENT_SOURCE_DIR}/build.cpp")

# Create dummy targets for demonstration
add_executable(simple_app src/main.cpp)
add_library(simple_lib STATIC src/lib/simple_lib.cpp)
add_executable(unit_tests tests/test_main.cpp)

# Link with the configuration library (if needed for runtime queries)
target_link_libraries(simple_app PRIVATE cppup::cppup_config)
target_link_libraries(simple_lib PRIVATE cppup::cppup_config)
target_link_libraries(unit_tests PRIVATE cppup::cppup_config)

# In a real cppup project, these targets would be generated automatically
# based on the build.cpp configuration