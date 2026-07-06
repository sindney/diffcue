// tests/test_git_adapter.cpp — integration test for git_adapter (task 5.6).
//
// Creates a temp git repo fixture: init → commit → modify → add → delete →
// rename → untracked. Verifies list_changes() reports each status correctly.
#include "catch_amalgamated.hpp"
#include "git/git_adapter.h"
#include "platform/subprocess.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

// Force-remove a directory tree, clearing read-only attributes on Windows
// (git's .git/ pack files are read-only and block std::filesystem::remove_all).
void remove_all_force(const std::filesystem::path& p) {
    std::error_code ec;
    if (!std::filesystem::exists(p, ec)) return;
    for (auto& entry : std::filesystem::recursive_directory_iterator(p, ec)) {
        std::filesystem::permissions(entry.path(),
            std::filesystem::perms::owner_all | std::filesystem::perms::group_all,
            std::filesystem::perm_options::replace, ec);
    }
    std::filesystem::remove_all(p, ec);
}

void run_git(const std::filesystem::path& repo, const std::vector<std::string>& args) {
    std::vector<std::string> full = {"-C", repo.generic_string()};
    full.insert(full.end(), args.begin(), args.end());
    diffcue::platform::subprocess::run_capture("git", full);
}

void write_file(const std::filesystem::path& p, const std::string& content) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << content;
}

}  // namespace

TEST_CASE("git_adapter: list_changes on a temp repo fixture", "[git_adapter][integration]") {
    auto repo = std::filesystem::temp_directory_path() / "diffcue_git_test_repo";
    remove_all_force(repo);
    std::filesystem::create_directories(repo);
    // init + identity + initial commit.
    run_git(repo, {"init", "-q"});
    run_git(repo, {"config", "user.email", "test@diffcue.test"});
    run_git(repo, {"config", "user.name", "diffcue test"});
    write_file(repo / "keep.cpp", "int a = 0;\n");
    run_git(repo, {"add", "keep.cpp"});
    run_git(repo, {"commit", "-q", "-m", "initial"});

    // modify (worktree change to a committed file)
    write_file(repo / "keep.cpp", "int a = 1;\n");

    // delete: commit a file, then remove it from the worktree.
    write_file(repo / "del.txt", "to be deleted\n");
    run_git(repo, {"add", "del.txt"});
    run_git(repo, {"commit", "-q", "-m", "stage del"});
    std::filesystem::remove(repo / "del.txt");

    // rename: commit a file, then `git mv` it (staged rename).
    write_file(repo / "old_name.h", "#pragma once\n");
    run_git(repo, {"add", "old_name.h"});
    run_git(repo, {"commit", "-q", "-m", "stage rename src"});
    run_git(repo, {"mv", "old_name.h", "new_name.h"});

    // add (staged, NOT committed — must come after all commits so it stays Added)
    write_file(repo / "new.cpp", "int b = 2;\n");
    run_git(repo, {"add", "new.cpp"});

    // untracked (never staged)
    write_file(repo / "untracked.md", "# hi\n");

    // staged-add-then-delete: stage a new file, then remove it from the
    // working tree without updating the index. Git reports this as "AD"
    // (index: Added, worktree: Deleted). diffcue should classify it as
    // Deleted — the file is gone from disk.
    write_file(repo / "ghost.cpp", "int g = 0;\n");
    run_git(repo, {"add", "ghost.cpp"});
    std::filesystem::remove(repo / "ghost.cpp");

    // list_changes should report all of the above.
    auto entries = diffcue::git::list_changes(repo);
    REQUIRE_FALSE(entries.empty());

    // Verify each expected file appears.
    auto find = [&](const std::string& path) -> const diffcue::git::GitEntry* {
        for (const auto& e : entries) {
            if (e.relpath.generic_string() == path) return &e;
        }
        return nullptr;
    };

    const auto* modified = find("keep.cpp");
    REQUIRE(modified != nullptr);
    REQUIRE(modified->status == diffcue::git::FileStatus::Modified);

    const auto* added = find("new.cpp");
    REQUIRE(added != nullptr);

    const auto* deleted = find("del.txt");
    REQUIRE(deleted != nullptr);
    REQUIRE(deleted->status == diffcue::git::FileStatus::Deleted);

    const auto* renamed = find("new_name.h");
    REQUIRE(renamed != nullptr);
    REQUIRE(renamed->status == diffcue::git::FileStatus::Renamed);
    REQUIRE(renamed->renamed_from.has_value());

    const auto* untracked = find("untracked.md");
    REQUIRE(untracked != nullptr);
    REQUIRE(untracked->status == diffcue::git::FileStatus::Untracked);

    // The staged-add-then-delete file must be classified as Deleted
    // (not Added), since it no longer exists on disk.
    const auto* ghost = find("ghost.cpp");
    REQUIRE(ghost != nullptr);
    REQUIRE(ghost->status == diffcue::git::FileStatus::Deleted);

    diffcue::git::clear_blob_cache();
    remove_all_force(repo);
}
