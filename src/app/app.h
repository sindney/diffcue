// app/app.h — the App class owns all state + the run loop (task 9.1).
#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <vector>

#include "git/git_status.h"
#include "model/cue_store.h"
#include "model/diff_model.h"
#include "model/file_tree.h"
#include "model/prefs.h"
#include "platform/window.h"
#include "ui/command_palette.h"
#include "ui/diff_viewer_panel.h"
#include "ui/find_bar.h"
#include "ui/prompt_panel.h"

namespace diffcue {

class App {
public:
    explicit App(std::filesystem::path folder);
    ~App();

    // Run the event loop until the user quits. Returns the process exit code.
    int run();

private:
    // Re-scan git status and rebuild the file tree (called on folder open
    // and on focus-regain, task 9.6).
    void refresh_git_status();

    // Open a folder by its already-canonical path. Consolidates the shared
    // body for all folder-open entrances (CLI init, menubar picker,
    // drag-drop, Open Recent, command palette). Sets folder_, title, prefs,
    // cues, git status, resets current file/diff/viewer, applies prefs.
    void open_folder(const std::filesystem::path& canonical);

    // Open a file from the file browser into the diff viewer (task 9.3).
    void open_file(const std::filesystem::path& relpath);

    // Build a FileDiff for `relpath` by reading old/new blobs (task 6.3).
    model::FileDiff build_file_diff(const std::filesystem::path& relpath);

    // Collect all hunks across all modified files in file-browser order
    // (task 8.8). Each entry is (relpath, hunk_index).
    struct HunkRef { std::filesystem::path path; int hunk_index; };
    void collect_all_hunks();
    // DFS walker that matches the file browser's display order (dirs-first,
    // alphabetical) so Next/Prev steps through files in the same order the
    // user sees in the tree.
    static void collect_hunks_recursive(const model::FileTreeNode& node,
                                        std::vector<HunkRef>& out);
    void jump_hunk(int delta);

    // Prev/Next: scroll to the next changed line within the current file,
    // or switch to the next file if at the end of changes.
    void scroll_to_next_change(int direction);

    // Apply prefs (theme, palette, diff mode) on startup (task 7.6).
    void apply_prefs();

    platform::Window window_;
    std::filesystem::path folder_;
    std::vector<git::GitEntry> entries_;
    std::unique_ptr<model::FileTreeNode> file_tree_;
    std::optional<model::CueStore> cues_;
    model::Prefs prefs_;
    std::optional<model::FileDiff> current_diff_;
    std::filesystem::path current_open_path_;

    ui::DiffViewerPanel diff_viewer_;
    ui::PromptPanel prompt_panel_;
    ui::FindBarState find_bar_;

    bool prompt_open_ = false;
    float prompt_toast_ms_ = 0.0f;
    bool show_all_ = false;
    bool git_missing_ = false;
    bool not_git_repo_ = false;
    bool about_open_ = false;
    // Cmd+P command palette visibility (toggled by bare Cmd+P / Ctrl+P).
    bool palette_open_ = false;
    // Set when the user picks a recent-folder entry whose path no longer
    // exists; rendered as a modal error popup next frame.
    std::string folder_error_;

    // Hunk navigation state (task 8.8).
    std::vector<HunkRef> all_hunks_;
    int current_hunk_ = -1;
    int current_change_idx_ = 0;

    // Track the last applied palette/theme to detect changes.
    std::string last_applied_palette_;
    std::string last_applied_theme_;
    model::DiffMode last_applied_mode_ = model::DiffMode::SideBySide;
};

}  // namespace diffcue
