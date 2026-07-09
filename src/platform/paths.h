// platform/paths.h — platform-specific filesystem locations.
#pragma once

#include <filesystem>

namespace diffcue::platform {

// Per-user, per-machine config directory for diffcue.
//   Windows: %APPDATA%\diffcue
//   macOS:   ~/Library/Application Support/diffcue
//   Linux:   ~/.config/diffcue
// Falls back to "." if the platform env var is unset. Shared by the ImGui
// ini (diffcue.ini) and the global prefs (prefs.json) so all user-level
// state lives in one place. Per-project state (cues) stays in
// <folder>/.diffcue/ next to the reviewed code.
std::filesystem::path config_dir();

}  // namespace diffcue::platform
