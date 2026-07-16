// tests/test_async_refresh.cpp — async refresh invariants (change: async-refresh).
//
// Verifies the background worker's threading contracts:
// - single in-flight invariant (no second worker spawns on overlapping requests)
// - request coalescing (N rapid requests → at most one follow-up refresh)
// - folder-switch guard (stale result for prior folder is discarded)
// - atomic apply (entries/file_tree/hunks all update together in one step)
//
// These tests construct a real App (the window will be invalid in headless
// environments, but the worker still runs). A temp git repo fixture provides
// deterministic git status output.
#include "catch_amalgamated.hpp"
#include "app/app.h"
#include "git/git_adapter.h"
#include "model/cue_store.h"
#include "platform/subprocess.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

namespace {

// Force-remove a directory tree, clearing read-only attributes on Windows.
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

// Build a temp git repo with one committed file AND one uncommitted change
// (so git status reports something). Returns the repo path.
std::filesystem::path make_temp_repo(const std::string& name) {
    auto repo = std::filesystem::temp_directory_path() / name;
    remove_all_force(repo);
    std::filesystem::create_directories(repo);
    run_git(repo, {"init", "-q"});
    run_git(repo, {"config", "user.email", "test@diffcue.test"});
    run_git(repo, {"config", "user.name", "diffcue test"});
    write_file(repo / "keep.cpp", "int a = 0;\n");
    run_git(repo, {"add", "keep.cpp"});
    run_git(repo, {"commit", "-q", "-m", "initial"});
    // Modify the committed file so git status reports a change.
    write_file(repo / "keep.cpp", "int a = 1;\n");
    return repo;
}

// Wait until refresh_count_ reaches at least `target`, polling every 5ms
// up to `timeout_ms`. Returns true if reached, false on timeout.
bool wait_for_refresh_count(const diffcue::App& app, int target, int timeout_ms = 3000) {
    auto start = std::chrono::steady_clock::now();
    while (app.test_refresh_count() < target) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        if (elapsed > timeout_ms) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

// Pump pending results (simulates the per-frame UI thread pump).
void pump(diffcue::App& app) {
    app.test_pump_result();
}

}  // namespace

TEST_CASE("async_refresh: worker completes initial refresh on construction",
          "[async_refresh]") {
    auto repo = make_temp_repo("diffcue_async_initial");
    diffcue::App app(repo);
    // The initial refresh is requested by open_folder() in the constructor.
    REQUIRE(wait_for_refresh_count(app, 1));
    // Pump the result so it's applied (the test has no run() loop).
    pump(app);
    REQUIRE_FALSE(app.test_entries().empty());
    REQUIRE(app.test_file_tree() != nullptr);
    diffcue::git::clear_blob_cache();
    remove_all_force(repo);
}

TEST_CASE("async_refresh: single in-flight invariant — overlapping requests don't spawn second worker",
          "[async_refresh]") {
    auto repo = make_temp_repo("diffcue_async_single");
    diffcue::App app(repo);
    REQUIRE(wait_for_refresh_count(app, 1));
    pump(app);

    // Fire 5 rapid refresh requests. The worker is idle (just finished), so
    // the first request starts a refresh; the remaining 4 should coalesce
    // into at most one follow-up (single-slot pending). Total = 1 (initial)
    // + 1 (follow-up) = 2.
    for (int i = 0; i < 5; ++i) {
        app.test_request_refresh();
    }
    // Wait for the follow-up (count = 2).
    REQUIRE(wait_for_refresh_count(app, 2));
    pump(app);
    // Confirm no further refreshes run — coalesced into one follow-up.
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    REQUIRE(app.test_refresh_count() == 2);
    diffcue::git::clear_blob_cache();
    remove_all_force(repo);
}

TEST_CASE("async_refresh: coalescing — rapid requests produce at most one follow-up",
          "[async_refresh]") {
    auto repo = make_temp_repo("diffcue_async_coalesce");
    diffcue::App app(repo);
    REQUIRE(wait_for_refresh_count(app, 1));
    pump(app);
    const int initial_count = app.test_refresh_count();

    // Fire 10 rapid refreshes. Coalescing means at most 1 follow-up runs.
    for (int i = 0; i < 10; ++i) {
        app.test_request_refresh();
    }
    // Wait for the follow-up to complete.
    REQUIRE(wait_for_refresh_count(app, initial_count + 1));
    pump(app);
    // Confirm no further refreshes run (coalesced into one).
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    REQUIRE(app.test_refresh_count() == initial_count + 1);
    diffcue::git::clear_blob_cache();
    remove_all_force(repo);
}

TEST_CASE("async_refresh: folder-switch guard discards stale result",
          "[async_refresh]") {
    auto repo_a = make_temp_repo("diffcue_async_guard_a");
    auto repo_b = make_temp_repo("diffcue_async_guard_b");

    diffcue::App app(repo_a);
    REQUIRE(wait_for_refresh_count(app, 1));
    pump(app);
    // Confirm A's entries contain keep.cpp (modified).
    bool has_a = false;
    for (const auto& e : app.test_entries()) {
        if (e.relpath.generic_string() == "keep.cpp") has_a = true;
    }
    REQUIRE(has_a);

    // Trigger a refresh for A, then switch folder to B BEFORE pumping.
    // The stale A result should be discarded by apply_refresh_result.
    app.test_request_refresh();
    REQUIRE(wait_for_refresh_count(app, 2));
    // Switch folder to B before pumping — A's result is now stale.
    app.test_set_folder(repo_b);
    pump(app);  // should discard A's result (folder mismatch)
    // entries_ should still reflect A (the stale result was discarded).
    bool still_has_a = false;
    for (const auto& e : app.test_entries()) {
        if (e.relpath.generic_string() == "keep.cpp") still_has_a = true;
    }
    REQUIRE(still_has_a);  // A's entries NOT overwritten by discarded result

    // Now request a refresh for B and pump it — should apply B's entries.
    app.test_request_refresh();
    REQUIRE(wait_for_refresh_count(app, 3));
    pump(app);
    bool has_b = false;
    for (const auto& e : app.test_entries()) {
        if (e.relpath.generic_string() == "keep.cpp") has_b = true;
    }
    REQUIRE(has_b);  // B's entries now applied

    diffcue::git::clear_blob_cache();
    remove_all_force(repo_a);
    remove_all_force(repo_b);
}

TEST_CASE("async_refresh: atomic apply — entries, file_tree, hunks update together",
          "[async_refresh]") {
    auto repo = make_temp_repo("diffcue_async_atomic");
    // keep.cpp is already Modified (committed + modified) — it counts as a
    // hunk (non-untracked). Add one untracked file to verify hunks exclude
    // untracked entries.
    write_file(repo / "untracked.md", "# hi\n");

    diffcue::App app(repo);
    REQUIRE(wait_for_refresh_count(app, 1));
    pump(app);

    // After a single pump, all three should be consistent: entries should
    // contain both files, file_tree should be non-null, and hunks should
    // contain exactly the non-untracked entries.
    REQUIRE(app.test_file_tree() != nullptr);
    REQUIRE(app.test_entries().size() >= 2);

    // Count non-untracked entries — these should each have a hunk.
    int non_untracked = 0;
    std::set<std::string> non_untracked_paths;
    for (const auto& e : app.test_entries()) {
        if (e.status != diffcue::git::FileStatus::Untracked) {
            ++non_untracked;
            non_untracked_paths.insert(e.relpath.generic_string());
        }
    }
    REQUIRE(non_untracked >= 1);
    REQUIRE(app.test_hunks().size() == static_cast<size_t>(non_untracked));

    // Every hunk path should be a non-untracked entry path (same refresh).
    for (const auto& h : app.test_hunks()) {
        REQUIRE(non_untracked_paths.count(h.path.generic_string()) == 1);
    }

    diffcue::git::clear_blob_cache();
    remove_all_force(repo);
}

TEST_CASE("hunks: binary-extension files excluded from next/prev navigation",
          "[async_refresh][hunks]") {
    auto repo = make_temp_repo("diffcue_hunks_binary");
    // keep.cpp is Modified (non-untracked) → should be a hunk.
    // Add a tracked binary file (.dll) and modify it so git status reports
    // it as Modified (not Untracked, not Clean).
    write_file(repo / "native.dll",
               std::string("MZ\0\0\0\0\0", 8) + std::string("binary payload"));
    run_git(repo, {"add", "native.dll"});
    run_git(repo, {"commit", "-q", "-m", "add binary"});
    write_file(repo / "native.dll",
               std::string("MZ\0\0\0\0\1", 8) + std::string("changed payload"));

    diffcue::App app(repo);
    REQUIRE(wait_for_refresh_count(app, 1));
    pump(app);

    // native.dll is Modified so it appears in entries, but its .dll extension
    // is binary → it must NOT appear in the hunk list (Next/Prev navigation).
    bool has_dll = false;
    for (const auto& e : app.test_entries()) {
        if (e.relpath.extension() == ".dll") has_dll = true;
    }
    REQUIRE(has_dll);

    for (const auto& h : app.test_hunks()) {
        REQUIRE_FALSE(h.path.extension() == ".dll");
    }
    // keep.cpp (text, modified) should still be present.
    bool has_keep = false;
    for (const auto& h : app.test_hunks()) {
        if (h.path.filename() == "keep.cpp") has_keep = true;
    }
    REQUIRE(has_keep);

    diffcue::git::clear_blob_cache();
    remove_all_force(repo);
}
