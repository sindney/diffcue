// ui/menubar.h — File/View/Help menubar (task 7.3, 7.4, 7.5).
//
// Renders the app menubar. Selections that need App-level action (open folder,
// quit, show-all toggle) are surfaced via the returned MenubarActions struct;
// theme/palette/diff-mode selections are written straight into `prefs`.
#pragma once

#include "model/prefs.h"

namespace diffcue::ui {

struct MenubarActions {
    bool open_folder_clicked = false;
    bool quit_clicked = false;
    bool show_all_toggled = false;  // value is in `show_all` below
    bool show_all = false;
    bool about_clicked = false;
    bool open_palette_clicked = false;  // Cmd+P command palette
    // Index into prefs.recent_folders; -1 = no selection this frame.
    int open_recent_index = -1;
    bool clear_recent = false;  // Clear the recent folders list
};

// Render the menubar. `prefs` is read for current selections and updated
// in place when the user picks a new theme/palette/diff-mode. Returns the
// set of discrete actions the App should react to this frame.
MenubarActions render_menubar(model::Prefs& prefs);

}  // namespace diffcue::ui
