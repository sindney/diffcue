// ui/prompt_panel.cpp — read-only prompt editor + copy button (task 8.7).
#include "ui/prompt_panel.h"

#include <memory>

#include "imgui.h"
#include "TextEditor.h"

namespace diffcue::ui {

struct PromptPanel::Impl {
    TextEditor editor;
    bool toast_active = false;
    bool pending_focus = false;  // set by set_text(); consumed on next render
};

PromptPanel::PromptPanel() : impl_(std::make_unique<Impl>()) {
    impl_->editor.SetReadOnlyEnabled(false);  // editable so the user can trim before copy
    impl_->editor.SetShowWhitespacesEnabled(false);
}

PromptPanel::~PromptPanel() = default;

void PromptPanel::set_text(const std::string& text) {
    impl_->editor.SetText(text);
    // Request focus + select-all on the next render so the user can
    // immediately Ctrl+C (or trim with the keyboard) without clicking in.
    impl_->pending_focus = true;
}

std::string PromptPanel::get_text() const {
    return impl_->editor.GetText();
}

bool PromptPanel::render(const char* title, bool* p_open, float toast_remaining_ms) {
    bool copy_pressed = false;

    // Center the window on first appearance + use a larger default size.
    ImGui::SetNextWindowSize(ImVec2(820, 600), ImGuiCond_FirstUseEver);
    {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 center(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
                     vp->WorkPos.y + vp->WorkSize.y * 0.5f);
        ImGui::SetNextWindowPos(center, ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    }
    if (ImGui::Begin(title, p_open)) {
        // Esc closes the window (in addition to the X button).
        // Check root-or-child focus so the editor's child window also triggers.
        if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
            ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
            *p_open = false;
        }

        if (ImGui::Button("Copy to Clipboard")) {
            copy_pressed = true;
            impl_->toast_active = true;
        }
        if (impl_->toast_active && toast_remaining_ms > 0) {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.9f, 0.3f, 1.0f));
            ImGui::Text("Copied!");
            ImGui::PopStyleColor();
        } else {
            impl_->toast_active = false;
        }
        ImGui::Separator();

        // Grab focus + select all text when the panel freshly opens, so the
        // user can immediately Ctrl+C without clicking into the editor.
        // SetFocus() sets an internal flag consumed by the upcoming Render().
        if (impl_->pending_focus) {
            impl_->editor.SetFocus();
            impl_->editor.SelectAll();
            impl_->pending_focus = false;
        }
        impl_->editor.Render("##prompt_editor", ImVec2(0, 0), false);
    }
    ImGui::End();

    return copy_pressed;
}

}  // namespace diffcue::ui
