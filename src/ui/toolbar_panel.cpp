// ui/toolbar_panel.cpp — top toolbar (task 8.1, 8.8, 8.9).
#include "ui/toolbar_panel.h"

#include <cstdio>

#include "imgui.h"

namespace diffcue::ui {

ToolbarActions render_toolbar(const model::CueStore& cues,
                              model::Prefs& prefs,
                              bool ignore_eol,
                              bool is_refreshing) {
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
    // Refresh: while a refresh is in flight on the background worker, show
    // "Refreshing..." and disable the button so it can't be re-clicked
    // mid-refresh.
    const char* refresh_label = is_refreshing ? "Refreshing...##r" : "Refresh##r";
    if (is_refreshing) ImGui::BeginDisabled();
    if (ImGui::Button(refresh_label)) actions.refresh = true;
    if (is_refreshing) ImGui::EndDisabled();
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

    // Cue operations: count | clear | copy prompt.
    // The Cues button opens a centered cue-list dialog (handled in App::run).
    // No hover popup — the dialog is keyboard-navigable (Up/Down/Enter/Esc)
    // and stays open until the user picks a cue or presses Esc.
    char cue_label[64];
    std::snprintf(cue_label, sizeof(cue_label), "Cues: %d##cues", cues.count());
    if (ImGui::Button(cue_label)) {
        actions.open_cue_list = true;
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Open the cue list dialog.\n"
                          "Arrow keys to navigate, Enter to jump, Esc to close.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) actions.clear_cues = true;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove all cues.");
    ImGui::SameLine();
    if (ImGui::Button("Copy Prompt")) actions.copy_prompt = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Build a prompt from all cues and copy to clipboard.\nShortcut: Ctrl+Shift+P");
    }

    return actions;
}

}  // namespace diffcue::ui
