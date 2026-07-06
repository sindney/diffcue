// ui/toolbar_panel.cpp — top toolbar (task 8.1, 8.8, 8.9).
#include "ui/toolbar_panel.h"

#include <cstdio>

#include "imgui.h"

namespace diffcue::ui {

ToolbarActions render_toolbar(const model::CueStore& cues,
                              model::Prefs& prefs,
                              bool find_bar_open,
                              bool ignore_eol) {
    ToolbarActions actions;

    // Prev / Next change.
    if (ImGui::Button("Prev")) actions.prev_change = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Jump to previous changed section.\nShortcut: Ctrl+<");
    }
    ImGui::SameLine();
    if (ImGui::Button("Next")) actions.next_change = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Jump to next changed section.\nShortcut: Ctrl+>");
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) actions.refresh = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Re-scan git status and refresh the file tree.\n"
                          "Also happens automatically when you Alt-Tab back to diffcue.");
    }
    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Toggle buttons: Side by Side / Inline, Ignore EOL.
    bool sbs = (prefs.diff_mode == model::DiffMode::SideBySide);
    if (sbs) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
    if (ImGui::Button("Side by Side")) {
        prefs.diff_mode = model::DiffMode::SideBySide;
        actions.diff_mode_toggled = true;
    }
    if (sbs) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show old and new files side by side.");

    ImGui::SameLine();
    bool inline_mode = (prefs.diff_mode == model::DiffMode::Inline);
    if (inline_mode) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
    if (ImGui::Button("Inline")) {
        prefs.diff_mode = model::DiffMode::Inline;
        actions.diff_mode_toggled = true;
    }
    if (inline_mode) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show changes inline (unified view).");

    ImGui::SameLine();
    if (ignore_eol) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
    if (ImGui::Button("Ignore EOL")) {
        actions.ignore_eol_toggled = true;
    }
    if (ignore_eol) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Normalize line endings before diffing.");

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Find toggle.
    if (find_bar_open) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
    if (ImGui::Button("Find")) actions.find_toggled = true;
    if (find_bar_open) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle find bar (Ctrl+F).");

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Cue operations: count | clear | copy prompt.
    char cue_label[64];
    std::snprintf(cue_label, sizeof(cue_label), "Cues: %d##cues", cues.active_count());
    if (cues.count() > 0 && ImGui::Button(cue_label)) {
        ImGui::OpenPopup("cue_dropdown");
    }
    if (cues.count() > 0 && ImGui::IsItemHovered()) ImGui::SetTooltip("Click to list all cues.");
    if (ImGui::BeginPopup("cue_dropdown")) {
        for (int i = 0; i < cues.count(); ++i) {
            const auto& c = cues.cues()[i];
            char entry[256];
            std::snprintf(entry, sizeof(entry), "%s:%d - %s",
                          c.file.generic_string().c_str(), c.line, c.text.c_str());
            if (ImGui::Selectable(entry)) {
                actions.jump_to_cue_index = i;
            }
        }
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) actions.clear_cues = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove all cues.");
    ImGui::SameLine();
    if (ImGui::Button("Copy Prompt")) actions.copy_prompt = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Build a prompt from all cues and copy to clipboard.\nShortcut: Ctrl+P");
    }

    return actions;
}

}  // namespace diffcue::ui
