// git/git_status.h — FileStatus enum + GitEntry struct (task 5.1).
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace diffcue::git {

enum class FileStatus {
    Modified,
    Added,
    Deleted,
    Renamed,
    Untracked,
    Clean,
};

// One entry from `git status --porcelain=v1 -z`.
struct GitEntry {
    std::filesystem::path relpath;
    FileStatus status = FileStatus::Clean;
    // For Renamed entries, the original path before the rename.
    std::optional<std::filesystem::path> renamed_from;

    // Short status code for display in the file browser (task 8.2).
    const char* short_code() const;
    // Human-readable label.
    const char* label() const;
};

}  // namespace diffcue::git
