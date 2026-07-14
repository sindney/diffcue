// ui/command_palette.h — Cmd+P command palette (Sublime/VSCode-style).
//
// A keyboard-driven command list that filters and dispatches the SAME
// actions the toolbar and menubar expose. The palette returns a
// PaletteActions struct; App::run() merges it into the existing
// ToolbarActions / MenubarActions dispatch (no duplicated effect logic).
#pragma once

#include "model/cue_store.h"
#include "model/prefs.h"

namespace diffcue::ui {

enum class PaletteCommand {
    NextChange,
    PrevChange,
    OpenFolder,
    OpenRecent,      // payload = index into prefs.recent_folders
    Refresh,
    ClearCues,
    CopyPrompt,
    JumpToCue,       // payload = index into cues.cues()
    ListCues,        // open the cue list dialog
    ToggleDiffMode,
    ToggleIgnoreEOL,
    ShowAll,
    About,
    Quit,
};

struct PaletteActions {
    bool run = false;
    PaletteCommand command = PaletteCommand::NextChange;
    int payload = -1;  // index for OpenRecent / JumpToCue
};

// Render the command palette window. Returns true when a command is run this
// frame (out is populated). `open_flag` is the App-owned toggle so the
// function can close the palette (Esc / Enter / selection) by flipping it.
// When `open_flag` is false the palette does not render and returns false.
bool render_command_palette(PaletteActions& out,
                            bool& open_flag,
                            const model::Prefs& prefs,
                            const model::CueStore& cues);

}  // namespace diffcue::ui
