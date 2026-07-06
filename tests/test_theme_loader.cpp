// tests/test_theme_loader.cpp — unit test for theme_loader (task 7.7).
//
// Asserts that load_themes() returns exactly the number of Setup*Style()
// function headers in thirdparty/theme.txt. Catches drift between the
// hardcoded theme table and the parsed file.
#include "catch_amalgamated.hpp"
#include "ui/theme_loader.h"

#include "imgui.h"

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

namespace {

// Count `void Setup*Style()` headers in theme.txt by scanning the file.
int count_theme_headers(const std::string& path) {
    std::ifstream f(path);
    if (!f) return -1;
    std::string line;
    int n = 0;
    while (std::getline(f, line)) {
        // Match "void SetupXxxStyle()" at the start of a line (after whitespace).
        auto first_non_space = line.find_first_not_of(" \t");
        if (first_non_space == std::string::npos) continue;
        std::string trimmed = line.substr(first_non_space);
        if (trimmed.rfind("void Setup", 0) == 0 &&
            trimmed.find("Style()") != std::string::npos) {
            ++n;
        }
    }
    return n;
}

}  // namespace

TEST_CASE("theme_loader: parsed count matches theme.txt headers", "[theme_loader]") {
    const auto& themes = diffcue::ui::load_themes();
    REQUIRE_FALSE(themes.empty());

    const std::string theme_txt_path = DIFFCUE_SOURCE_DIR "/thirdparty/theme.txt";
    int headers = count_theme_headers(theme_txt_path);
    INFO("theme.txt has " << headers << " Setup*Style() headers; "
         "load_themes() returned " << themes.size());
    REQUIRE(headers > 0);
    REQUIRE(themes.size() == static_cast<size_t>(headers));
}

TEST_CASE("theme_loader: apply_theme falls back to first on unknown", "[theme_loader]") {
    // apply_theme() calls ImGui::GetStyle() / StyleColorsDark() which require
    // a live ImGui context. Create one for the duration of this test.
    ImGui::CreateContext();
    // Should not crash; falls back to the first theme.
    diffcue::ui::apply_theme("NonExistentThemeName");
    diffcue::ui::apply_theme("");  // empty name also falls back
    ImGui::DestroyContext();
}
