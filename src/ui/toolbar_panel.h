// ui/toolbar_panel.h — top toolbar (task 8.1).
#pragma once

#include <filesystem>
#include <string>

#include "model/cue_store.h"
#include "model/diff_model.h"
#include "model/prefs.h"

namespace diffcue::ui {

struct ToolbarActions {
    bool prev_change = false;
    bool next_change = false;
    bool copy_prompt = false;
    bool clear_cues = false;
    bool find_toggled = false;
    bool diff_mode_toggled = false;
    bool ignore_eol_toggled = false;
    bool refresh = false;
    int jump_to_cue_index = -1;
};

// Render the toolbar. `ignore_eol` is the current EOL-normalization state
// (passed from the diff viewer so the toggle button reflects it).
ToolbarActions render_toolbar(const model::CueStore& cues,
                              model::Prefs& prefs,
                              bool find_bar_open,
                              bool ignore_eol);

}  // namespace diffcue::ui
