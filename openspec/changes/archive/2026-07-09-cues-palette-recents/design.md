## Context

diffcue's UI today is a menubar + toolbar + dockable workspace. The toolbar (`src/ui/toolbar_panel.cpp`) returns a `ToolbarActions` POD struct of bool/int flags per frame; the menubar (`src/ui/menubar.cpp`) returns a `MenubarActions` POD struct. `App::run()` (in `src/app/app.cpp`) is the single dispatcher: it reads both structs and runs the corresponding handler inline. Cues live in `model::CueStore`; user prefs live in `model::Prefs` (`<config_dir>/prefs.json`) parsed by a small hand-rolled JSON reader/writer (`src/model/prefs.cpp`) that today only handles three scalar fields (`app_theme`, `editor_palette`, `diff_mode`).

Four pain points motivate this change:

1. **Jump-to-cue is broken.** `app.cpp:559–562` calls `open_file(c.file)` and ignores `c.line`, so jumping switches file but never scrolls. The cue `line` is already 1-based end-to-end (verified at `cue_store.h:26`, `diff_viewer_panel.cpp:170–173`, `diff_viewer_panel.cpp:325–328`); the fix is one missing `diff_viewer_.scroll_to_line(c.line)` line.
2. **Cue list requires a click.** The button is gated on `cues.count() > 0` and the list opens via `ImGui::OpenPopup` on click; hover only shows a text tooltip. The user wants hover-to-show.
3. **Clear-cues modal needs keyboard.** No Enter/Esc handling; the user wants both, with hints in the button labels.
4. **No keyboard-driven command entry, no recent-folders memory.** The user wants a Cmd+P palette (Sublime/VSCode-style) and a File ▸ Open Recent submenu. Both MUST reuse the existing toolbar/menubar action code — no duplicated effect logic.

There is one conflict: Copy Prompt currently owns Cmd+P (`app.cpp:571–576`). The user has decided to rebind Copy Prompt to Cmd+Shift+P and give Cmd+P to the palette.

TextEditor ordering contract from `thirdparty/ImGuiColorTextEdit/TextEditor.h:197–204`: `SetText` (resets scroll) → `SetCursor` → `ScrollToLine` (cancels SetCursor's scroll) → `Render`. The jump fix preserves this order: `open_file` → `set_diff` → `SetText` runs first, the new `scroll_to_line` marks a request, and `Render` runs later in the frame.

## Goals / Non-Goals

**Goals:**
- Fix jump-to-cue so the cue's line is centered in the diff viewer.
- Make the cue list open on hover (with a dwell delay) and remove the text tooltip; always render the `Cues: N` button (including `Cues: 0`).
- Add Enter=confirm / Esc=cancel keyboard handling to the `Clear Cues?` modal, with shortcut hints in the button labels.
- Add `Open Recent` (up to 10 folders, most-recent-first, deduped) persisted in `prefs.json`; reachable from the menubar and the palette.
- Add a Cmd+P command palette that filters/dispatches the SAME actions as the toolbar and menubar.
- Rebind Copy Prompt to Cmd+Shift+P (frees Cmd+P for the palette).

**Non-Goals:**
- No fuzzy-matching scorer (SublimeTelemetry-style) — case-insensitive substring filter is sufficient for the command list size (~15–30 entries). Can be upgraded later without spec change.
- No palette plugin/extension system — the command list is compiled in.
- No cross-window state for the palette (no "recently run commands" recents list inside the palette).
- No per-cue editing from the palette beyond what the existing marker click already does.
- No new third-party deps. ImGui popups + a `Begin` window are sufficient.
- No spec change to the jump-to-cue wording — the existing spec already says "scroll to the cue's line"; only the implementation was missing.

## Decisions

### D1: Reuse the existing `cue_dropdown` popup, opened on hover-dwell, instead of a tooltip or a custom window.

**Why:** The popup body (`toolbar_panel.cpp:85–96`) already renders the selectable list. ImGui's `BeginPopup` machinery gives us closing-on-outside-click for free. A `SetTooltip` can't host `Selectable` widgets reliably. A manually positioned `Begin` window would re-implement popup behavior we already have.

**How:** Track hover state with a small dwell timer (≈300 ms) in `render_toolbar`. When the timer elapses, call `ImGui::OpenPopup("cue_dropdown")`. To make the popup hoverable (so the user can move the mouse into it without it closing), use `ImGui::BeginPopup` with no `NoMove`/`NoResize` flags and let ImGui's standard "popup closes when clicked outside" behavior handle dismissal. Because ImGui popups close on outside click but NOT on mere mouse-leave, we add a per-frame check: if neither the button nor the popup is hovered AND the popup is open, close it via `ImGui::CloseCurrentPopup()` (gated to avoid fighting ImGui's own close logic).

