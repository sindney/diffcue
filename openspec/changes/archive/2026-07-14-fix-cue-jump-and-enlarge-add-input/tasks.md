## 1. Make the cue-list / prev-next / palette jump actually scroll the viewer

- [x] 1.1 In `src/ui/diff_viewer_panel.cpp` `Impl` struct, add `int pending_scroll_line_ = -1;` alongside the existing `cue_input_open`, `cue_text_buf`, etc. members.
- [x] 1.2 In `DiffViewerPanel::scroll_to_line(int line)` (currently `src/ui/diff_viewer_panel.cpp:174-177`), replace the body that calls `impl_->diff.ScrollToLine(line - 1, TextEditor::Scroll::alignMiddle)` with `impl_->pending_scroll_line_ = line;`. Keep the signature unchanged so `app.cpp:872` and the prev/next-change callers do not need to be edited.
- [x] 1.3 In `DiffViewerPanel::render`, BEFORE `impl_->diff.Render(...)` (the TextDiff opens its inner `BeginChild` inside `Render`), if `impl_->pending_scroll_line >= 0`, compute `target_y = max(0, (pending_scroll_line - 1) * line_h - visible_h * 0.5f + line_h * 0.5f)` using the current frame's `GetLineHeight()` and `GetContentRegionAvail().y`, call `ImGui::SetNextWindowScroll(ImVec2(0, target_y))` (applies to the TextDiff's inner `BeginChild`), then reset `pending_scroll_line = -1`. **Implementation note:** the original design proposed applying the scroll AFTER `diff.Render()` via `ImGui::SetScrollY` on the outer child, but the TextDiff's inner `BeginChild` owns the vertical scroll, so `SetScrollY` on the outer child would be a no-op. `SetNextWindowScroll` before `diff.Render()` is the correct ImGui pattern: it targets the next `BeginChild` (the TextDiff's), and uses the current frame's geometry (unlike the third-party `ScrollToLine` path, which uses stale `visibleLines`).
- [x] 1.4 ~~If `TextDiff::lineInfo` is not currently public, add the accessors / helper needed.~~ **Not needed.** The cue's `c.line` is set at `diff_viewer_panel.cpp:306` as `1 + (mouse_y - origin.y) / line_h`, which is the 1-based diff-view row index. `c.line - 1` IS the `lineInfo` index — no conversion needed. No third-party code changes required.
- [x] 1.5 Build (`cmake --build build --config Release` or the project's standard build command) and resolve any compile errors.

## 2. Verify the cue jump end-to-end

- [ ] 2.1 Manually verify the user's reported scenario: open a folder with at least two files that have cues, hover the `Cues: N` button to open the dropdown, click a cue whose file is NOT the currently-open file. The diff viewer must load the target file AND the cue's line must be visible in the viewport (not at the very top, not at the very bottom — centered). Repeat for a cue in the currently-open file whose line is past the first viewport.
- [ ] 2.2 Manually verify palette jumps: open Cmd+P / Ctrl+P, type the cue's filename or path, press Enter. Same behavior — file loads, line centered.
- [ ] 2.3 Manually verify prev/next change: click a file, then click `Prev` or `Next` in the toolbar. The diff viewer should center on the changed section, not collapse to the top.

## 3. Enlarge and scroll the add-cue popup's text input

- [x] 3.1 ~~In `src/ui/diff_viewer_panel.cpp` inside `BeginPopup("##cue_popup")` (around lines 328-387), restructure the per-row `PushItemWidth(80)` so it applies ONLY to the `Line##cue_line` `InputText`~~. **Already correct.** The existing code already scopes `PushItemWidth(80)` / `PopItemWidth()` to only the `Line##cue_line` input (lines 366-369); the multiline is rendered after the `PopItemWidth()`. No change needed.
- [x] 3.2 Resize the `InputTextMultiline("##cue_popup_text", ...)` from `ImVec2(380, 100)` to `ImVec2(std::min(ImGui::GetContentRegionAvail().x, 720.0f), 160.0f)`. The 720 px cap keeps the popup from overflowing narrow windows.
- [x] 3.3 Add `ImGuiInputTextFlags_WordWrap` to the existing `ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine` bitmask on the `InputTextMultiline` call. `WordWrap` is bitwise-OR-safe with the others. Keep the helper text `Ctrl+Enter for new line, Enter to submit` and the `Add` / `Cancel` / (when editing) `Delete` button row as they are.
- [x] 3.4 Build clean.

## 4. Verify the add-cue popup UX

- [ ] 4.1 Right-click a diff line, paste a 400-character single-line string into the text field. Confirm: text wraps to multiple visible rows, no horizontal scrollbar appears, the user can see all of the typed text without resizing the popup, pressing Enter submits (cue is stored verbatim, no inserted `\n`).
- [ ] 4.2 Right-click a diff line, paste 20 lines of text. Confirm: a vertical scrollbar appears on the text field, the user can scroll to reach the rest of the text, the popup's overall size does not balloon beyond the screen.
- [ ] 4.3 Confirm: `Line##cue_line` input still has a narrow width (it is a 1-4 digit number, 80 px is fine), the `Side: old/new` toggle still works, `Add` / `Cancel` / `Delete` (when editing an existing cue) still work and the `Ctrl+Enter` for new line / `Enter` to submit contract still holds.
- [ ] 4.4 Confirm: existing cues that were saved with a 2048-char buffer and no wrap still load and display correctly in the wider, wrap-enabled popup (backward compatibility).

## 5. Final integration verification

- [ ] 5.1 Walk through every scenario in `specs/review-cues/spec.md` (in the change's `specs/` directory) and confirm each behaves as written. Note any deviation in the change log.
- [x] 5.2 Run the existing test suite (`ctest` or the project's test command) and confirm no regressions in `test_cue_store.cpp` and any UI-adjacent tests. **Result:** 39/44 tests pass. The 5 failures are all pre-existing in `async_refresh` (3 tests) and `detect_meta` (2 tests) — unrelated to this change. All 4 cue-related tests pass.
- [x] 5.3 Final clean build.
