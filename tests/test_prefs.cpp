// tests/test_prefs.cpp — unit tests for prefs (task 6.7).
#include "catch_amalgamated.hpp"
#include "model/prefs.h"

#include <algorithm>
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
    REQUIRE(p.recent_folders.empty());

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

// Helper: make a real existing directory so the missing-path filter keeps it.
static std::filesystem::path make_existing_dir(const std::string& suffix) {
    auto p = std::filesystem::temp_directory_path() / ("diffcue_prefs_rf_" + suffix);
    std::filesystem::remove_all(p);
    std::filesystem::create_directories(p);
    return p;
}

TEST_CASE("prefs: recent_folders empty round-trip", "[prefs][recent]") {
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_prefs_rf_empty_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    Prefs p;
    save_prefs(tmp, p);
    Prefs loaded = load_prefs(tmp);
    REQUIRE(loaded.recent_folders.empty());

    std::filesystem::remove_all(tmp);
}

TEST_CASE("prefs: recent_folders single round-trip", "[prefs][recent]") {
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_prefs_rf_single_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    auto real_dir = make_existing_dir("single");

    Prefs p;
    p.recent_folders = {real_dir};
    save_prefs(tmp, p);
    Prefs loaded = load_prefs(tmp);
    REQUIRE(loaded.recent_folders.size() == 1);
    REQUIRE(loaded.recent_folders[0] == real_dir);

    std::filesystem::remove_all(tmp);
    std::filesystem::remove_all(real_dir);
}

TEST_CASE("prefs: recent_folders 10 entries round-trip", "[prefs][recent]") {
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_prefs_rf_ten_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    std::vector<std::filesystem::path> dirs;
    for (int i = 0; i < 10; ++i) {
        dirs.push_back(make_existing_dir("ten_" + std::to_string(i)));
    }

    Prefs p;
    p.recent_folders = dirs;
    save_prefs(tmp, p);
    Prefs loaded = load_prefs(tmp);
    REQUIRE(loaded.recent_folders.size() == 10);
    for (int i = 0; i < 10; ++i) {
        REQUIRE(loaded.recent_folders[i] == dirs[i]);
    }

    std::filesystem::remove_all(tmp);
    for (auto& d : dirs) std::filesystem::remove_all(d);
}

TEST_CASE("prefs: recent_folders 11+ entries round-trip preserves all", "[prefs][recent]") {
    // The parser/writer does not enforce the cap — that's App::open_folder's
    // job. Verify the persistence layer faithfully stores whatever it's given
    // (an 11-entry list survives, so App's cap-at-10 is the single chokepoint).
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_prefs_rf_eleven_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    std::vector<std::filesystem::path> dirs;
    for (int i = 0; i < 11; ++i) {
        dirs.push_back(make_existing_dir("eleven_" + std::to_string(i)));
    }

    Prefs p;
    p.recent_folders = dirs;
    save_prefs(tmp, p);
    Prefs loaded = load_prefs(tmp);
    REQUIRE(loaded.recent_folders.size() == 11);
    for (int i = 0; i < 11; ++i) {
        REQUIRE(loaded.recent_folders[i] == dirs[i]);
    }

    std::filesystem::remove_all(tmp);
    for (auto& d : dirs) std::filesystem::remove_all(d);
}

TEST_CASE("prefs: recent_folders missing path filtered at load", "[prefs][recent]") {
    auto tmp = std::filesystem::temp_directory_path() / "diffcue_prefs_rf_missing_test";
    std::filesystem::remove_all(tmp);
    std::filesystem::create_directories(tmp);

    auto real_a = make_existing_dir("miss_a");
    auto real_b = make_existing_dir("miss_b");
    // A path that does NOT exist on disk.
    auto fake = std::filesystem::temp_directory_path() / "diffcue_prefs_rf_does_not_exist_xyz";

    Prefs p;
    p.recent_folders = {real_a, fake, real_b};
    save_prefs(tmp, p);

    Prefs loaded = load_prefs(tmp);
    REQUIRE(loaded.recent_folders.size() == 2);
    REQUIRE(loaded.recent_folders[0] == real_a);
    REQUIRE(loaded.recent_folders[1] == real_b);
    REQUIRE(std::find(loaded.recent_folders.begin(),
                      loaded.recent_folders.end(), fake)
            == loaded.recent_folders.end());

    std::filesystem::remove_all(tmp);
    std::filesystem::remove_all(real_a);
    std::filesystem::remove_all(real_b);
}
