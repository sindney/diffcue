// git/git_adapter.h — git CLI wrapper (tasks 5.2-5.4). No libgit2.
#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "git/git_status.h"

namespace diffcue::git {

// List all changed files in `root` (tracked + untracked) by spawning
// `git -C <root> status --porcelain=v1 -z`. Returns one GitEntry per
// changed file. On error (git missing, not a repo), returns empty.
std::vector<GitEntry> list_changes(const std::filesystem::path& root);

// Read the HEAD version of `relpath` via `git -C <root> show HEAD:<relpath>`.
// Returns empty string if the file is untracked (no HEAD version) or on
// error. Cached per session so repeated opens of the same file are cheap.
std::string read_blob_old(const std::filesystem::path& root,
                          const std::filesystem::path& relpath);

// Read the working-tree version of `relpath` directly via std::ifstream.
// Returns empty string on read failure.
std::string read_blob_new(const std::filesystem::path& root,
                          const std::filesystem::path& relpath);

// Probe whether `git` is on PATH and runnable. Used by the startup check
// (task 5.5). Returns true if `git --version` exits 0.
bool git_available();

// Check if `root` is inside a git work tree.
bool is_repo(const std::filesystem::path& root);

// Clear the per-session HEAD-blob cache. Called when the folder changes.
void clear_blob_cache();

}  // namespace diffcue::git
