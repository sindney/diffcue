// app/app.cpp — App class + run loop (tasks 9.1-9.6).
#include "app/app.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <set>
#include <vector>

#include "dtl.h"

#include "imgui.h"
#include "imgui_internal.h"  // DockBuilder API (default dock layout seeding)
#include "diffcue/version.h"
#include "git/git_adapter.h"
#include "model/file_meta.h"
#include "model/prompt_builder.h"
#include "platform/clipboard.h"
#include "platform/file_dialog.h"
#include "platform/open_url.h"
#include "ui/file_browser_panel.h"
#include "ui/menubar.h"
#include "ui/command_palette.h"
#include "ui/theme_loader.h"
#include "ui/toolbar_panel.h"

#include <GLFW/glfw3.h>

namespace diffcue {

namespace {

// Static drop path: the GLFW drop callback sets this, the run loop reads +
// clears it next frame. diffcue is single-instance, so a static is fine.
std::optional<std::filesystem::path> g_dropped_folder;

// Static focus flag: set by the GLFW focus callback when the window regains
// focus, processed next frame to re-scan git status.
std::atomic<bool> g_window_focused{false};

// Center a modal popup on the viewport with a fixed default size.
// Call this right before BeginPopupModal. Uses ImGuiCond_Appearing so the
// user can still resize/move the popup after it opens.
void center_modal(ImVec2 size) {
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 center(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                 vp->WorkPos.y + vp->WorkSize.y * 0.5f);
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
}

// Seed the main dockspace with a default layout that reproduces the
// pre-docking fixed arrangement: Files on the left (~20% / ~300px), Diff
// filling the center. Prompt is intentionally NOT docked here — it opens as
// a centered floating window (see prompt_panel.cpp's SetNextWindowPos) and
// the user can dock it manually by dragging. Only fires on first run when
// the dockspace is empty (no ini / fresh install). After this one-shot,
// ImGui persists the user's layout in diffcue.ini and the static flag keeps
// the check from running every frame.
//
// Uses the DockBuilder API from imgui_internal.h. Must be called BEFORE the
// corresponding Begin("Files"/"Diff"/"Prompt") calls in the same frame —
// DockBuilderDockWindow patches pending window docks by name.
void seed_default_dock_layout(ImGuiID dockspace_id) {
    static bool seeded = false;
    if (seeded) return;
    ImGuiDockNode* node = ImGui::DockBuilderGetNode(dockspace_id);
    if (node != nullptr && !node->IsEmpty()) {
        // A saved layout (from ini) is already present — don't clobber it.
        seeded = true;
        return;
    }
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetContentRegionAvail());
    // Split left (~20% ≈ 300px on a 1500px-wide window) for Files; Diff
    // fills the remainder.
    ImGuiID id_left = 0, id_main = 0;
    ImGui::DockBuilderSplitNode(dockspace_id, ImGuiDir_Left, 0.20f, &id_left, &id_main);
    ImGui::DockBuilderDockWindow("Files", id_left);
    ImGui::DockBuilderDockWindow("Diff", id_main);

    ImGui::DockBuilderFinish(dockspace_id);
    seeded = true;
}

// GLFW drop callback — stores the first dropped path for the App to process
// next frame. Called during glfwPollEvents(), so we must not touch ImGui here.
void glfw_drop_callback(GLFWwindow* /*win*/, int path_count, const char* paths[]) {
    if (path_count <= 0 || !paths || !paths[0]) return;
    std::error_code ec;
    auto p = std::filesystem::path(paths[0]);
    if (std::filesystem::is_directory(p, ec)) {
        g_dropped_folder = std::move(p);
    }
}

// GLFW focus callback — sets a flag so the run loop re-scans git status when
// the user returns to diffcue (files may have changed externally).
void glfw_focus_callback(GLFWwindow* /*win*/, int focused) {
    if (focused) {
        g_window_focused.store(true, std::memory_order_relaxed);
    }
}

}  // namespace

App::App(std::filesystem::path folder)
    : folder_(std::move(folder)) {
    // Initialize the native file dialog library.
    platform::file_dialog::init();

    // Register drag-and-drop callback (GLFW file drop).
    if (window_.handle()) {
        glfwSetDropCallback(window_.handle(), glfw_drop_callback);
        glfwSetWindowFocusCallback(window_.handle(), glfw_focus_callback);
    }

    // Probe git (task 5.5).
    if (!git::git_available()) {
        git_missing_ = true;
        return;
    }

    // Start the background refresh worker BEFORE open_folder() so the
    // first refresh request (issued at the end of open_folder) is served
    // async. The worker waits on worker_cv_ until request_pending_ is set,
    // so it won't read folder_/cues_ until open_folder has set them. If
    // the thread fails to spawn (rare: OS out of resources), fall back to
    // synchronous refresh — correctness is preserved, only the async UX
    // is lost.
    cancel_token_ = std::make_shared<std::atomic<bool>>(false);
    try {
        worker_ = std::thread([this] { worker_loop(); });
    } catch (const std::system_error& e) {
        std::cerr << "diffcue: could not start refresh worker ("
                  << e.what() << "); falling back to sync refresh\n";
        worker_failed_ = true;
    }

    // The CLI folder is already canonical (see cli/args.cpp). Delegate the
    // shared folder-open body to open_folder.
    open_folder(folder_);
}

