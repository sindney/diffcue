// tests/test_subprocess.cpp — integration test for subprocess (task 10.4).
#include "catch_amalgamated.hpp"
#include "platform/subprocess.h"

TEST_CASE("subprocess: capture git --version", "[subprocess][integration]") {
    std::string out = diffcue::platform::subprocess::run_capture("git", {"--version"});
    REQUIRE_FALSE(out.empty());
    REQUIRE(out.find("git version") != std::string::npos);
}

TEST_CASE("subprocess: run_succeeds on valid command", "[subprocess][integration]") {
    bool ok = diffcue::platform::subprocess::run_succeeds("git", {"--version"});
    REQUIRE(ok);
}

TEST_CASE("subprocess: empty output on non-existent command", "[subprocess]") {
    std::string out = diffcue::platform::subprocess::run_capture(
        "diffcue_nonexistent_binary_xyz", {"--foo"});
    REQUIRE(out.empty());
}
