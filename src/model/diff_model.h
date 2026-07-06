// model/diff_model.h — per-file diff data (task 6.3).
//
// Wraps the old/new text + metadata for one file, plus the hunk list from
// ImGuiColorTextEdit::TextDiff. Caps at 5000 changed lines (design R7) with
// a `truncated` flag so huge files don't block the UI.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "model/file_meta.h"

namespace diffcue::model {

// One contiguous changed line range in the new file (1-based, inclusive).
// Used by the Prev/Next-change toolbar nav (task 8.8).
struct Hunk {
    int start_line = 0;
    int end_line = 0;
};

enum class DiffMode {
    SideBySide,
    Inline,
};

struct FileDiff {
    std::filesystem::path path;
    std::string old_text;
    std::string new_text;
    FileMeta old_meta;
    FileMeta new_meta;
    std::vector<Hunk> hunks;
    bool truncated = false;  // true if the diff exceeded the 5000-line cap
    bool binary = false;     // true if either side is binary
};

// Cap used per design R7. Files exceeding this many changed lines get the
// truncation banner instead of a full diff.
constexpr int kMaxChangedLines = 5000;

}  // namespace diffcue::model
