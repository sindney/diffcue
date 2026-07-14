## Context

diffcue is a Qt/ImGui-based diff viewer. The cues feature (in `src/model/cue_store.{h,cpp}`) lets reviewers attach short per-line comments. Cues are listed in a `cue_dropdown` popup anchored to the toolbar's `Cues: N` button (`src/ui/toolbar_panel.cpp`) and via the command palette's `Jump to Cue` entries (`src/ui/command_palette.cpp`); both code paths set `ToolbarActions::jump_to_cue_index = i`, which `App::run()` (`src/app/app.cpp:865-873`) dispatches by calling `open_file(c.file)` then `diff_viewer_.scroll_to_line(c.line)`. The cue popup is opened with `BeginPopup` and an item is `Selectable` per cue (`toolbar_panel.cpp:131-141`).

The `diff_viewer_.scroll_to_line(int line)` API is a one-liner (`src/ui/diff_viewer_panel.cpp:174-177`) that calls `impl_->diff.ScrollToLine(line - 1, TextEditor::Scroll::alignMiddle)`. `DiffViewerPanel::render` (`src/ui/diff_viewer_panel.cpp:209-392`) owns the `TextDiff` (`thirdparty/ImGuiColorTextEdit/TextDiff.h`); `TextDiff` extends `TextEditor` but overrides `Render()` with a custom side-by-side path.

The right-click "Add cue" popup lives inside `DiffViewerPanel::render` (`src/ui/diff_viewer_panel.cpp:299-387`). It opens `BeginPopup("##cue_popup")` on right-click in the diff viewer and renders a fixed `ImVec2(380, 100)` `InputTextMultiline` for the cue text plus a `Line##cue_line` numeric input (80 px wide via `PushItemWidth(80)`), a `Side: old/new` toggle button, and `Add` / `Cancel` / (when editing) `Delete` buttons. The buffer `cue_text_buf[2048]` is large but the visible window is tiny and lacks a horizontal scrollbar or word-wrap, so long single lines and multi-paragraph content are unreadable.

## Goals / Non-Goals

**Goals:**
- Make clicking a cue in `cue_dropdown` (or selecting `Jump to Cue: ...` in the palette) actually scroll the diff viewer to the cue's line, in the correct file, every time. The scroll must be correct on the first frame after the file switches (the file switch and the scroll happen in the same frame).
- Make the right-click `Add cue` / `Edit cue` popup's text-input area large enough to be readable for long single-line content and small multi-line content, with a reachable vertical scrollbar and a usable width.
- Keep the existing UX intact: cue list is still a `BeginPopup`, palette still uses the same `jump_to_cue_index` flag, the right-click popup is still anchored to the click and uses the same submit / cancel / delete semantics.

**Non-Goals:**
- No new third-party dependencies and no patches to the third-party `TextDiff` / `TextEditor` code.
- No changes to the cue JSON sidecar format, no new keyboard shortcuts, no changes to the toolbar button, the popup hover-dwell logic, or the cue-marker sidebar.
- No spec change for "Cue side and line stability" or the "Clear all cues confirmation" or "Per-file cue marker rendering" requirements — only "Add a cue" and "Cue list and jump-to-cue" change.
- No fuzzy search, no multi-cue selection, no cue history, no inline preview of long text in the dropdown row.

## Decisions

### D1: Drive the jump-to-cue scroll from `DiffViewerPanel::render` against the outer child, not via `TextDiff::ScrollToLine`.

**Why:** `TextDiff::renderSideBySide` (`thirdparty/ImGuiColorTextEdit/TextDiff.cpp:217-235`) consumes `scrollToLineNumber` *before* it computes `visibleLines` (line 267). On the first frame after `set_diff`, `visibleLines` is the previous frame's value (or 0), so the `alignMiddle` formula `max(0, (line - visibleLines/2) * glyphSize.y)` collapses to roughly `line * glyphSize.y` and the scroll lands at the very top of the new file — usually out of view of the cue. Patching the third-party `TextDiff` is out of scope. The fix: record the target line on `DiffViewerPanel`, and in `render`, *after* `impl_->diff.Render(...)` returns and `origin` / `line_h` are computed, call `ImGui::SetScrollY(target_y)` on the outer `BeginChild("diff_viewer", ...)`. This:
1. Uses the current frame's `visibleSize` and `line_h` (not a stale or zero value).
2. Targets the diff viewer's outer scroll container, which is what the user actually sees and what carries the vertical scrollbar.
3. Trivially co-exists with the existing `scrollToLineNumber` / `TextDiff` flow — we don't disable or change the third-party scroll path, we just provide a working one for the cue-jump and prev/next-change use cases.

