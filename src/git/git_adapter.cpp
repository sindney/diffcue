// git/git_adapter.cpp — git CLI wrapper (tasks 5.2-5.5).
//
// Spawns `git -C <root> status --porcelain=v1 -z` and `git show HEAD:<path>`
// via platform::subprocess::run_capture. Parses porcelain v1 -z output into
// GitEntry. No libgit2; git is a hard runtime prerequisite.
#include "git/git_adapter.h"

#include <cstdio>
#include <fstream>
#include <mutex>
#include <sstream>

#include "platform/subprocess.h"

namespace diffcue::git {

namespace {

// Cache HEAD-blob contents per (root, relpath) for the session.
// Key is "<root>/<relpath>" in generic string form.
std::mutex g_blob_mutex;
std::unordered_map<std::string, std::string> g_blob_cache;

FileStatus parse_status_code(char x, char y) {
    // porcelain v1: XY where X=index, Y=worktree.
    // We collapse to a single display status.
    if (x == '?' && y == '?') return FileStatus::Untracked;
    // Worktree deletion takes precedence — if the file is gone from disk
    // (Y='D'), show it as Deleted regardless of what the index claims
    // (e.g. staged-as-added then deleted from disk → "AD" → Deleted).
    if (y == 'D') return FileStatus::Deleted;
    if (x == 'D') return FileStatus::Deleted;
    if (x == 'A' || y == 'A') return FileStatus::Added;
    if (x == 'R' || y == 'R') return FileStatus::Renamed;
    if (x == 'C' || y == 'C') return FileStatus::Renamed;  // copy → treat as rename-ish
    if (x == 'M' || y == 'M') return FileStatus::Modified;
    return FileStatus::Modified;  // any other change → modified
}

}  // namespace

std::vector<GitEntry> list_changes(const std::filesystem::path& root) {
    std::vector<GitEntry> entries;
    const std::string root_str = root.generic_string();
    const std::string out = platform::subprocess::run_capture(
        "git", {"-C", root_str, "status", "--porcelain=v1", "-z", "-uall"});

    // Parse NUL-delimited records. Each record is "XY <path>" or
    // "XY <path>\0<orig_path>" for renames/copies.
    size_t i = 0;
    const size_t len = out.size();
    while (i + 2 < len) {
        char x = out[i];
        char y = out[i + 1];
        // Skip the space after XY.
        if (out[i + 2] != ' ') {
            // Malformed; advance one byte and retry.
            ++i;
            continue;
        }
        i += 3;

        // Read the path up to the next NUL.
        size_t path_end = out.find('\0', i);
        if (path_end == std::string::npos) path_end = len;
        std::string path_str(out, i, path_end - i);
        i = path_end + 1;

        GitEntry e;
        e.status = parse_status_code(x, y);
        e.relpath = std::filesystem::path(path_str);

        // For renames/copies (R/C), a second NUL-delimited field holds the
        // original path.
        if (e.status == FileStatus::Renamed && i < len) {
            size_t orig_end = out.find('\0', i);
            if (orig_end == std::string::npos) orig_end = len;
            std::string orig(out, i, orig_end - i);
            e.renamed_from = std::filesystem::path(orig);
            i = orig_end + 1;
        }

        entries.push_back(std::move(e));
    }
    return entries;
}

std::string read_blob_old(const std::filesystem::path& root,
                          const std::filesystem::path& relpath) {
    const std::string key = (root / relpath).generic_string();
    {
        std::lock_guard<std::mutex> lk(g_blob_mutex);
        auto it = g_blob_cache.find(key);
        if (it != g_blob_cache.end()) return it->second;
    }

    std::string out = platform::subprocess::run_capture(
        "git", {"-C", root.generic_string(), "show",
                "HEAD:" + relpath.generic_string()});

    // Empty result is ambiguous (could be an empty file or a missing one);
    // we cache whatever git returned so we don't re-spawn.
    std::lock_guard<std::mutex> lk(g_blob_mutex);
    g_blob_cache[key] = out;
    return out;
}

std::string read_blob_new(const std::filesystem::path& root,
                          const std::filesystem::path& relpath) {
    const auto full = root / relpath;
    std::ifstream f(full, std::ios::binary);
    if (!f) return {};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

bool git_available() {
    return platform::subprocess::run_succeeds("git", {"--version"});
}

bool is_repo(const std::filesystem::path& root) {
    return platform::subprocess::run_succeeds(
        "git", {"-C", root.generic_string(), "rev-parse", "--is-inside-work-tree"});
}

void clear_blob_cache() {
    std::lock_guard<std::mutex> lk(g_blob_mutex);
    g_blob_cache.clear();
}

}  // namespace diffcue::git
