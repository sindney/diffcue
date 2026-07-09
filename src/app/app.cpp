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
#include "ui/file_browser_panel.h"
#include "ui/menubar.h"
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

    // Set window title to include the folder path.
    window_.set_title("diffcue - " + folder_.generic_string());

    // Probe git (task 5.5).
    if (!git::git_available()) {
        git_missing_ = true;
        return;
    }

    // Load prefs + cues + file tree (task 9.3).
    prefs_ = model::load_prefs();
    cues_.emplace(folder_);
    refresh_git_status();

    // Check if the folder is a git repo.
    not_git_repo_ = !git::is_repo(folder_);

    // Apply theme + palette + diff mode on startup (task 7.6).
    apply_prefs();
}

App::~App() {
    platform::file_dialog::quit();
}

void App::refresh_git_status() {
    entries_ = git::list_changes(folder_);
    file_tree_ = model::build_file_tree(entries_);
    git::clear_blob_cache();
    collect_all_hunks();

    // Refresh stale cues (task 9.6).
    if (cues_) {
        cues_->refresh_stale([this](const std::filesystem::path& file, model::Side) -> int {
            // Return line count of the new-side file, or 0 if unknown.
            auto it = std::find_if(entries_.begin(), entries_.end(),
                [&](const git::GitEntry& e) { return e.relpath == file; });
            if (it == entries_.end()) return 0;
            std::string text = git::read_blob_new(folder_, file);
            int lines = 0;
            for (char c : text) if (c == '\n') ++lines;
            if (!text.empty() && text.back() != '\n') ++lines;
            return lines;
        });
    }
}

model::FileDiff App::build_file_diff(const std::filesystem::path& relpath) {
    model::FileDiff fd;
    fd.path = relpath;

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
    int changed = 0;
    for (char c : fd.new_text) if (c == '\n') ++changed;
    if (changed > model::kMaxChangedLines) fd.truncated = true;
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
            folder_ = std::filesystem::canonical(*g_dropped_folder, ec);
            window_.set_title("diffcue - " + folder_.generic_string());
            prefs_ = model::load_prefs();
            cues_.emplace(folder_);
            git::clear_blob_cache();
            refresh_git_status();
            current_open_path_.clear();
            current_diff_.reset();
            diff_viewer_.clear();
            not_git_repo_ = !git::is_repo(folder_);
            apply_prefs();
            g_dropped_folder.reset();
        }

        // --- Re-scan on window focus regain (files may have changed) ---
        if (g_window_focused.exchange(false, std::memory_order_relaxed)) {
            if (!not_git_repo_) {
                refresh_git_status();
            }
        }

        // --- Menubar ---
        auto mactions = ui::render_menubar(prefs_);
        if (mactions.quit_clicked) window_.request_close();
        if (mactions.show_all_toggled) show_all_ = mactions.show_all;

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
                folder_ = std::filesystem::canonical(*picked, ec);
                window_.set_title("diffcue - " + folder_.generic_string());
                prefs_ = model::load_prefs();
                cues_.emplace(folder_);
                git::clear_blob_cache();
                refresh_git_status();
                current_open_path_.clear();
                current_diff_.reset();
                diff_viewer_.clear();
                not_git_repo_ = !git::is_repo(folder_);
                apply_prefs();
            }
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
            if (ImGui::Button("OK")) {
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
            auto tactions = ui::render_toolbar(*cues_, prefs_, find_bar_.open,
                                               diff_viewer_.ignore_eol());
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
                if (ImGui::Button("Clear")) {
                    cues_->clear();
                    ImGui::CloseCurrentPopup();
                }
                ImGui::SameLine();
                if (ImGui::Button("Cancel")) {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }
            if (tactions.find_toggled) find_bar_.open = !find_bar_.open;
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
            }
            ImGui::EndChild();
        }

        // --- Ctrl+F (task 8.6) ---
        if (ImGui::IsKeyPressed(ImGuiKey_F, false) && ImGui::GetIO().KeyCtrl) {
            find_bar_.open = !find_bar_.open;
        }

        // --- Ctrl+P to open the Copy Prompt panel ---
        if (ImGui::IsKeyPressed(ImGuiKey_P, false) && ImGui::GetIO().KeyCtrl) {
            std::string prompt = model::build_prompt(*cues_);
            prompt_panel_.set_text(prompt);
            prompt_open_ = true;
        }

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

        // --- Find bar ---
        if (find_bar_.open) {
            ImGui::BeginChild("findbar", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
            ui::render_find_bar(find_bar_);
            ImGui::EndChild();
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
            auto factions = ui::render_file_browser(*file_tree_, show_all_, cued_files,
                                                      current_open_path_.generic_string());
            if (factions.open_file) {
                open_file(*factions.open_file);
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

        ImGui::End();  // ##main
        window_.render();
    }
    return 0;
}

}  // namespace diffcue