**How:** Add `int pending_scroll_line_ = -1;` to `DiffViewerPanel::Impl` (in `diff_viewer_panel.cpp`). Keep the existing public `scroll_to_line(int line)` signature (so `app.cpp:872` and the prev/next-change handlers don't have to change), and have it set `pending_scroll_line_ = line` *instead of* calling `impl_->diff.ScrollToLine(...)`. In `DiffViewerPanel::render`, after `impl_->diff.Render(...)` runs and `diff_h = ImGui::GetItemRectSize().y` is known, if `pending_scroll_line_ >= 0`, compute `target_y = max(0, (pending_scroll_line_ - 1) * line_h - diff_h * 0.5f + line_h * 0.5f)` (centered, clamped to `>= 0`) and call `ImGui::SetScrollY(target_y)`, then reset `pending_scroll_line_ = -1`. The `+ 0.5f * line_h` term aligns the target line at the viewport center rather than at the viewport top edge. `SetScrollY` operates on the *current* scroll target, which at that point in the render is the `BeginChild("diff_viewer", ...)` (since the code is inside that child block before `EndChild` at line 390). After the scroll is applied, the `SetScrollY` call mutates the current scroll position; the next frame's `render` will see the new position and re-center if needed (no, it won't — `pending_scroll_line_` is reset).

**Why not also disable `TextDiff::ScrollToLine`:** we keep the third-party call site intact in case future code wants it; the cue and prev/next paths bypass it.

**Why not do the scroll from `app.cpp` directly:** `app.cpp` doesn't have access to the inner `BeginChild` cursor / line-height geometry; doing it from `DiffViewerPanel::render` keeps the geometry concern local.

### D2: Resize and word-wrap the multiline input in the right-click popup.

**Why:** the current `ImVec2(380, 100)` is too small for any meaningful cue text. 380 px is also less than the popup's natural width — the popup has a `Line##cue_line` `InputText` (80 px) and a `Side: old/new` button on one row, then the multiline below, all inside the same `BeginPopup`. There is plenty of horizontal room we are not using. The vertical 100 px shows ~4 lines at the default font size, which is fine for short cues but unusable for long stack traces / paragraphs.

**How:** in `diff_viewer_panel.cpp` around line 352-357, change the `InputTextMultiline` size to a popup-width / ~160-px tall field. The popup does not call `SetNextWindowSize`, so the popup auto-sizes to its contents; widening the multiline will widen the popup, which is what we want. The size argument to `InputTextMultiline` is just a *visible* window; if the user types more, the vertical scrollbar ImGui already provides becomes visible. Concretely:
- Width: use `ImGui::GetContentRegionAvail().x` so the box spans whatever the popup gives it (the popup's content area).
- Height: `160.0f` (≈6 lines at default font, comfortable for typical cues, still compact).
- Add `ImGuiInputTextFlags_WordWrap` to the existing `EnterReturnsTrue | CtrlEnterForNewLine` flag bitmask. With word-wrap on, a long single line that exceeds the visible width wraps onto the next visual line and `Ctrl+Enter` is still required to insert a literal `\n`; this matches the existing helper text "Ctrl+Enter for new line, Enter to submit".
- Drop the `PushItemWidth(80) / PopItemWidth` block from around the multiline call (the `80` is currently around *both* the line number and the multiline because they share a `PushItemWidth` scope; the multiline should fill the popup width and the line number keeps its narrow width via a dedicated `PushItemWidth(80)` *before only* the line-number `InputText`).

**Why not a fixed `SetNextWindowSize` on the popup:** anchoring the popup to the click point and using the click's screen position as a hint, the popup can grow to the right and downward; the existing `ImGui::OpenPopup("##cue_popup")` after right-click lets the popup auto-place. We do not need to call `SetNextWindowSize` unless we want a minimum width; we let the multiline dictate the size and the popup's other rows (line / side / buttons) follow.

**Why not move to a non-inline popup (a centered modal):** a modal would lose the line-anchored "Add cue here" affordance. The current inline `BeginPopup` is the right shape; the only complaint is the size of the multiline.

### D3: Keep `app.cpp` and `toolbar_panel.cpp` mostly unchanged.

**Why:** the existing dispatch in `app.cpp:865-873` already does the right thing at the policy level (`open_file(c.file)` then `diff_viewer_.scroll_to_line(c.line)`); the only thing wrong is what `scroll_to_line` does. By keeping the public API (`scroll_to_line(int line)`) unchanged and just changing its body, the only line in `app.cpp` that touches this is unchanged. The toolbar's cue dropdown (`toolbar_panel.cpp:131-141`) and the palette's `Jump to Cue` (`command_palette.cpp:73-86`) already set the same `jump_to_cue_index` flag with the same index → cue mapping; they need no change. The prev/next-change code path in `app.cpp` (`scroll_to_next_change`, called from lines 416, 504, 519, 524, 527, 531, 533, 889, 891) already calls `diff_viewer_.scroll_to_line(...)` and will benefit from the same fix for free.

### D4: Apply the scroll *after* `diff.Render` returns, not before, so the geometry is current-frame.

**Why:** `diff.Render` populates `GetLastRenderOrigin()` and `GetLineHeight()` for the current frame (see `diff_viewer_panel.cpp:263-264`). The `textScroll` and firstVisibleLine values inside `TextDiff` are also current-frame after `Render` returns. Doing `ImGui::SetScrollY` after `Render` is the standard ImGui pattern for "user invoked an action, geometry is now stable, apply the resulting scroll". Doing it before `Render` would race with the diff's own internal scroll state, which is why the third-party `ScrollToLine` path is fragile (it sets a request that `Render` honors, but `Render`'s own order bug eats the centering math).

**Why not also use `ImGui::SetScrollHereY`:** that requires an item to anchor on; we have a line index, not an item, so a direct `SetScrollY(target_y)` is the cleanest.

## Risks / Trade-offs

- **Scroll might over/undershoot if the diff has heavily deleted lines** → The `c.line` is the line in the *new* file (set by the right-click code at `diff_viewer_panel.cpp:306` and the cue's `side` field), but the rendered TextDiff interleaves added / deleted / common lines, so `lineInfo.size()` is not the same as the new file's line count. Mitigation: convert `c.line` from new-file line to a TextDiff `lineInfo` index before computing `target_y`. The existing `c.side` field lets us disambiguate: if `c.side == Side::New`, walk the `cues`'s `line` and the TextDiff's `lineInfo` to find the diff-view index whose `rightLine + 1 == c.line`. If we don't want to add the conversion here (it's a bigger change), the simpler mitigation is to record both the cue's `line` and the diff-view index at cue-creation time and add a `lineinfo_index` field to `Cue`. Decision: keep the existing single-`line` field and do the side-aware `lineInfo` lookup at scroll time (add a helper on `DiffViewerPanel` that takes a `Cue` and returns the correct `lineInfo` index, or returns the original `line - 1` as a fallback for files where the indexing coincides). For the common case the user showed in the screenshot (a file with mostly added lines — `+ 5 + 1 = 6` rows of additions in a row, no deletions), `rightLine + 1 == line` exactly, so the lookup is a no-op. Verify with the existing tests and the manual scenario in task 1.
- **Prev/Next change paths may shift behavior** → they go through the same `scroll_to_line`, so they get the fix for free, but the centering point changes from "top-aligned" to "centered". Acceptable; the change is a strict improvement and is what the spec already says ("scroll the viewer so the cue's line is centered"). If users prefer top-aligned, we can switch to `alignTop` by setting `target_y = max(0, (pending_scroll_line_ - 1) * line_h)` instead.
- **`SetScrollY` is on the *current* window** → at the point we call it, the current window is the `BeginChild("diff_viewer", ...)` (line 212). That is the outer scroll container whose scroll position the user actually sees. Confirmed by reading `diff_viewer_panel.cpp:209-390`.
- **The right-click popup auto-grows wider, which may push the cue title / file path off the right edge of the screen on small windows** → Mitigation: cap the multiline width at a sensible max (e.g., `min(ImGui::GetContentRegionAvail().x, 720.0f)`) so very wide popups don't overflow. The title row stays on one line and ImGui's auto-wrap on `TextDisabled` will handle long paths.
- **`InputTextMultiline` with `WordWrap` and the existing `EnterReturnsTrue | CtrlEnterForNewLine` flags together** → the flag is `ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CtrlEnterForNewLine | ImGuiInputTextFlags_WordWrap`. Confirmed `ImGuiInputTextFlags_WordWrap` exists in imgui.h and is bitwise-OR-safe with the others.
- **Touching the third-party's `TextDiff` scrollToLineNumber is intentionally avoided** to keep the change self-contained.

## Migration Plan

No data migration. No protocol changes. No on-disk state changes. The change is a code-only fix in `src/ui/diff_viewer_panel.{h,cpp}` (and a small wording update in `openspec/specs/review-cues/spec.md`). Rollback is `git revert` of the change.

## Open Questions

- Should the cue popup also pre-fill with the diff line's text as a starting point for the cue? (E.g., user right-clicks a `// TODO` line; the cue starts with `// TODO`. The current spec says "the inline text input" only.) Default: no, keep this change scoped to the size + scroll fix; pre-fill is a separate UX request.
- Should `pending_scroll_line_` survive across frames so a window-resize that re-flows the diff honors it again? Default: no — once we apply the scroll, clear the request. Re-request on a new jump. The third-party path doesn't retry either.
- Should we expose a `Scroll::alignTop` option for the cue list so power users who like the cue at the top can opt in? Default: no; keep centering. Spec already says "centered".
