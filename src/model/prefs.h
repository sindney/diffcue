// model/prefs.h — persisted user preferences (task 6.6).
//
// Stored globally at <config_dir>/prefs.json (e.g. %APPDATA%\diffcue\prefs.json)
// — these are editor/UI settings that apply to the user, not to a specific
// project. Per-project state (cues) lives at <folder>/.diffcue/cues.json.
// Defaults to the first theme in theme.txt + "Dark" editor palette +
// side-by-side diff mode.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "model/diff_model.h"

namespace diffcue::model {

struct Prefs {
    std::string app_theme;          // theme name from theme.txt; empty → first theme
    std::string editor_palette;     // "Dark" | "Light" | "Mariana"
    DiffMode diff_mode = DiffMode::SideBySide;
    // Most-recently-first, deduplicated by canonical path, capped at 10.
    // Folders that no longer exist on disk are filtered out at load time.
    std::vector<std::filesystem::path> recent_folders;
};

// Load prefs from <dir>/prefs.json. Falls back to defaults
// (first theme handled by caller, "Dark" palette, side-by-side) when the
// file is missing or malformed. The path-based overload is used by tests;
// the no-arg overload reads from platform::config_dir().
Prefs load_prefs(const std::filesystem::path& dir);
Prefs load_prefs();

// Save prefs to <dir>/prefs.json atomically. The path-based overload is
// used by tests; the no-arg overload writes to platform::config_dir().
void save_prefs(const std::filesystem::path& dir, const Prefs& prefs);
void save_prefs(const Prefs& prefs);

}  // namespace diffcue::model
