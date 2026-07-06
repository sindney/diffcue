// tests/test_cue_store.cpp — unit tests for cue_store (task 6.7).
#include "catch_amalgamated.hpp"
#include "model/cue_store.h"

#include <filesystem>

using namespace diffcue::model;

TEST_CASE("cue_store: sidecar round-trip", "[cue_store]") {
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_cue_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    {
        CueStore store(tmp);
        store.clear();
        store.add({"a.cpp", Side::New, 5, "fix this", 1000});
        store.add({"b.h", Side::Old, 12, "wrong", 2000});
        REQUIRE(store.count() == 2);
    }

    // Reload from the sidecar.
    {
        CueStore store(tmp);
        REQUIRE(store.count() == 2);
        // Cues preserve file/line/text.
        bool found_a = false, found_b = false;
        for (const auto& c : store.cues()) {
            if (c.file == "a.cpp" && c.line == 5 && c.text == "fix this") found_a = true;
            if (c.file == "b.h" && c.line == 12 && c.text == "wrong") found_b = true;
        }
        REQUIRE(found_a);
        REQUIRE(found_b);
    }

    std::filesystem::remove_all(tmp);
}

TEST_CASE("cue_store: stale detection", "[cue_store]") {
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_cue_stale_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    CueStore store(tmp);
    store.clear();
    store.add({"a.cpp", Side::New, 5, "ok", 0});
    store.add({"a.cpp", Side::New, 999, "stale", 0});

    // File has 10 lines: line 5 is valid, line 999 is stale.
    store.refresh_stale([](const std::filesystem::path&, Side) -> int { return 10; });

    REQUIRE_FALSE(store.cues()[0].stale);   // line 5
    REQUIRE(store.cues()[1].stale);          // line 999

    REQUIRE(store.active_count() == 1);

    std::filesystem::remove_all(tmp);
}
