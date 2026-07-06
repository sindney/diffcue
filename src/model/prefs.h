// model/prefs.h — persisted user preferences (task 6.6).
//
// Stored at `<folder>/.diffcue/prefs.json`. Defaults to the first theme in
// theme.txt + "Dark" editor palette + side-by-side diff mode.
#pragma once

#include <filesystem>
#include <string>

#include "model/diff_model.h"

namespace diffcue::model {

struct Prefs {
    std::string app_theme;          // theme name from theme.txt; empty → first theme
    std::string editor_palette;     // "Dark" | "Light" | "Mariana"
    DiffMode diff_mode = DiffMode::SideBySide;
};

// Load prefs from `<folder>/.diffcue/prefs.json`. Falls back to defaults
// (first theme handled by caller, "Dark" palette, side-by-side) when the
// file is missing or malformed.
Prefs load_prefs(const std::filesystem::path& folder);

// Save prefs to `<folder>/.diffcue/prefs.json` atomically.
void save_prefs(const std::filesystem::path& folder, const Prefs& prefs);

}  // namespace diffcue::model
