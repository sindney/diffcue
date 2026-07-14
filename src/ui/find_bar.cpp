// ui/find_bar.cpp — Ctrl+F find overlay (tasks 8.5, 8.6).
#include "ui/find_bar.h"

#include "imgui.h"

namespace diffcue::ui {

bool render_find_bar(FindBarState& state) {
    bool esc_pressed = false;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ImGui::BeginChild("find_bar", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::PopStyleVar();

    ImGui::Text("Find:");
    ImGui::SameLine();
    ImGui::PushItemWidth(300);
    bool enter_pressed = ImGui::InputText("##needle", state.needle, sizeof(state.needle),
                     ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();

    // Enter in the text box = Next (user-friendly: type, Enter, Enter, Enter...)
    if (enter_pressed) {
        state.next_pressed = true;
    }

    ImGui::SameLine();
    if (ImGui::Button("Next (F3)") || ImGui::IsKeyPressed(ImGuiKey_F3, false)) {
        state.next_pressed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Prev (Shift+F3)") ||
        (ImGui::IsKeyPressed(ImGuiKey_F3, false) && ImGui::GetIO().KeyShift)) {
        state.prev_pressed = true;
    }
    ImGui::SameLine();
    ImGui::Checkbox("Highlight all", &state.highlight_all);
    ImGui::SameLine();
    // "Aa" toggle for case sensitivity.
    bool aa = state.case_sensitive;
    if (ImGui::Checkbox("Aa", &aa)) state.case_sensitive = aa;
    ImGui::SameLine();
    if (ImGui::Button("Close (Esc)") || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        esc_pressed = true;
        state.open = false;
        state.needle[0] = '\0';
    }

    ImGui::EndChild();
    return esc_pressed;
}

}  // namespace diffcue::ui