App::~App() {
    // Signal the worker to shut down. Set the cancel token FIRST so any
    // in-flight git subprocess is killed immediately (the worker's
    // compute_refresh() will return early), THEN set shutdown_ and join.
    // Without the cancel, the destructor would block waiting for a slow
    // `git status` to finish — pointless when the user is closing the app.
    if (cancel_token_) cancel_token_->store(true, std::memory_order_relaxed);
    shutdown_.store(true, std::memory_order_release);
    worker_cv_.notify_all();
    result_cv_.notify_one();  // unblock worker if waiting on result consumption
    if (worker_.joinable()) {
        worker_.join();
    }
    platform::file_dialog::quit();
}

// Run on the worker thread. Produces the entire result of one refresh:
// entries, file tree, hunks, and per-cue stale flags. Does NOT touch the
// live App members consumed by the UI thread (entries_/file_tree_/cues_) —
// the UI thread applies the result in apply_refresh_result().
App::RefreshResult App::compute_refresh() {
    RefreshResult r;
    r.folder = folder_;
    // Capture the current cancel token. If open_folder() creates a new token
    // (for a folder switch), the old token flips to true and list_changes
    // kills the git subprocess. The new refresh (next loop iteration) will
    // capture the new token and proceed normally.
    auto cancel = cancel_token_;
    const std::atomic<bool>* cancel_ptr = cancel ? cancel.get() : nullptr;
    r.entries = git::list_changes(folder_, cancel_ptr);
    // If cancelled, return early with whatever we have (likely empty).
    // The stale-folder guard in apply_refresh_result() will discard it.
    if (cancel_ptr && cancel_ptr->load(std::memory_order_relaxed)) {
        return r;
    }
    r.file_tree = model::build_file_tree(r.entries);

    // Build hunk list locally from the new file tree (matches the order
    // collect_all_hunks would produce on the UI thread).
    if (r.file_tree) {
        collect_hunks_recursive(*r.file_tree, r.hunks);
    }

    // Snapshot the current cue list (file + side + line only) so the worker
    // doesn't read cues_ while the UI thread might mutate it. The snapshot
    // is small (file, side, line — no text/created pointers).
    struct CueSnap { std::filesystem::path file; model::Side side; int line; };
    std::vector<CueSnap> snap;
    if (cues_) {
        snap.reserve(cues_->cues().size());
        for (const auto& c : cues_->cues()) {
            snap.push_back({c.file, c.side, c.line});
        }
    }

    // Evaluate staleness against the snapshot. Look up each cued file in the
    // new entries to decide whether to read its line count.
    for (const auto& c : snap) {
        auto it = std::find_if(r.entries.begin(), r.entries.end(),
            [&](const git::GitEntry& e) { return e.relpath == c.file; });
        bool stale = true;
        if (it != r.entries.end()) {
            std::string text = git::read_blob_new(folder_, c.file);
            int lines = 0;
            for (char ch : text) if (ch == '\n') ++lines;
            if (!text.empty() && text.back() != '\n') ++lines;
            stale = (lines == 0 || c.line < 1 || c.line > lines);
        }
        r.cue_stale[c.file.generic_string()] = {c.side, stale};
    }

    return r;
}

void App::worker_loop() {
    while (true) {
        // Wait for a request (or shutdown).
        {
            std::unique_lock<std::mutex> lk(worker_mutex_);
            worker_cv_.wait(lk, [this] {
                return shutdown_.load(std::memory_order_relaxed) ||
                       request_pending_.load(std::memory_order_relaxed);
            });
        }
        if (shutdown_.load(std::memory_order_relaxed)) return;

        // Single-slot backpressure: don't produce a new result until the
        // prior one has been consumed by the UI thread.
        {
            std::unique_lock<std::mutex> lk(result_mutex_);
            result_cv_.wait(lk, [this] {
                return !result_ready_.load(std::memory_order_relaxed) ||
                       shutdown_.load(std::memory_order_relaxed);
            });
        }
        if (shutdown_.load(std::memory_order_relaxed)) return;

        // Clear the request now — we're about to serve it. Any request that
        // arrived during the waits above is reflected in this run (folder_
        // is read inside compute_refresh). A request arriving DURING
        // compute_refresh sets request_pending_ again and triggers one
        // follow-up loop — that's the coalescing.
        request_pending_.store(false, std::memory_order_relaxed);

        refresh_in_flight_.store(true, std::memory_order_relaxed);
        RefreshResult r = compute_refresh();
        refresh_in_flight_.store(false, std::memory_order_relaxed);
        refresh_count_.fetch_add(1, std::memory_order_relaxed);

        if (shutdown_.load(std::memory_order_relaxed)) return;

        {
            std::lock_guard<std::mutex> lk(result_mutex_);
            result_ = std::move(r);
            result_ready_.store(true, std::memory_order_release);
        }
        result_cv_.notify_one();
    }
}

void App::request_refresh() {
    if (worker_failed_ || !worker_.joinable()) {
        // Worker isn't running — run sync on the UI thread (fallback).
        RefreshResult r = compute_refresh();
        apply_refresh_result(std::move(r));
        return;
    }
    {
        std::lock_guard<std::mutex> lk(worker_mutex_);
        request_pending_.store(true, std::memory_order_relaxed);
    }
    worker_cv_.notify_one();
}

