# cppup Configuration API Library

cmake_minimum_required(VERSION 3.20)

# Library target
add_library(cppup_config STATIC
    # Core types and structures
    types.cpp
    outputs.cpp
    profile.cpp
    build_configuration.cpp
    
    # Platform and runtime support
    platform.cpp
    runtime.cpp
    
    # Configuration processing
    compiler.cpp
    loader.cpp
    validation.cpp
    package_resolver.cpp
    toolchain_resolver.cpp
    profile_processor.cpp
    
    # Build execution
    build_executor.cpp
    build_step_executor.cpp
)

# Public headers
target_include_directories(cppup_config PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# Compiler requirements
target_compile_features(cppup_config PUBLIC cxx_std_20)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(cppup_config PRIVATE kernel32)
elseif(UNIX AND NOT APPLE)
    target_link_libraries(cppup_config PRIVATE dl pthread)
elseif(APPLE)
    target_link_libraries(cppup_config PRIVATE dl pthread)
endif()

# Export symbols for shared library builds
if(BUILD_SHARED_LIBS)
    target_compile_definitions(cppup_config PRIVATE CPPUP_CONFIG_EXPORTS)
    target_compile_definitions(cppup_config INTERFACE CPPUP_CONFIG_IMPORTS)
endif()

# Install targets
install(TARGETS cppup_config
    EXPORT cppup_config_targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install headers
install(FILES
    cppup_config.h
    types.h
    outputs.h
    profile.h
    build_configuration.h
    platform.h
    runtime.h
    build_executor.h
    DESTINATION include/cppup
)

# Install examples
install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/cppup/examples
    DESTINATION include/cppup
    FILES_MATCHING PATTERN "*.cpp"
)

# Export configuration
install(EXPORT cppup_config_targets
    FILE cppup_config_targets.cmake
    NAMESPACE cppup::
    DESTINATION lib/cmake/cppup_config
)

# Create config file
include(CMakePackageConfigHelpers)

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cppup_config_config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/cppup_config_config.cmake
    INSTALL_DESTINATION lib/cmake/cppup_config
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/cppup_config_config_version.cmake
    VERSION 1.0.0
    COMPATIBILITY SameMajorVersion
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/cppup_config_config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/cppup_config_config_version.cmake
    DESTINATION lib/cmake/cppup_config
)

# Tests
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

# Examples
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()