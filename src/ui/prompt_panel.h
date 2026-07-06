// ui/prompt_panel.h — read-only prompt editor + copy button (task 8.7).
#pragma once

#include <memory>
#include <string>

namespace diffcue::ui {

// Owns a read-only TextEditor showing the generated prompt. The App fills it
// via set_text() and the user can trim before copying. Copy button puts the
// current text on the clipboard (App wires the actual clipboard call).
class PromptPanel {
public:
    PromptPanel();
    ~PromptPanel();

    // Set the prompt text shown in the editor.
    void set_text(const std::string& text);

    // Get the current editor text (post any user edits).
    std::string get_text() const;

    // Render the panel. Returns true when the "Copy to Clipboard" button is
    // pressed (caller calls platform::clipboard::copy_to_clipboard).
    // `toast_remaining_ms` drives the transient "Copied!" toast.
    bool render(const char* title, bool* p_open, float toast_remaining_ms);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace diffcue::ui
