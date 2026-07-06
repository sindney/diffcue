// ui/file_browser_panel.cpp — left-panel file tree (task 8.2).
#include "ui/file_browser_panel.h"

#include <cstdio>

#include "imgui.h"

namespace diffcue::ui {

namespace {

ImVec4 status_color(diffcue::git::FileStatus s) {
    switch (s) {
        case diffcue::git::FileStatus::Modified:  return ImVec4(0.9f, 0.7f, 0.2f, 1.0f);
        case diffcue::git::FileStatus::Added:     return ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
        case diffcue::git::FileStatus::Deleted:   return ImVec4(0.9f, 0.3f, 0.3f, 1.0f);
        case diffcue::git::FileStatus::Renamed:   return ImVec4(0.5f, 0.6f, 0.9f, 1.0f);
        case diffcue::git::FileStatus::Untracked: return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);
        default:                                   return ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
    }
}

const char* status_code(diffcue::git::FileStatus s) {
    switch (s) {
        case diffcue::git::FileStatus::Modified:  return "M";
        case diffcue::git::FileStatus::Added:     return "A";
        case diffcue::git::FileStatus::Deleted:   return "D";
        case diffcue::git::FileStatus::Renamed:   return "R";
        case diffcue::git::FileStatus::Untracked: return "U";
        default: return " ";
    }
}

// expand(root, target_file) — walk the tree from the root following the
// path components of `target_file`, collecting the relpath of every
// directory on the path. The returned set is the exact set of directories
// that must be open for the target file to be visible in the tree view.
//
// Returns an empty set when `target_file` is empty or not found under root.
std::set<std::string> expand_to_target(const model::FileTreeNode& root,
                                       const std::string& target_file) {
    std::set<std::string> result;
    if (target_file.empty()) return result;

    // Split target_file into path segments on '/'.
    std::vector<std::string> segs;
    size_t start = 0;
    for (;;) {
        size_t sep = target_file.find('/', start);
        if (sep == std::string::npos) {
            segs.push_back(target_file.substr(start));
            break;
        }
        segs.push_back(target_file.substr(start, sep - start));
        start = sep + 1;
    }
    // Drop the trailing filename segment — we only need directories.
    if (segs.size() <= 1) return result;  // target is a top-level file, no dirs to expand
    segs.pop_back();

    // Walk down the tree following the segments.
    const model::FileTreeNode* cur = &root;
    for (const auto& seg : segs) {
        const model::FileTreeNode* next = nullptr;
        for (const auto& c : cur->children) {
            if (c->is_dir && c->name == seg) {
                next = c.get();
                break;
            }
        }
        if (!next) return result;  // path not found in tree — give up silently
        result.insert(next->relpath.generic_string());
        cur = next;
    }
    return result;
}

void render_node(const model::FileTreeNode& node, FileBrowserActions& actions,
                 bool show_all, int depth, const std::set<std::string>& cued_files,
                 const std::string& current_file,
                 const std::set<std::string>& dirs_to_expand) {
    if (node.name.empty() && depth == 0) {
        for (const auto& c : node.children)
            render_node(*c, actions, show_all, depth, cued_files, current_file, dirs_to_expand);
        return;
    }

    if (!show_all && node.is_dir && node.changed_file_count == 0) return;
    if (!show_all && !node.is_dir && node.status == diffcue::git::FileStatus::Clean) return;

    ImGui::PushID(node.relpath.generic_string().c_str());
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Indent(static_cast<float>(depth * 16));

    // Yellow dot for files/folders that have cues.
    bool has_cue = false;
    if (!node.is_dir) {
        has_cue = cued_files.count(node.relpath.generic_string()) > 0;
    } else {
        std::string prefix = node.relpath.generic_string() + "/";
        for (const auto& cf : cued_files) {
            if (cf.rfind(prefix, 0) == 0) { has_cue = true; break; }
        }
    }
    if (has_cue) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.85f, 0.0f, 1.0f));
        ImGui::TextUnformatted("\xe2\x97\x8f");  // U+25CF BLACK CIRCLE
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    if (!node.is_dir) {
        // File: render status code + name as a single selectable row.
        bool is_current = (node.relpath.generic_string() == current_file);
        std::string label = std::string(status_code(node.status)) + "  " + node.name;
        if (is_current) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.5f, 0.8f, 0.6f));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.3f, 0.5f, 0.8f, 0.8f));
        }
        ImGui::PushStyleColor(ImGuiCol_Text, status_color(node.status));
        if (ImGui::Selectable(label.c_str(), is_current, ImGuiSelectableFlags_SpanAllColumns)) {
            actions.open_file = node.relpath;
        }
        ImGui::PopStyleColor();
        if (is_current) {
            ImGui::PopStyleColor(2);
        }
    } else {
        // Directory: render the [D] badge and count first, then set the open
        // state and call TreeNodeEx. SetNextItemOpen MUST be the last call
        // before TreeNodeEx — any widget that calls ItemAdd (including
        // TextUnformatted) between them will clear the NextItemData flag
        // and silently swallow the expand request.
        ImGui::PushStyleColor(ImGuiCol_Text, status_color(node.status));
        ImGui::TextUnformatted("[D]");
        ImGui::PopStyleColor();
        ImGui::SameLine();

        bool should_expand = dirs_to_expand.count(node.relpath.generic_string()) > 0;
        if (should_expand) {
            ImGui::SetNextItemOpen(true);
        }
        bool open = ImGui::TreeNodeEx(node.name.c_str(), 0);
        if (node.changed_file_count > 0) {
            ImGui::SameLine();
            ImGui::TextDisabled("(%d)", node.changed_file_count);
        }
        if (open) {
            for (const auto& c : node.children)
                render_node(*c, actions, show_all, depth + 1, cued_files, current_file, dirs_to_expand);
            ImGui::TreePop();
        }
    }

    ImGui::Unindent(static_cast<float>(depth * 16));
    ImGui::PopID();
}

}  // namespace

FileBrowserActions render_file_browser(const model::FileTreeNode& root, bool show_all,
                                       const std::set<std::string>& cued_files,
                                       const std::string& current_file) {
    FileBrowserActions actions;

    // Detect if the current file changed since last frame (next/prev or click).
    // If so, compute the set of directories that must be expanded to make the
    // target file visible. This set is computed once per frame at the top level
    // (NOT inside the recursive render_node) so the result isn't affected by
    // recursion order. On subsequent frames the set is empty and the user has
    // full control — SetNextItemOpen is not called, so ImGui uses the persisted
    // open/collapsed state from its tree storage.
    static std::string prev_file;
    bool file_changed = (current_file != prev_file);
    prev_file = current_file;

    std::set<std::string> dirs_to_expand;
    if (file_changed) {
        dirs_to_expand = expand_to_target(root, current_file);
    }

    ImGui::BeginChild("file_browser", ImVec2(0, 0), ImGuiChildFlags_Borders);

    if (root.children.empty()) {
        ImGui::TextDisabled("No changes in this folder.");
        ImGui::TextDisabled("Open a git working tree to review.");
    } else {
        if (ImGui::BeginTable("file_tree", 1, ImGuiTableFlags_RowBg)) {
            render_node(root, actions, show_all, 0, cued_files, current_file, dirs_to_expand);
            ImGui::EndTable();
        }
    }

    ImGui::EndChild();
    return actions;
}

}  // namespace diffcue::ui
