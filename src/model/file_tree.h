// model/file_tree.h — lazy tree of changed files (task 6.2).
//
// The root holds changed files discovered by git_adapter::list_changes,
// grouped into a directory tree (dirs first, then files, alphabetically).
// Each node carries a FileStatus; folder nodes aggregate their children's
// status so the file browser can show "this folder has modifications".
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "git/git_status.h"

namespace diffcue::model {

struct FileTreeNode {
    std::string name;                       // display name (file or dir basename, "" for root)
    std::filesystem::path relpath;          // path relative to the opened folder
    bool is_dir = false;
    diffcue::git::FileStatus status = diffcue::git::FileStatus::Clean;
    std::vector<std::unique_ptr<FileTreeNode>> children;  // dirs first, then files

    // Aggregated count of changed files under this node (recursive).
    int changed_file_count = 0;
};

// Build the root node from a list of git entries. The tree is built fully
// (no lazy directory iteration — git status already gives us the exact
// changed set, so there's nothing to lazily expand).
std::unique_ptr<FileTreeNode> build_file_tree(const std::vector<diffcue::git::GitEntry>& entries);

}  // namespace diffcue::model
