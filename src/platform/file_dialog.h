// platform/file_dialog.h — native folder picker via nfd (nativefiledialog-extended).
#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace diffcue::platform::file_dialog {

// Initialize NFD. Call once at startup. Returns false on failure.
bool init();

// Deinitialize NFD. Call once at shutdown.
void quit();

// Show a native folder-picker dialog. Returns the chosen folder, or
// std::nullopt if the user cancelled or an error occurred.
// `default_path` is the initially-selected folder (optional).
std::optional<std::filesystem::path> pick_folder(
    const std::string& default_path = "");

}  // namespace diffcue::platform::file_dialog
