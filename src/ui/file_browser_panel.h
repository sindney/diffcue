// ui/file_browser_panel.h — left-panel file tree (task 8.2).
#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>

#include "model/file_tree.h"

namespace diffcue::ui {

struct FileBrowserActions {
    std::optional<std::filesystem::path> open_file;
};

// Render the file browser tree. `cued_files` is the set of relpaths that
// have at least one cue — these get a yellow dot badge. `current_file` is
// the currently-open file (highlighted).
FileBrowserActions render_file_browser(const model::FileTreeNode& root,
                                       bool show_all,
                                       const std::set<std::string>& cued_files = {},
                                       const std::string& current_file = "");

}  // namespace diffcue::ui