bool App::try_take_result(RefreshResult& out) {
    if (!result_ready_.load(std::memory_order_acquire)) {
        return false;
    }
    {
        std::lock_guard<std::mutex> lk(result_mutex_);
        if (result_.has_value()) {
            out = std::move(*result_);
            result_.reset();
        }
        result_ready_.store(false, std::memory_order_release);
    }
    result_cv_.notify_one();
    return true;
}

void App::apply_refresh_result(RefreshResult&& r) {
    // Stale-folder guard: if the user switched folders while this refresh
    // was in flight, discard the result so it can't overwrite the new
    // folder's state.
    if (r.folder != folder_) {
        return;
    }
    git::clear_blob_cache();
    entries_ = std::move(r.entries);
    file_tree_ = std::move(r.file_tree);
    all_hunks_ = std::move(r.hunks);
    // Apply per-cue stale flags by (file, side) lookup so cue add/remove
    // that happened on the UI thread during the refresh is safe — an
    // unmapped cue simply keeps its current stale flag.
    if (cues_) {
        for (auto& c : cues_->cues()) {
            auto it = r.cue_stale.find(c.file.generic_string());
            if (it != r.cue_stale.end() && it->second.first == c.side) {
                c.stale = it->second.second;
            }
        }
    }
}

void App::refresh_git_status() {
    // Async: enqueue a refresh request to the background worker. The worker
    // produces a RefreshResult off-thread; the UI thread pumps and applies it
    // once per frame in App::run(). Coalescing means overlapping requests
    // collapse to one running + one pending refresh.
    request_refresh();
}

void App::open_folder(const std::filesystem::path& canonical) {
    // Cancel any in-flight git refresh for the previous folder. The worker's
    // compute_refresh() captured the OLD cancel_token_; setting it to true
    // makes list_changes kill its git subprocess and return early. We then
    // create a NEW token so the upcoming refresh for this folder runs
    // uninterrupted.
    if (cancel_token_) cancel_token_->store(true, std::memory_order_relaxed);
    cancel_token_ = std::make_shared<std::atomic<bool>>(false);

    folder_ = canonical;
    window_.set_title("diffcue - " + folder_.generic_string());
    prefs_ = model::load_prefs();
    cues_.emplace(folder_);
    git::clear_blob_cache();
    // Drop any unconsumed result from a prior folder so a late result from
    // the previous folder can't overwrite this new folder's state.
    {
        std::lock_guard<std::mutex> lk(result_mutex_);
        result_.reset();
        result_ready_.store(false, std::memory_order_release);
    }
    result_cv_.notify_one();
    refresh_git_status();
    current_open_path_.clear();
    current_diff_.reset();
    diff_viewer_.clear();
    not_git_repo_ = !git::is_repo(folder_);
    apply_prefs();

    // Record this folder in recent_folders: dedupe by canonical path
    // (remove if present), insert at position 0, cap at 10. Persist
    // immediately so the list survives a crash right after opening.
    auto& rf = prefs_.recent_folders;
    rf.erase(std::remove(rf.begin(), rf.end(), folder_), rf.end());
    rf.insert(rf.begin(), folder_);
    if (rf.size() > 10) rf.resize(10);
    model::save_prefs(prefs_);
}

model::FileDiff App::build_file_diff(const std::filesystem::path& relpath) {
    model::FileDiff fd;
    fd.path = relpath;

    // Fast path: known binary extension → skip blob reading entirely.
    // read_blob_old spawns a `git show HEAD:<path>` subprocess and both
    // readers load the full file into memory — pointless for binaries.
    if (model::is_binary_extension(relpath.extension().string())) {
        fd.binary = true;
        fd.old_meta.encoding = model::Encoding::Binary;
        fd.new_meta.encoding = model::Encoding::Binary;
        return fd;
    }

    auto it = std::find_if(entries_.begin(), entries_.end(),
        [&](const git::GitEntry& e) { return e.relpath == relpath; });
    const bool is_new = (it != entries_.end() &&
                         (it->status == git::FileStatus::Untracked ||
                          it->status == git::FileStatus::Added));

    if (is_new) {
        fd.old_text.clear();  // no HEAD version → pure addition
    } else {
        fd.old_text = git::read_blob_old(folder_, relpath);
    }
    fd.new_text = git::read_blob_new(folder_, relpath);
    fd.old_meta = model::detect_meta(fd.old_text);
    fd.new_meta = model::detect_meta(fd.new_text);
    fd.binary = (fd.old_meta.encoding == model::Encoding::Binary ||
                 fd.new_meta.encoding == model::Encoding::Binary);
    // Truncation check removed: it was counting total lines in the new file
    // (not changed lines), so any file >5000 lines was incorrectly marked as
    // truncated. The diff viewer can handle large files on its own.
    return fd;
}

void App::open_file(const std::filesystem::path& relpath) {
    current_open_path_ = relpath;
    current_diff_ = build_file_diff(relpath);
    diff_viewer_.set_diff(*current_diff_, prefs_.diff_mode);
    current_change_idx_ = 0;

    // Update the next/prev position to match the clicked file,
    // so Next/Prev continues from this point in the file chain.
    for (int i = 0; i < static_cast<int>(all_hunks_.size()); ++i) {
        if (all_hunks_[i].path == relpath) {
            current_hunk_ = i;
            break;
        }
    }
}

