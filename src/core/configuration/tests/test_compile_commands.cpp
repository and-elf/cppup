#include "../compile_commands.hpp"
#include "../build_configuration.hpp"

#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>

using namespace cppup::configuration;
namespace fs = std::filesystem;

namespace {

fs::path make_tmp_dir(std::string_view tag) {
    std::random_device rd;
    auto name = std::string{"cppup_cc_test_"} + std::string{tag} + "_" +
                std::to_string(rd());
    auto path = fs::temp_directory_path() / name;
    fs::create_directories(path);
    return path;
}

std::string slurp(const fs::path& p) {
    std::ifstream f(p);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::size_t count_entries(const std::string& json) {
    std::size_t n = 0;
    std::size_t pos = 0;
    while ((pos = json.find("\"file\":", pos)) != std::string::npos) {
        ++n;
        ++pos;
    }
    return n;
}

bool contains(const std::string& s, std::string_view needle) {
    return s.find(needle) != std::string::npos;
}

}  // namespace

void test_one_entry_per_source() {
    auto root = make_tmp_dir("one_per_src");
    BuildConfiguration config;
    config.binaries.push_back(Binary{"app", {"src/main.cpp", "src/util.cpp"}});

    auto result = emit_compile_commands(config, root, root / "build", false);
    assert(result && "emit_compile_commands should succeed");
    assert(fs::exists(*result));
    assert(result->filename() == "compile_commands.json");

    auto json = slurp(*result);
    assert(count_entries(json) == 2);
    assert(contains(json, "src/main.cpp"));
    assert(contains(json, "src/util.cpp"));
    assert(contains(json, "\"directory\": \"" + root.string() + "\""));
    assert(contains(json, "\"arguments\":"));

    fs::remove_all(root);
    std::cout << "test_one_entry_per_source passed\n";
}

void test_includes_defines_and_flags() {
    auto root = make_tmp_dir("flags");
    BuildConfiguration config;
    config.compile_flags = {Flag{"-Wall"}, Flag{"-std=c++23"}};
    config.definitions = {Definition{"FOO", "bar"}, Definition{"NDEBUG"}};
    config.include_paths = {"include", "src"};
    config.binaries.push_back(Binary{"app", {"src/main.cpp"}});

    auto result = emit_compile_commands(config, root, root / "build", false);
    assert(result);

    auto json = slurp(*result);
    assert(contains(json, "\"-Wall\""));
    assert(contains(json, "\"-std=c++23\""));
    assert(contains(json, "\"-DFOO=bar\""));
    assert(contains(json, "\"-DNDEBUG\""));
    assert(contains(json, "\"-I" + (root / "include").string() + "\""));
    assert(contains(json, "\"-I" + (root / "src").string() + "\""));

    fs::remove_all(root);
    std::cout << "test_includes_defines_and_flags passed\n";
}

void test_toolchain_compiler_used() {
    auto root = make_tmp_dir("toolchain");
    BuildConfiguration config;
    config.toolchain = Toolchain{"clang++"};
    config.binaries.push_back(Binary{"app", {"main.cpp"}});

    auto result = emit_compile_commands(config, root, root / "build", false);
    assert(result);
    auto json = slurp(*result);
    assert(contains(json, "\"clang++\""));

    BuildConfiguration default_config;
    default_config.binaries.push_back(Binary{"app", {"main.cpp"}});
    auto default_root = make_tmp_dir("toolchain_default");
    auto r2 = emit_compile_commands(default_config, default_root,
                                    default_root / "build", false);
    assert(r2);
    auto json2 = slurp(*r2);
    assert(contains(json2, "\"g++\""));

    fs::remove_all(root);
    fs::remove_all(default_root);
    std::cout << "test_toolchain_compiler_used passed\n";
}

void test_all_target_kinds_emitted() {
    auto root = make_tmp_dir("targets");
    BuildConfiguration config;
    config.libraries.push_back(Library{"lib", {"lib/a.cpp", "lib/b.cpp"}});
    config.binaries.push_back(Binary{"app", {"src/main.cpp"}});
    config.tests.push_back(Test{"unit", {"tests/t.cpp"}});

    auto result = emit_compile_commands(config, root, root / "build", false);
    assert(result);

    auto json = slurp(*result);
    assert(count_entries(json) == 4);
    assert(contains(json, "lib/a.cpp"));
    assert(contains(json, "lib/b.cpp"));
    assert(contains(json, "src/main.cpp"));
    assert(contains(json, "tests/t.cpp"));

    fs::remove_all(root);
    std::cout << "test_all_target_kinds_emitted passed\n";
}

void test_asan_adds_fsanitize() {
    auto root = make_tmp_dir("asan");
    BuildConfiguration config;
    config.binaries.push_back(Binary{"app", {"main.cpp"}});

    auto without = emit_compile_commands(config, root, root / "build", false);
    assert(without);
    assert(!contains(slurp(*without), "-fsanitize=address"));

    auto with = emit_compile_commands(config, root, root / "build", true);
    assert(with);
    assert(contains(slurp(*with), "-fsanitize=address"));
    assert(contains(slurp(*with), "-fno-omit-frame-pointer"));

    fs::remove_all(root);
    std::cout << "test_asan_adds_fsanitize passed\n";
}

void test_paths_are_absolute() {
    auto root = make_tmp_dir("abs");
    BuildConfiguration config;
    config.binaries.push_back(Binary{"app", {"src/main.cpp"}});

    auto result = emit_compile_commands(config, root, root / "build", false);
    assert(result);
    auto json = slurp(*result);

    auto expected = (root / "src" / "main.cpp").string();
    assert(contains(json, "\"file\": \"" + expected + "\""));

    fs::remove_all(root);
    std::cout << "test_paths_are_absolute passed\n";
}

void test_quotes_in_definition_value_are_escaped() {
    auto root = make_tmp_dir("escape");
    BuildConfiguration config;
    config.definitions = {Definition{"VERSION", "\"1.2\""}};
    config.binaries.push_back(Binary{"app", {"main.cpp"}});

    auto result = emit_compile_commands(config, root, root / "build", false);
    assert(result);
    auto json = slurp(*result);
    assert(contains(json, "\\\"1.2\\\""));

    fs::remove_all(root);
    std::cout << "test_quotes_in_definition_value_are_escaped passed\n";
}

int main() {
    test_one_entry_per_source();
    test_includes_defines_and_flags();
    test_toolchain_compiler_used();
    test_all_target_kinds_emitted();
    test_asan_adds_fsanitize();
    test_paths_are_absolute();
    test_quotes_in_definition_value_are_escaped();
    std::cout << "All compile_commands tests passed\n";
    return 0;
}