**Alternatives considered:**
- Always-open on hover with no dwell (instant). Rejected — feels jumpy when the user brushes the button moving to another toolbar control.
- Render the list in a `Begin` tooltip window. Rejected — duplicates popup logic and breaks keyboard nav.
- Keep click-to-open AND add hover-to-open. Rejected by the user ("directly show cue list when hover, not a tooltip").

**Empty case:** Always render the button (`Cues: 0`); hovering when empty opens the popup with a single disabled `Selectable("No cues", false, ImGuiSelectableFlags_Disabled)` row so the user gets consistent feedback.

### D2: Jump fix is a single `scroll_to_line(c.line)` call after `open_file(c.file)`.

**Why:** `scroll_to_next_change` at `app.cpp:331, 346, 353, 355` already proves this pattern. `scroll_to_line` is 1-based in, 0-based internally (`diff_viewer_panel.cpp:172`); cue `line` is 1-based end-to-end (`cue_store.h:26`). No indexing change. The `TextEditor` ordering contract is respected: `open_file` → `set_diff` → `SetText` runs in the same frame, `scroll_to_line` marks a request, `Render` runs later in the same frame at `app.cpp:631`.

**Why not also scroll the file browser tree:** the spec only requires the diff viewer to scroll; tree-scroll is out of scope (and `open_file` currently only highlights, per the research report).

### D3: Clear-cues modal — Enter/Esc via `ImGui::IsKeyPressed` in the modal block, shortcut hints baked into labels.

**Why:** The modal is already a `BeginPopupModal` (`app.cpp:537–549`). Adding `if (ImGui::IsKeyPressed(ImGuiKey_Enter)) { cues_->clear(); CloseCurrentPopup(); }` and `if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { CloseCurrentPopup(); }` inside the modal body is the minimal change. ImGui's default popup-Esc-close may already close it, but being explicit ensures the cancel path is unconditional and discoverable. Renaming buttons to `Clear (Enter)` and `Cancel (Esc)` is a one-line label change per button and satisfies the "show shortcuts in the button name" requirement. There's room in the modal (currently 440×180) so the longer labels fit.

**Why not use `ImGui::Button` with explicit shortcut flags / `MenuItem` shortcut hints:** `MenuItem` is for menus, not modals. Button label hints are the simplest approach and match the user's request ("you can show the shortcuts in the button name, there's space for that").

### D4: Refactor folder-open into a single `App::open_folder(path)` helper BEFORE adding recents / palette.

**Why:** The research report notes folder-open logic is copy-pasted 3× today (CLI init at `app.cpp:110–140`, menubar picker at `app.cpp:468–485`, drag-drop at `app.cpp:431–445`). Adding `Open Recent` and a palette `Open Folder`/recent-folder entry would create a 5th and 6th copy. A single `open_folder(const std::filesystem::path& canonical)` method on `App` consolidates all six entrances, makes recent-folders recording trivial (one call site), and is a prerequisite for the spec's "shared action dispatch" requirement.

**Why not keep the copies and just add the recording call to each:** 6 recording calls is more error-prone than 1, and the spec requires every entrance to record — easiest to enforce with one method.

### D5: Persist `recent_folders` in `prefs.json` via a new `parse_array` helper in the hand-rolled JSON parser.

