// ui/find_bar.h — Ctrl+F find overlay (tasks 8.5, 8.6).
#pragma once

#include <string>

namespace diffcue::ui {

struct FindBarState {
    bool open = false;
    char needle[256] = {};
    bool case_sensitive = false;
    bool highlight_all = false;
    // match navigation is driven by the App against the focused TextEditor;
    // the find bar just exposes next/prev button state.
    bool next_pressed = false;
    bool prev_pressed = false;
};

// Render the find bar overlay. Returns true if Esc was pressed (caller closes).
bool render_find_bar(FindBarState& state);

}  // namespace diffcue::ui