// Walk the file tree in DFS order (children are already sorted dirs-first
// alphabetically by build_file_tree) and collect file nodes that are not
// Untracked. This matches the order the user sees in the file browser tree,
// so Next/Prev steps through files in the same sequence as clicking down the
// tree.
void App::collect_hunks_recursive(const model::FileTreeNode& node,
                                   std::vector<HunkRef>& out) {
    if (node.is_dir) {
        for (const auto& c : node.children) {
            collect_hunks_recursive(*c, out);
        }
    } else if (node.status != git::FileStatus::Untracked) {
        // Skip files with known binary extensions (.dll/.lib/.a/.so/.png/...)
        // so Next/Prev only lands on text files. Extension-based only — no
        // blob reading here (read_blob_old spawns a git subprocess per file).
        if (model::is_binary_extension(node.relpath.extension().string()))
            return;
        out.push_back({node.relpath, 0});
    }
}

void App::collect_all_hunks() {
    all_hunks_.clear();
    if (file_tree_) {
        collect_hunks_recursive(*file_tree_, all_hunks_);
    }
}

void App::jump_hunk(int delta) {
    if (all_hunks_.empty()) return;
    if (current_hunk_ < 0) current_hunk_ = 0;
    current_hunk_ = (current_hunk_ + delta + static_cast<int>(all_hunks_.size()))
                    % static_cast<int>(all_hunks_.size());
    open_file(all_hunks_[current_hunk_].path);
}

// Helper: compute changed line sections using dtl (proper diff, not naive
// line-by-line). Returns {start_1based, end_1based} for each contiguous
// range of added/changed lines in the new file.
static std::vector<std::pair<int,int>> compute_changed_sections(const model::FileDiff& fd) {
    // Normalize EOL to LF (same as the diff viewer's "Ignore EOL" does).
    // Without this, CRLF vs LF differences make every line look "changed"
    // to dtl, producing one giant section covering the entire file.
    auto normalize_eol = [](const std::string& s) {
        std::string out; out.reserve(s.size());
        for (size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\r') {
                out += '\n';
                if (i + 1 < s.size() && s[i + 1] == '\n') ++i;
            } else { out += s[i]; }
        }
        return out;
    };

    auto split_lines = [](const std::string& text) {
        std::vector<std::string> lines;
        std::string cur;
        for (char c : text) {
            if (c == '\n') { lines.push_back(cur); cur.clear(); }
            else cur += c;
        }
        if (!cur.empty()) lines.push_back(cur);
        return lines;
    };

    auto old_lines = split_lines(normalize_eol(fd.old_text));
    auto new_lines = split_lines(normalize_eol(fd.new_text));

    // Use dtl to compute the shortest edit script.
    dtl::Diff<std::string, std::vector<std::string>> diff(old_lines, new_lines);
    diff.compose();
    auto ses = diff.getSes().getSequence();

    // Walk the SES to find contiguous ADD ranges in the new file.
    std::vector<std::pair<int,int>> sections;
    int new_line = 1;  // 1-based, current line in new file
    int section_start = -1;

    for (const auto& entry : ses) {
        int type = entry.second.type;
        if (type == 1) {  // SES_ADD
            if (section_start < 0) section_start = new_line;
            new_line++;
        } else if (type == 0) {  // SES_COMMON
            if (section_start >= 0) {
                sections.push_back({section_start, new_line - 1});
                section_start = -1;
            }
            new_line++;
        } else { // SES_DELETE (-1)
            if (section_start >= 0) {
                sections.push_back({section_start, new_line - 1});
                section_start = -1;
            }
        }
    }
    if (section_start >= 0) {
        sections.push_back({section_start, new_line - 1});
    }

    return sections;
}

void App::scroll_to_next_change(int direction) {
    if (all_hunks_.empty()) return;
    const int n_files = static_cast<int>(all_hunks_.size());

    // Ensure we have a file open.
    if (!current_diff_ || current_hunk_ < 0) {
        current_hunk_ = 0;
        current_change_idx_ = 0;
        open_file(all_hunks_[0].path);
        return;
    }

    // Compute sections for the current file.
    auto sections = compute_changed_sections(*current_diff_);

    if (sections.empty()) {
        current_hunk_ = (current_hunk_ + direction + n_files) % n_files;
        current_change_idx_ = 0;
        open_file(all_hunks_[current_hunk_].path);
        auto new_sections = compute_changed_sections(*current_diff_);
        if (!new_sections.empty()) {
            int idx = (direction > 0) ? 0 : static_cast<int>(new_sections.size()) - 1;
            current_change_idx_ = idx;
            diff_viewer_.scroll_to_line(new_sections[idx].first);
        }
        return;
    }

    // Advance within the current file.
    current_change_idx_ += direction;

    if (current_change_idx_ < 0) {
        current_hunk_ = (current_hunk_ - 1 + n_files) % n_files;
        open_file(all_hunks_[current_hunk_].path);
        auto new_sections = compute_changed_sections(*current_diff_);
        current_change_idx_ = new_sections.empty() ? 0
            : static_cast<int>(new_sections.size()) - 1;
        if (!new_sections.empty())
            diff_viewer_.scroll_to_line(new_sections[current_change_idx_].first);
    } else if (current_change_idx_ >= static_cast<int>(sections.size())) {
        current_hunk_ = (current_hunk_ + 1) % n_files;
        open_file(all_hunks_[current_hunk_].path);
        auto new_sections = compute_changed_sections(*current_diff_);
        current_change_idx_ = 0;
        if (!new_sections.empty())
            diff_viewer_.scroll_to_line(new_sections[0].first);
    } else {
        diff_viewer_.scroll_to_line(sections[current_change_idx_].first);
    }
}

