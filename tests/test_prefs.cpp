// tests/test_prefs.cpp — unit tests for prefs (task 6.7).
#include "catch_amalgamated.hpp"
#include "model/prefs.h"

#include <filesystem>

using namespace diffcue::model;

TEST_CASE("prefs: defaults when missing", "[prefs]") {
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_prefs_default_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    Prefs p = load_prefs(tmp);
    REQUIRE(p.editor_palette == "Dark");
    REQUIRE(p.diff_mode == DiffMode::SideBySide);
    REQUIRE(p.app_theme.empty());  // caller applies first theme when empty

    std::filesystem::remove_all(tmp);
}

TEST_CASE("prefs: round-trip", "[prefs]") {
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_prefs_roundtrip_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    Prefs original;
    original.app_theme = "Dracula";
    original.editor_palette = "Mariana";
    original.diff_mode = DiffMode::Inline;
    save_prefs(tmp, original);

    Prefs loaded = load_prefs(tmp);
    REQUIRE(loaded.app_theme == "Dracula");
    REQUIRE(loaded.editor_palette == "Mariana");
    REQUIRE(loaded.diff_mode == DiffMode::Inline);

    std::filesystem::remove_all(tmp);
}
