## Why

Two cue-popup bugs hurt the review flow:

1. **Cues-list jump is broken.** Clicking a cue in the toolbar's `cue_dropdown` (or the `Jump to Cue` entries in the command palette) calls `open_file(c.file)` then `diff_viewer_.scroll_to_line(c.line)`, but the viewer never actually scrolls to the cue's line — the file switches but the viewport stays where it was. The same path is used for palette jumps. This makes the cue list feel like a no-op, so reviewers stop using it.
2. **The add-cue popup is cramped.** Right-clicking a line opens a `380×100` `InputTextMultiline` for the cue text. The field is too small to read multi-line or long single-line content, and there is no hint of how to scroll it. Reviewers routinely copy long stack traces or paragraph-long context into cues; the current UI forces them to type blind.

Both are UX bugs in the existing `review-cues` capability, not new features, and they share the same code paths so they belong in one change.

## What Changes

- **Make cue-list jumps actually scroll the viewer.** The current `diff_viewer_.scroll_to_line(c.line)` call routes through `TextDiff::ScrollToLine`, which marks a request honored by `TextDiff::renderSideBySide` via `ImGui::SetNextWindowScroll`. Investigation shows `TextDiff::renderSideBySide` consumes `scrollToLineNumber` **before** it computes `visibleLines` (third-party `TextDiff.cpp` line ~217 vs ~267), so on the first frame after a file switch the `alignMiddle` formula collapses to `line * glyphSize.y` (no centering) and the scroll position lands at the very top of the new file — usually out of view of the cue. Rather than patch the third-party library, this change introduces a direct scroll path on `DiffViewerPanel` that records the target line and, on the next `render()`, calls `ImGui::SetScrollY(target_y)` against the diff viewer's outer `BeginChild` after geometry is known, so the centering math uses the correct, current-frame `visibleSize.y / line_h`. The existing `DiffViewerPanel::scroll_to_line(int line)` API and the `app.cpp` dispatch at lines 865–872 are updated to use the new path. `scroll_to_next_change` (which has the same bug for the same reason) is migrated to the same path so the fix is consistent.
- **Resize the add-cue input area and let it scroll.** Replace the hard-coded `ImVec2(380, 100)` with a size that fills the popup's available width and gives the multiline box enough vertical room to be useful (~6 visible lines / 160 px), and add `ImGuiInputTextFlags_WordWrap` so long single lines wrap rather than run off the right edge. The right-click popup continues to be inline (anchored to the click), but its content is reorganized so the multiline box uses the popup's full content width (no `PushItemWidth(80)` on the text field — only the `Line##cue_line` input keeps its narrow width) and the existing vertical scrollbar that `InputTextMultiline` already provides is reachable. The `Add` / `Cancel` / `Delete` button row stays in place; the helper text `Ctrl+Enter for new line, Enter to submit` stays underneath the text field.

## Capabilities

### New Capabilities
- (none)

### Modified Capabilities
- `review-cues`: The "Add a cue to a diff line" requirement's wording changes from "small inline text input" to a wording that gives the multiline field a useful, scrollable size and word-wrap. The "Cue list and jump-to-cue" requirement's jump-to-cue scenario is unchanged in intent but the implementation MUST actually scroll the viewer to the cue's line (today it is broken — file switches but viewport does not move). Existing side-and-line-stability semantics are unchanged.

## Impact

- **Code:**
  - `src/ui/diff_viewer_panel.h` / `.cpp` — add a `pending_scroll_line_` (int, -1 = none) member plus a small `Scroll` enum (or just an `int`) on `DiffViewerPanel`; `scroll_to_line(int line)` sets it; `render()` applies it via `ImGui::SetScrollY` on the outer diff viewer child *after* the TextDiff has rendered and the cursor geometry is known, so the math is stable. Resize the `InputTextMultiline` from `(380, 100)` to a popup-width / ~160 px height and add `ImGuiInputTextFlags_WordWrap`; drop the `PushItemWidth(80)` call from the multiline so the box fills the popup width.
  - `src/app/app.cpp` — no behavior change at lines 865–872 (still call `open_file(c.file)` + `diff_viewer_.scroll_to_line(c.line)`); the new DiffViewerPanel method is the only place the actual scroll is computed.
- **Specs:** `specs/review-cues/spec.md` — modify the "Add a cue to a diff line" and "Cue list and jump-to-cue" requirement blocks (delta spec, not a new capability).
- **Dependencies:** No new third-party deps. No spec for third-party `TextDiff` changes — we work around it.
- **Persistence / on-disk:** Unchanged.
- **Compatibility:** No protocol, file-format, or shortcut changes. The cue list `Cues: N` button / popup, command palette `Jump to Cue` entries, and right-click `Add cue` / `Edit cue` flows all keep their current shape.
