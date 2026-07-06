// tests/test_cli_args.cpp — unit tests for cli/args (task 4.4).
#include "catch_amalgamated.hpp"
#include "cli/args.h"

#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace {

// Build a char* argv from a list of strings (argv[0] is the program name).
std::vector<char*> make_argv(const std::vector<std::string>& args) {
    static std::vector<std::string> storage;
    storage = args;
    static std::vector<char*> argv;
    argv.clear();
    for (auto& s : storage) argv.push_back(s.data());
    argv.push_back(nullptr);
    return argv;
}

}  // namespace

TEST_CASE("parse_args: --help", "[cli]") {
    auto argv = make_argv({"diffcue", "--help"});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Help);
}

TEST_CASE("parse_args: -h", "[cli]") {
    auto argv = make_argv({"diffcue", "-h"});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Help);
}

TEST_CASE("parse_args: --version", "[cli]") {
    auto argv = make_argv({"diffcue", "--version"});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Version);
}

TEST_CASE("parse_args: -V", "[cli]") {
    auto argv = make_argv({"diffcue", "-V"});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Version);
}

TEST_CASE("parse_args: unknown flag", "[cli]") {
    auto argv = make_argv({"diffcue", "--bogus"});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Error);
    REQUIRE_FALSE(r.error.empty());
}

TEST_CASE("parse_args: too many positionals", "[cli]") {
    auto argv = make_argv({"diffcue", "folder1", "folder2"});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Error);
}

TEST_CASE("parse_args: non-existent path", "[cli]") {
    auto argv = make_argv({"diffcue", "/this/path/does/not/exist/xyz123"});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Error);
}

TEST_CASE("parse_args: file not folder", "[cli]") {
    // Use this test file itself as a non-directory path.
    auto argv = make_argv({"diffcue", __FILE__});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Error);
}

TEST_CASE("parse_args: valid folder", "[cli]") {
    auto argv = make_argv({"diffcue", "."});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Ok);
    REQUIRE(r.folder.has_value());
}

TEST_CASE("parse_args: no folder uses current dir", "[cli]") {
    auto argv = make_argv({"diffcue"});
    auto r = diffcue::cli::parse_args(static_cast<int>(argv.size() - 1), argv.data());
    REQUIRE(r.status == diffcue::cli::ParseStatus::Ok);
    REQUIRE(r.folder.has_value());
}

TEST_CASE("usage_text is non-empty", "[cli]") {
    REQUIRE_FALSE(diffcue::cli::usage_text().empty());
}
