// ui/diff_viewer_panel.h — TextDiff host + EOL/encoding header (task 8.3, 8.4).
#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>

#include "model/cue_store.h"
#include "model/diff_model.h"
#include "model/prefs.h"

namespace diffcue::ui {

struct DiffViewerActions {
    bool add_cue_requested = false;
    int add_cue_line = 0;
    model::Side add_cue_side = model::Side::New;
    std::string add_cue_text;
    // When editing an existing cue (right-click on a line that already has one),
    // this is the index of the cue being edited (-1 = new cue).
    int edit_cue_index = -1;
    // When the user clicks "Delete" in the cue popup.
    int delete_cue_index = -1;
};

// Owns a TextDiff instance. Call set_diff() when the open file changes,
// then render() every frame.
class DiffViewerPanel {
public:
    DiffViewerPanel();
    ~DiffViewerPanel();

    // Load a new file diff into the TextDiff. `diff` carries old/new text +
    // metadata. Call this whenever the open file changes.
    void set_diff(const model::FileDiff& diff, model::DiffMode mode);

    // Apply a palette change to the underlying TextDiff.
    void set_palette_by_name(const std::string& name);

    // Switch side-by-side ↔ inline (task 8.4).
    void set_diff_mode(model::DiffMode mode);

    // Scroll the diff to show `line` (1-based). Used by prev/next nav.
    void scroll_to_line(int line);

    // EOL normalization toggle (exposed for the toolbar checkbox).
    bool ignore_eol() const;
    void set_ignore_eol(bool v);

    // Clear the current diff (show empty editor).
    void clear();

    // Render the panel. `cues` is the cue store (for marker rendering).
    DiffViewerActions render(const model::CueStore& cues);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace diffcue::ui
