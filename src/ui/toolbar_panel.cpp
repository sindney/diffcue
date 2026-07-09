// ui/toolbar_panel.cpp — top toolbar (task 8.1, 8.8, 8.9).
#include "ui/toolbar_panel.h"

#include <cstdio>

#include "imgui.h"

namespace diffcue::ui {

namespace {

// Hover-dwell accumulator for the Cues button. When the mouse dwells on the
// button for ~300ms, the cue_dropdown popup opens (no click required). Resets
// to 0 whenever the button is unhovered. Single-instance toolbar → file-scope
// static is fine (per design D8).
float cue_hover_timer = 0.0f;

// One-frame grace period before the cue_dropdown closes on mouse-leave. Lets
// the mouse transit from the button into the popup without snapping it shut.
int cue_popup_grace = 1;

}  // namespace

ToolbarActions render_toolbar(const model::CueStore& cues,
                              model::Prefs& prefs,
                              bool find_bar_open,
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

    // Find toggle.
    if (find_bar_open) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.3f, 1.0f));
    if (ImGui::Button("Find")) actions.find_toggled = true;
    if (find_bar_open) ImGui::PopStyleColor();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Toggle find bar (Ctrl+F).");

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // Cue operations: count | clear | copy prompt.
    // The Cues button is always rendered (even at 0). Hovering it for ~300ms
    // opens the cue_dropdown popup; clicking opens it instantly. The popup
    // closes when the mouse leaves both the button and the popup body.
    //
    // The button hover check uses AllowWhenBlockedByPopup so that when the
    // popup is open the button STILL reports as hovered — without this, the
    // popup blocks the button's hover, the close-when-unhovered logic fires,
    // and the popup flashes open/closed every frame.
    char cue_label[64];
    std::snprintf(cue_label, sizeof(cue_label), "Cues: %d##cues", cues.count());
    if (ImGui::Button(cue_label)) {
        ImGui::OpenPopup("cue_dropdown");
        cue_hover_timer = 0.0f;
    }
    const bool cue_button_hovered =
        ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
    if (cue_button_hovered) {
        cue_hover_timer += ImGui::GetIO().DeltaTime;
    } else {
        cue_hover_timer = 0.0f;
    }
    // Hover-dwell opens the popup after ~300ms (OpenPopup is a no-op if the
    // popup is already open, so calling it while the dwell holds is safe).
    if (cue_button_hovered && cue_hover_timer >= 0.3f) {
        ImGui::OpenPopup("cue_dropdown");
    }
    if (ImGui::BeginPopup("cue_dropdown")) {
        // Empty state: a single disabled row for consistent feedback.
        if (cues.count() == 0) {
            ImGui::Selectable("No cues", false, ImGuiSelectableFlags_Disabled);
        } else {
            for (int i = 0; i < cues.count(); ++i) {
                const auto& c = cues.cues()[i];
                char entry[256];
                std::snprintf(entry, sizeof(entry), "%s:%d - %s",
                              c.file.generic_string().c_str(), c.line, c.text.c_str());
                if (ImGui::Selectable(entry)) {
                    actions.jump_to_cue_index = i;
                    ImGui::CloseCurrentPopup();
                }
            }
        }
        // Close-when-unhovered: if neither the button nor the popup body is
        // hovered this frame, close (after a one-frame grace period so the
        // mouse can transit from the button into the popup).
        const bool popup_hovered = ImGui::IsWindowHovered();
        if (!cue_button_hovered && !popup_hovered) {
            if (cue_popup_grace > 0) {
                --cue_popup_grace;
            } else {
                ImGui::CloseCurrentPopup();
            }
        } else {
            cue_popup_grace = 1;
        }
        ImGui::EndPopup();
    } else {
        // Popup closed — reset grace so the next open starts fresh.
        cue_popup_grace = 1;
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