**Why:** `prefs.json` is the natural home (it's already the cross-folder, per-user store). The hand-rolled parser in `prefs.cpp` only handles three scalar fields today; adding a `parse_array` + `write_array` pair is a small, contained extension. Pulling in a JSON library (nlohmann/json, etc.) for one array field is disproportionate.

**How:** Extend `Prefs` with `std::vector<std::filesystem::path> recent_folders`. In `load_prefs`, after parsing the existing scalars, look for `"recent_folders"` and parse the array of strings. In `save_prefs`, write the array after the existing fields. Paths stored as canonical absolute strings. Dedup and cap-at-10 enforced in the writer/recorder (so even a hand-edited prefs file with duplicates or 20 entries gets cleaned on next save). At load time, filter out entries whose path no longer exists (`std::filesystem::exists`) — matches the spec's "Missing folder is dropped at load" scenario.

**Why not a separate `recent.json` file:** splits user state across two files for no benefit; `prefs.json` is already the right home.

### D6: Command palette is a new `src/ui/command_palette.cpp` / `.h` returning a `PaletteActions` struct, dispatched by `App::run()` alongside `ToolbarActions` / `MenubarActions`.

**Why:** The spec requires "shared action dispatch" — palette selections must set the SAME flags the toolbar/menubar set. The cleanest way to enforce this without coupling the palette to the toolbar's struct is to have the palette return its own `PaletteActions` struct, and have `App::run()` merge it into the same handlers. For commands that already have a flag in `ToolbarActions` or `MenubarActions` (Next, Previous, Refresh, Clear Cues, Copy Prompt, Open Folder, About, Quit, Show All, diff-mode toggle, etc.), the palette sets that same flag. For commands unique to the palette (per-recent-folder open, per-cue jump), the palette sets `palette_open_recent_index` / `palette_jump_to_cue_index` ints, mirroring the existing `jump_to_cue_index` pattern.

**Why a new struct instead of reusing `ToolbarActions`:** the palette also triggers menubar-only actions (About, Quit, Open Folder, Open Recent, Show All). A dedicated `PaletteActions` that App merges into the existing dispatch is clearer than overloading `ToolbarActions` with menubar concerns.

**How (rendering):** The palette is a `Begin("Command Palette", ...)` window rendered at the top of `App::run()`'s frame, gated by `palette_open_`. `Cmd+P` / `Ctrl+P` toggles `palette_open_`. When open, render a `InputText` (filter) + a child region with `Selectable` rows filtered by the input. Track `selected_index_` across frames. `Enter` runs the selected command (sets the appropriate flag(s) in `PaletteActions`), `Esc` and re-pressing `Cmd+P` close the palette. The window uses `ImGuiWindowFlags_NoDocking` (so it's not dockable — it's chrome) and is positioned at the top-center of the viewport (Sublime/VSCode-style) via `SetNextWindowPos`.

**Command list construction:** built per-frame from (a) a static list of named commands, (b) the current `recent_folders`, (c) the current `cues`. Each command carries: display name, category, and a small closure-or-enum that maps to a `PaletteActions` field. A simple `enum class PaletteCommand { NextChange, PrevChange, OpenFolder, OpenRecentN, ... }` plus a parallel `int payload` (for N) keeps it POD — no std::function needed.

**Alternatives considered:**
- Have the palette call `App` methods directly. Rejected — couples palette to App and bypasses the flag-based dispatch the spec mandates.
- Have the palette write into `ToolbarActions`/`MenubarActions` directly. Rejected — those are returned-by-value from their respective `render_*` functions; the palette would need its own return path anyway.

### D7: Cmd+P and Cmd+Shift+P handling via `ImGui::GetIO().KeyShift`.

**Why:** ImGui exposes `ImGuiKey_P` and `ImGui::GetIO().KeyShift`. Two checks: bare Cmd+P (no Shift) toggles palette; Cmd+Shift+P runs Copy Prompt. Order matters — check the shifted combo FIRST so a Shift+P press doesn't fall through to the bare-P branch.

**How:** In `App::run()`'s keyboard-shortcut block (currently `app.cpp:459–466`), replace the existing `Cmd+P → copy_prompt` line (`app.cpp:571–576`) with:
```
if (Cmd+P && !KeyShift) palette_open_ = !palette_open_;
if (Cmd+Shift+P)        tactions.copy_prompt = true;  // or a dedicated path
```
The toolbar tooltip text at `toolbar_panel.cpp:103` updates from `Ctrl+P` to `Ctrl+Shift+P`.

### D8: Hover-dwell on the Cues button uses a per-frame accumulator, not a `SetTooltip`.

**Why:** ImGui doesn't have a built-in "hover for N ms then call this" API. A small `float cue_hover_timer_` accumulated with `ImGui::GetIO().DeltaTime` when `IsItemHovered()` is true, reset to 0 when unhovered, and fired once when it crosses ~0.3s, is the standard pattern. We keep the state on the toolbar's render path (file-scope `static` is fine — the toolbar is single-instance).

## Risks / Trade-offs

- **[Hover popup fights ImGui's auto-close]** → ImGui closes popups on outside-click but not on mouse-leave. Mitigation: per-frame check that closes the popup when neither the button nor the popup is hovered; gate the close with a one-frame grace period so the mouse can transit from button to popup without the popup closing. Test: hover, move into popup, click an entry — entry must fire.
- **[Palette steals Cmd+P from users who muscle-memory'd Copy Prompt]** → Spec'd as BREAKING; mitigated by the tooltip + menubar + palette all surfacing the new Cmd+Shift+P binding, and the palette itself lists Copy Prompt as a runnable command (so Cmd+P → type "cop" → Enter also runs Copy Prompt in two keystrokes).
- **[Hand-rolled JSON array parser bug corrupts prefs.json]** → Mitigation: `save_prefs` writes atomically (temp file + rename, matching the existing cue-store pattern); `load_prefs` falls back to defaults on any parse error (existing behavior); add a unit test for parse_array round-tripping 0, 1, 10, 11+ entries with dedup.
- **[Recent folder recorded for a folder that fails to open (not a git repo)]** → Decision: record it anyway. The user explicitly opened it; "not a git repo" is a recoverable state (they may init one). The Not-a-Git-Repo warning still shows. Rejected alternative: skip recording on failure — surprises the user who expected to see it in recents.
- **[Palette performance with large cue counts]** → A user with hundreds of cues would make the palette list long. Mitigation: substring filter limits visible rows; ImGui's `Selectable` in a child region with `ImGuiListClipper` keeps it fast. Non-goal: fuzzy scoring.
- **[Dwell timer feels laggy or jumpy]** → 300 ms is the standard tooltip-dwell; configurable later if needed. If the user clicks the button before the timer elapses, open the popup immediately on click (so click is still a fast path even though hover is the primary trigger).
- **[Modal Enter conflicts with ImGui's Enter-to-activate-focused-button]** → ImGui's default may already trigger the focused button on Enter. Mitigation: explicitly focus the `Clear (Enter)` button when the modal opens (`ImGui::SetKeyboardFocusHere`), so the default Enter behavior aligns with our explicit handler. The explicit `IsKeyPressed(ImGuiKey_Enter)` handler is the source of truth either way.

## Migration Plan

No data migration required:
- `prefs.json` without a `recent_folders` key loads as an empty list (the new `parse_array` returns empty when the key is absent).
- `cue_dropdown` popup machinery is unchanged in name and shape — only its trigger changes.
- The Clear Cues modal's button labels get longer (`Clear (Enter)` vs `Clear`); the modal width (440px) already accommodates this.
- Copy Prompt shortcut change is the only behavioral BREAKING; document it in the next release notes.

Rollout is a single build (no feature flag). Rollback is `git revert` — no on-disk state needs undoing (the new `recent_folders` key is harmless to old binaries, which ignore it on load and overwrite-without-it on save — wait, that would silently DROP the recents list. **Mitigation:** make `save_prefs` always write the array, even if empty, so a downgrade loses recents but doesn't corrupt anything. Acceptable for a v0.1.x tool.)

## Open Questions

- Should the palette remember its last-used command across opens (Sublime does; VSCode doesn't)? Default: no (VSCode-style) — keep it stateless. Can revisit if the user asks.
- Should `Open Recent` show the folder's basename or the full path as the label? Default: full path (matches the spec scenario). Long paths truncate visually but the menu item still works.
- Should the palette support multi-word fuzzy matching (e.g., "nc" matches "Next Change")? Default: no, plain substring only (matches the spec's "case-insensitive substring match"). Can upgrade later without spec change.
