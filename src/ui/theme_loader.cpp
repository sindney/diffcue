// ui/theme_loader.cpp — theme table + apply (tasks 7.1, 7.2, 7.4, design D11).
//
// theme_defs.inc (committed alongside this file) contains the 12
// SetupXxxStyle() function definitions extracted from thirdparty/theme.txt.
// We #include it here so the functions are compiled into this TU, then
// expose them via load_themes().
#include "ui/theme_loader.h"

#include <cstdio>
#include <cstring>

#include "imgui.h"

// Pull in the theme function definitions.
#include "theme_defs.inc"

namespace diffcue::ui {

namespace {

// Hardcoded table of (display-name, function-pointer). The 12 names come from
// the `void Setup*Style()` headers in theme.txt; the unit test in task 7.7
// asserts the count matches the headers, so any drift is caught.
const ThemeEntry kThemes[] = {
    {"Dark",             &SetupImGuiDarkStyle},
    {"ForestGreen",      &SetupForestGreenStyle},
    {"Amethyst",         &SetupImGuiAmethystStyle},
    {"Sapphire",         &SetupImGuiSapphireStyle},
    {"AmberYellow",      &SetupImGuiAmberYellowStyle},
    {"Dracula",          &SetupImGuiDraculaStyle},
    {"CatppuccinMocha",  &SetupImGuiCatppuccinMochaStyle},
    {"GruvboxHard",      &SetupImGuiGruvboxHardStyle},
    {"CrimsonVesuvius",  &SetupImGuiCrimsonVesuviusStyle},
    {"RoseQuartz",       &SetupImGuiRoseQuartzStyle},
    {"Cyberpunk",        &SetupImGuiCyberpunkStyle},
    {"PaperAndInk",      &SetupImGuiPaperAndInkStyle},
};

constexpr size_t kThemeCount = sizeof(kThemes) / sizeof(kThemes[0]);

}  // namespace

const std::vector<ThemeEntry>& load_themes() {
    static const std::vector<ThemeEntry> themes(std::begin(kThemes), std::end(kThemes));
    return themes;
}

void apply_theme(const std::string& name) {
    for (size_t i = 0; i < kThemeCount; ++i) {
        if (kThemes[i].name == name) {
            kThemes[i].apply();
            return;
        }
    }
    // No match — fall back to the first theme rather than leaving the app
    // unstyled. Log to stderr so the user can spot a typo'd prefs entry.
    if (!name.empty()) {
        std::fprintf(stderr, "diffcue: unknown app theme '%s', using default\n", name.c_str());
    }
    kThemes[0].apply();
}

}  // namespace diffcue::ui
