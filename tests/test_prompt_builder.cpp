// tests/test_prompt_builder.cpp — unit tests for prompt_builder (task 6.7).
#include "catch_amalgamated.hpp"
#include "model/cue_store.h"
#include "model/prompt_builder.h"

#include <filesystem>

using namespace diffcue::model;

TEST_CASE("build_prompt: 3-cue fixture grouped by file, sorted by line", "[prompt_builder]") {
    // Use a temp dir so the CueStore sidecar doesn't collide with anything.
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_prompt_test";
    std::filesystem::create_directories(tmp);
    CueStore store(tmp);
    store.clear();

    // Insert out of order to verify line sorting.
    store.add({"other.h", Side::New, 7, "missing include guard", 0});
    store.add({"file.cpp", Side::New, 42, "this is wrong, rename to foo", 0});
    store.add({"file.cpp", Side::Old, 10, "typo here", 0});

    std::string p = build_prompt(store);

    // No header — the prompt is a bare list, one cue per line.
    REQUIRE(p.find("# diffcue") == std::string::npos);

    // file.cpp lines sorted: 10 before 42.
    auto pos10 = p.find("file.cpp:10 - typo here");
    auto pos42 = p.find("file.cpp:42 - this is wrong, rename to foo");
    REQUIRE(pos10 != std::string::npos);
    REQUIRE(pos42 != std::string::npos);
    REQUIRE(pos10 < pos42);

    // other.h after file.cpp (grouped by file, alphabetical).
    auto pos_other = p.find("other.h:7 - missing include guard");
    REQUIRE(pos_other != std::string::npos);
    REQUIRE(pos42 < pos_other);

    // Clean up sidecar.
    std::filesystem::remove_all(tmp);
}

TEST_CASE("build_prompt: multiline cue text flattened to one line", "[prompt_builder]") {
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_prompt_multi";
    std::filesystem::create_directories(tmp);
    CueStore store(tmp);
    store.clear();

    // A cue with embedded newlines — must come out as a single line.
    store.add({"app.cpp", Side::New, 5, "line one\nline two\nline three", 0});

    std::string p = build_prompt(store);

    // The cue should occupy exactly one line in the prompt.
    auto pos = p.find("app.cpp:5 - line one line two line three");
    REQUIRE(pos != std::string::npos);

    // No raw newline should appear between "line one" and "line three".
    auto nl = p.find('\n', p.find("line one"));
    REQUIRE(nl > p.find("line three"));

    std::filesystem::remove_all(tmp);
}