void App::apply_prefs() {
    // App theme (task 7.6): use saved theme or the first one. When prefs has
    // no theme set, default to the first theme name so the menubar shows a
    // checkmark next to the active selection.
    const auto& themes = ui::load_themes();
    if (prefs_.app_theme.empty() && !themes.empty()) {
        prefs_.app_theme = themes[0].name;
    }
    ui::apply_theme(prefs_.app_theme);
    last_applied_theme_ = prefs_.app_theme;

    // Editor palette.
    diff_viewer_.set_palette_by_name(prefs_.editor_palette);
    last_applied_palette_ = prefs_.editor_palette;

    // Diff mode.
    diff_viewer_.set_diff_mode(prefs_.diff_mode);
    last_applied_mode_ = prefs_.diff_mode;
}

int App::run() {
    if (!window_.valid()) {
        std::cerr << "diffcue: failed to create window\n";
        return 1;
    }

    if (git_missing_) {
        // Modal error dialog (task 5.5): loop until the user dismisses.
        while (!window_.should_close()) {
            window_.poll_events();
            window_.new_frame();
            ImGui::OpenPopup("git missing");
            center_modal(ImVec2(480, 200));
            if (ImGui::BeginPopupModal("git missing", nullptr,
                                       ImGuiWindowFlags_NoCollapse)) {
                ImGui::Text("git not found on PATH.\ndiffcue requires git installed.");
                ImGui::Separator();
                if (ImGui::Button("OK")) {
                    ImGui::CloseCurrentPopup();
                    window_.request_close();
                }
                ImGui::EndPopup();
            }
            window_.render();
        }
        return 2;
    }

    // Show a one-time warning if the folder is not a git repo.
    bool show_not_repo_warning = not_git_repo_;

    while (!window_.should_close()) {
        window_.poll_events();
        window_.new_frame();

        // --- Command palette keyboard shortcuts ---
        // Bare Cmd+P / Ctrl+P toggles the palette; Cmd+Shift+P / Ctrl+Shift+P
        // runs Copy Prompt. Check the shifted combo first so a Shift+P press
        // does not fall through to the bare-P branch. The palette toggle is
        // owned here (the palette does not handle Cmd+P itself — avoids a
        // double-toggle).
        {
            ImGuiIO& io = ImGui::GetIO();
            if (ImGui::IsKeyPressed(ImGuiKey_P, false) && io.KeyCtrl && io.KeyShift) {
                std::string prompt = model::build_prompt(*cues_);
                prompt_panel_.set_text(prompt);
                prompt_open_ = true;
            } else if (ImGui::IsKeyPressed(ImGuiKey_P, false) && io.KeyCtrl && !io.KeyShift) {
                palette_open_ = !palette_open_;
            }
        }

        // --- Command palette render (top of frame, before menubar/toolbar) ---
        ui::PaletteActions paction;
        if (palette_open_) {
            ui::render_command_palette(paction, palette_open_, prefs_, *cues_);
        }

        // --- Not-a-git-repo warning ---
        if (show_not_repo_warning) {
            ImGui::OpenPopup("Not a Git Repository");
            show_not_repo_warning = false;
        }
        center_modal(ImVec2(520, 220));
        if (ImGui::BeginPopupModal("Not a Git Repository", nullptr, ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("'%s' is not a git repository.", folder_.generic_string().c_str());
            ImGui::Text("diffcue requires a git working tree to show changes.");
            ImGui::Separator();
            if (ImGui::Button("OK")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // --- Process drag-and-drop folder (set by the GLFW drop callback) ---
        if (g_dropped_folder) {
            std::error_code ec;
            auto canonical = std::filesystem::canonical(*g_dropped_folder, ec);
            if (!ec) {
                open_folder(canonical);
            }
            g_dropped_folder.reset();
        }

        // --- Pump: apply any completed async refresh result before evaluating
        // new triggers this frame. This keeps the visible state consistent
        // within a single frame (file tree, hunks, cue stale flags all swap
        // together). ---
        {
            RefreshResult r;
            if (try_take_result(r)) {
                apply_refresh_result(std::move(r));
            }
        }

        // --- Re-scan on window focus regain (files may have changed) ---
        if (g_window_focused.exchange(false, std::memory_order_relaxed)) {
            if (!not_git_repo_) {
                refresh_git_status();
            }
        }

        // --- Menubar ---
        auto mactions = ui::render_menubar(prefs_);
        // Merge palette menubar-type commands onto the SAME mactions flags so
        // the existing handlers run (no duplicated effect code).
        if (paction.run) {
            switch (paction.command) {
                case ui::PaletteCommand::OpenFolder:
                    mactions.open_folder_clicked = true; break;
                case ui::PaletteCommand::OpenRecent:
                    mactions.open_recent_index = paction.payload; break;
                case ui::PaletteCommand::ShowAll:
                    mactions.show_all_toggled = true;
                    mactions.show_all = !show_all_; break;
                case ui::PaletteCommand::About:
                    mactions.about_clicked = true; break;
                case ui::PaletteCommand::Quit:
                    mactions.quit_clicked = true; break;
                default: break;  // toolbar-type commands merged below
            }
        }
        if (mactions.quit_clicked) window_.request_close();
        if (mactions.show_all_toggled) show_all_ = mactions.show_all;
        if (mactions.open_palette_clicked) palette_open_ = true;

        // --- Keyboard shortcuts (the menu items show Ctrl+O / Ctrl+Q / Ctrl+F
        // as hint labels, but ImGui doesn't wire them — we handle them here) ---
        if (ImGui::IsKeyPressed(ImGuiKey_O, false) && ImGui::GetIO().KeyCtrl) {
            mactions.open_folder_clicked = true;
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Q, false) && ImGui::GetIO().KeyCtrl) {
            window_.request_close();
        }

        if (mactions.open_folder_clicked) {
            // Native folder picker (blocking modal — UI freezes while open).
            auto picked = platform::file_dialog::pick_folder(folder_.generic_string());
            if (picked) {
                std::error_code ec;
                auto canonical = std::filesystem::canonical(*picked, ec);
                if (!ec) {
                    open_folder(canonical);
                }
            }
        }
        if (mactions.open_recent_index >= 0) {
            // Lookup the path in prefs_.recent_folders. If it no longer
            // exists on disk, show an error popup and remove the entry
            // (spec: "Stale entry removed on selection"). Otherwise open
            // it via the shared open_folder path.
            const int idx = mactions.open_recent_index;
            if (idx >= 0 && idx < static_cast<int>(prefs_.recent_folders.size())) {
                const auto path = prefs_.recent_folders[idx];
                std::error_code ec;
                if (!std::filesystem::exists(path, ec)) {
                    folder_error_ = "error: folder not found: " + path.generic_string();
                    prefs_.recent_folders.erase(prefs_.recent_folders.begin() + idx);
                    model::save_prefs(prefs_);
                } else {
                    auto canonical = std::filesystem::canonical(path, ec);
                    if (!ec) {
                        open_folder(canonical);
                    } else {
                        folder_error_ = "error: folder not found: " + path.generic_string();
                        prefs_.recent_folders.erase(prefs_.recent_folders.begin() + idx);
                        model::save_prefs(prefs_);
                    }
                }
            }
        }
        if (mactions.clear_recent) {
            prefs_.recent_folders.clear();
            model::save_prefs(prefs_);
        }
        if (mactions.about_clicked) about_open_ = true;

        // --- About modal ---
        if (about_open_) {
            ImGui::OpenPopup("About");
            about_open_ = false;
        }
        center_modal(ImVec2(520, 320));
        if (ImGui::BeginPopupModal("About", nullptr, ImGuiWindowFlags_NoCollapse)) {
            ImGui::Text("diffcue %s", DIFFCUE_VERSION);
            ImGui::Separator();
            ImGui::Text("Diff reviewer + cue annotator for AI coding CLI review loops.");
            ImGui::Text("Review diff -> annotate lines -> emit a structured follow-up prompt.");
            ImGui::Separator();
            ImGui::Text("Built with ImGui + ImGuiColorTextEdit + GLFW3 (all statically linked).");
            ImGui::Text("Built with GLM 5.2 + CodeBuddy.");
            ImGui::Separator();
            if (ImGui::TextLink("https://github.com/sindney/diffcue")) {
                platform::open_url("https://github.com/sindney/diffcue");
            }
            ImGui::Separator();
            if (ImGui::Button("OK")) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // --- Recent-folder error popup (stale entry selected) ---
        if (!folder_error_.empty()) {
            ImGui::OpenPopup("Folder Error");
        }
        center_modal(ImVec2(480, 180));
        if (ImGui::BeginPopupModal("Folder Error", nullptr, ImGuiWindowFlags_NoCollapse)) {
            ImGui::TextWrapped("%s", folder_error_.c_str());
            ImGui::Separator();
            if (ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter, false)
                || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
                folder_error_.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // --- Fullscreen host window (fills the viewport work area below the
        // menubar). All panels render as children of this window so they fill
        // the available space instead of floating. ---
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(vp->WorkPos);
        ImGui::SetNextWindowSize(vp->WorkSize);
        ImGui::Begin("##main", nullptr,
                     ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus |
                     ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground);

        // --- Toolbar (task 9.2: top, 32px) ---
        {
            ImGui::BeginChild("toolbar", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
            auto tactions = ui::render_toolbar(*cues_, prefs_,
                                               diff_viewer_.ignore_eol(),
                                               is_refreshing());
            // Merge palette toolbar-type commands onto the SAME tactions flags
            // so the existing handlers run (no duplicated effect code).
            if (paction.run) {
                switch (paction.command) {
                    case ui::PaletteCommand::NextChange:
                        tactions.next_change = true; break;
                    case ui::PaletteCommand::PrevChange:
                        tactions.prev_change = true; break;
                    case ui::PaletteCommand::Refresh:
                        tactions.refresh = true; break;
                    case ui::PaletteCommand::ClearCues:
                        tactions.clear_cues = true; break;
                    case ui::PaletteCommand::CopyPrompt:
                        tactions.copy_prompt = true; break;
                    case ui::PaletteCommand::JumpToCue:
                        tactions.jump_to_cue_index = paction.payload; break;
                    case ui::PaletteCommand::ListCues:
                        tactions.open_cue_list = true; break;
                    case ui::PaletteCommand::ToggleDiffMode:
                        // Flip prefs.diff_mode here; the handler below applies it.
                        prefs_.diff_mode = (prefs_.diff_mode == model::DiffMode::SideBySide)
                                           ? model::DiffMode::Inline : model::DiffMode::SideBySide;
                        tactions.diff_mode_toggled = true; break;
                    case ui::PaletteCommand::ToggleIgnoreEOL:
                        tactions.ignore_eol_toggled = true; break;
                    default: break;  // menubar-type commands handled above
                }
            }
            if (tactions.prev_change) scroll_to_next_change(-1);
            if (tactions.next_change) scroll_to_next_change(1);
            if (tactions.refresh) {
                if (!not_git_repo_) refresh_git_status();
            }
            if (tactions.copy_prompt) {
                std::string prompt = model::build_prompt(*cues_);
                prompt_panel_.set_text(prompt);
                prompt_open_ = true;
            }
            if (tactions.clear_cues) {
                ImGui::OpenPopup("Clear Cues?");
            }
            center_modal(ImVec2(440, 180));
            if (ImGui::BeginPopupModal("Clear Cues?", nullptr, ImGuiWindowFlags_NoCollapse)) {
                ImGui::Text("Remove all %d cues?", cues_->count());
                ImGui::Separator();
                // Focus the confirm button ONLY when the modal first opens.
                // Calling SetKeyboardFocusHere every frame steals focus from
                // the Cancel button, making it unclickable.
                if (ImGui::IsWindowAppearing()) {
                    ImGui::SetKeyboardFocusHere();
                }
                if (ImGui::Button("Clear (Enter)")) {
                    cues_->clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel (Esc)")) {
                    ImGui::CloseCurrentPopup();
                }
                // Explicit keyboard contract: Enter confirms, Esc cancels.
                // These run alongside ImGui's default focused-button behavior;
                // the explicit handlers are the source of truth.
                if (ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                    cues_->clear();
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (tactions.diff_mode_toggled) {
                diff_viewer_.set_diff_mode(prefs_.diff_mode);
                if (current_diff_) diff_viewer_.set_diff(*current_diff_, prefs_.diff_mode);
                model::save_prefs(prefs_);
            }
            if (tactions.ignore_eol_toggled) {
                diff_viewer_.set_ignore_eol(!diff_viewer_.ignore_eol());
            }
            if (tactions.jump_to_cue_index >= 0) {
                const auto& c = cues_->cues()[tactions.jump_to_cue_index];
                open_file(c.file);
                diff_viewer_.scroll_to_line(c.line);
            }
            if (tactions.open_cue_list) {
                cue_list_open_ = true;
                cue_list_selected_ = 0;
            }
            ImGui::EndChild();
        }

        // --- Ctrl+F is handled by the TextEditor's built-in find (works in
        // both inline and SBS mode). No App-level find bar needed. ---

        // --- Ctrl+< / Ctrl+> for Prev/Next change ---
        // On most layouts `<` is Shift+, and `>` is Shift+., so the user
        // holds Ctrl+Shift while pressing Comma/Period.
        {
            ImGuiIO& io = ImGui::GetIO();
            if (io.KeyCtrl && io.KeyShift) {
                if (ImGui::IsKeyPressed(ImGuiKey_Comma, false)) {
                    scroll_to_next_change(-1);
                } else if (ImGui::IsKeyPressed(ImGuiKey_Period, false)) {
                    scroll_to_next_change(1);
                }
            }
        }

        // --- Dockspace: hosts the dockable Files / Diff / Prompt windows.
        // Fills the remaining area below the toolbar + find bar. On first run
        // (no saved layout) seed_default_dock_layout reproduces the previous
        // fixed arrangement (Files left, Diff center, Prompt bottom). ---
        ImGuiID dockspace_id = ImGui::GetID("MainDockSpace");
        ImGui::DockSpace(dockspace_id, ImVec2(0, 0));
        seed_default_dock_layout(dockspace_id);  // no-op once a layout is saved

        // Build set of cued file paths for the yellow-dot badge (shared by Files).
        std::set<std::string> cued_files;
        for (const auto& c : cues_->cues()) {
            cued_files.insert(c.file.generic_string());
        }

        // --- Files (dockable file browser) ---
        if (ImGui::Begin("Files", nullptr, ImGuiWindowFlags_NoCollapse)) {
            if (file_tree_) {
                auto factions = ui::render_file_browser(*file_tree_, show_all_, cued_files,
                                                          current_open_path_.generic_string());
                if (factions.open_file) {
                    open_file(*factions.open_file);
                }
            } else {
                // First refresh hasn't landed yet — show a loading placeholder
                // instead of dereferencing a null tree.
                ImGui::TextDisabled("Loading…");
            }
        }
        ImGui::End();

        // --- Diff (dockable diff viewer). The title shows the current file's
        // relative path so the tab identifies what's being reviewed; the
        // "###Diff" suffix keeps the window ID (and dock position) stable
        // across file changes. Falls back to "Diff" when no file is open. ---
        std::string diff_title = current_open_path_.empty()
            ? std::string("Diff###Diff")
            : (current_open_path_.generic_string() + "###Diff");
        if (ImGui::Begin(diff_title.c_str(), nullptr, ImGuiWindowFlags_NoCollapse)) {
            auto dactions = diff_viewer_.render(*cues_);
            if (dactions.add_cue_requested && !dactions.add_cue_text.empty()) {
                if (dactions.edit_cue_index >= 0) {
                    // Editing an existing cue — remove the old one, add the new.
                    cues_->remove(dactions.edit_cue_index);
                }
                model::Cue cue;
                cue.file = current_open_path_;
                cue.side = dactions.add_cue_side;
                cue.line = dactions.add_cue_line;
                cue.text = dactions.add_cue_text;
                cues_->add(std::move(cue));
            }
            if (dactions.delete_cue_index >= 0) {
                cues_->remove(dactions.delete_cue_index);
            }
        }
        ImGui::End();

        // --- Prompt (dockable; shown when "Copy Prompt" is clicked) ---
        if (prompt_open_) {
            bool copy_pressed = prompt_panel_.render("Prompt", &prompt_open_, prompt_toast_ms_);
            if (copy_pressed) {
                platform::clipboard::copy_to_clipboard(prompt_panel_.get_text());
                prompt_toast_ms_ = 1500.0f;  // 1.5s toast
            }
        }
        if (prompt_toast_ms_ > 0.0f) {
            prompt_toast_ms_ -= static_cast<float>(ImGui::GetIO().DeltaTime * 1000.0f);
            if (prompt_toast_ms_ < 0.0f) prompt_toast_ms_ = 0.0f;
        }

        // --- Detect prefs changes and apply (theme/palette/mode) ---
        if (prefs_.app_theme != last_applied_theme_) {
            ui::apply_theme(prefs_.app_theme);
            last_applied_theme_ = prefs_.app_theme;
            model::save_prefs(prefs_);
        }
        if (prefs_.editor_palette != last_applied_palette_) {
            diff_viewer_.set_palette_by_name(prefs_.editor_palette);
            last_applied_palette_ = prefs_.editor_palette;
            model::save_prefs(prefs_);
        }
        if (prefs_.diff_mode != last_applied_mode_) {
            diff_viewer_.set_diff_mode(prefs_.diff_mode);
            last_applied_mode_ = prefs_.diff_mode;
            if (current_diff_) diff_viewer_.set_diff(*current_diff_, prefs_.diff_mode);
            model::save_prefs(prefs_);
        }

        // --- Cue list dialog (centered, keyboard-navigable) ---
        // Opened from the toolbar Cues button or the palette "List Cues".
        // Up/Down to move selection, Enter/Space to jump, Esc to close.
        if (cue_list_open_) {
            ImGuiViewport* vp2 = ImGui::GetMainViewport();
            ImGui::SetNextWindowPos(
                ImVec2(vp2->WorkPos.x + vp2->WorkSize.x * 0.5f,
                       vp2->WorkPos.y + vp2->WorkSize.y * 0.5f),
                ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_Appearing);
            ImGui::Begin("Cue List", &cue_list_open_,
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse);

            if (cues_->count() == 0) {
                ImGui::TextDisabled("No cues. Right-click a diff line to add one.");
            } else {
                ImGui::TextDisabled("%d cue(s) — Up/Down to navigate, Enter to jump, Esc to close",
                                    cues_->count());
                ImGui::Separator();

                if (cue_list_selected_ < 0) cue_list_selected_ = 0;
                if (cue_list_selected_ >= cues_->count())
                    cue_list_selected_ = cues_->count() - 1;

                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false) && cue_list_selected_ > 0)
                    --cue_list_selected_;
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false) &&
                    cue_list_selected_ < cues_->count() - 1)
                    ++cue_list_selected_;
                const bool do_jump = ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
                                     ImGui::IsKeyPressed(ImGuiKey_Space, false);

                for (int i = 0; i < cues_->count(); ++i) {
                    const auto& c = cues_->cues()[i];
                    char entry[512];
                    std::snprintf(entry, sizeof(entry), "%s:%d - %s",
                                  c.file.generic_string().c_str(), c.line, c.text.c_str());
                    ImGui::PushID(i);
                    bool is_selected = (i == cue_list_selected_);
                    if (ImGui::Selectable(entry, is_selected)) {
                        cue_list_selected_ = i;
                        open_file(c.file);
                        diff_viewer_.scroll_to_line(c.line);
                        cue_list_open_ = false;
                    }
                    if (is_selected && do_jump) {
                        open_file(c.file);
                        diff_viewer_.scroll_to_line(c.line);
                        cue_list_open_ = false;
                    }
                    if (is_selected && ImGui::IsWindowAppearing())
                        ImGui::SetKeyboardFocusHere(-1);
                    ImGui::PopID();
                }
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
                cue_list_open_ = false;
            ImGui::End();
        }

        ImGui::End();  // ##main
        window_.render();
    }
    return 0;
}

}  // namespace diffcue
