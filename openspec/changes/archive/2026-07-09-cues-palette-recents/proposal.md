## Why

The cue-list and clear-cues interactions today demand a mouse click and offer no keyboard confirmation, jump-to-cue lands on the right file but the wrong scroll position (the cue's line is dropped on the floor), and there is no quick way to drive diffcue from the keyboard end-to-end. This change fixes the jump bug, makes the cue list appear on hover, adds keyboard confirm/cancel (with hints in the button label) to the clear-cues modal, persists an "Open Recent" list of up to 10 folders, and introduces a Cmd+P command palette (Sublime/VSCode-style) that reuses the exact same action code paths as the toolbar and menubar so every entrance behaves identically.

## What Changes

- **Hover-to-show cue list.** The "Cues: N" toolbar button opens the existing `cue_dropdown` popup after a short hover dwell (instead of requiring a click); the plain-text tooltip is removed. The button is always rendered (label `Cues: 0` when empty); hovering when empty shows an empty list / disabled state.
- **Jump-to-cue scroll fix.** `App::run()` now calls `diff_viewer_.scroll_to_line(c.line)` after `open_file(c.file)` so the cue's line is centered in the viewer. (The cue `line` field is already 1-based end-to-end; no indexing change.)
- **Clear-cues modal keyboard shortcuts.** In the "Clear Cues?" modal, **Enter** confirms (equivalent to clicking "Clear"), **Esc** cancels (closes the popup), and the button labels include the shortcut hint (e.g., `Clear (Enter)` / `Cancel (Esc)`).
- **Open Recent menu.** A new `File ▸ Open Recent ▸ <folder>` submenu lists the user's most recently opened folders (up to 10, most-recent-first, deduplicated by canonical path). Selecting one opens that folder exactly as `Open Folder...` does (same `open_folder(path)` code path). Opening a folder (via CLI, picker, drag-drop, or recent) records it in the list. The list persists to `prefs.json`.
- **Command palette (Cmd+P).** A new top-level ImGui window (centered modal-style) opens on **Cmd+P** (macOS) / **Ctrl+P** elsewhere, showing a single text input and a filtered list of commands. The palette includes:
  - **Navigation:** Next Change, Previous Change, Open Folder..., Open Recent ▸ (each recent folder as a sub-entry or filtered match), Refresh
  - **Cues:** Add cue (no-op when no file open), Clear Cues, Copy Prompt, Jump to Cue ▸ (each cue as a sub-entry)
  - **View:** Toggle Diff Mode (Side by Side / Inline), Toggle Ignore EOL, Toggle Find Bar, Show All
  - **App:** About diffcue, Quit
  - Typing filters the list by substring (case-insensitive); **Up/Down** moves the selection, **Enter** runs the highlighted command, **Esc** closes the palette. Each command routes through the **same action flags / handlers** the toolbar and menubar already use — no duplicated logic.
- **Copy Prompt rebind.** Copy Prompt's keyboard shortcut moves from **Cmd+P** / **Ctrl+P** to **Cmd+Shift+P** / **Ctrl+Shift+P** to free Cmd+P for the palette. The toolbar tooltip is updated to reflect the new binding. **BREAKING** for muscle-memory users.

## Capabilities

### New Capabilities
- `recent-folders`: Persist and surface the user's most recently opened folders (up to 10) in a File ▸ Open Recent submenu and the command palette; opening any folder records it.
- `command-palette`: A Cmd+P / Ctrl+P keyboard-driven command list that filters and dispatches the same actions the toolbar and menubar expose, with no duplicated action logic.

### Modified Capabilities
- `review-cues`: The cue counter's list opens on hover (with a dwell delay) rather than on click; the click-tooltip is removed. The Clear Cues confirmation modal gains an Enter=confirm / Esc=cancel keyboard contract with shortcut hints in the button labels. (Jump-to-cue already requires scrolling in the spec; this change fixes the implementation to match — no requirement wording change for the jump itself.)
- `prompt-generation`: Copy Prompt gains an explicit keyboard-shortcut requirement (Cmd+Shift+P / Ctrl+Shift+P), replacing the previous Cmd+P / Ctrl+P implementation-only binding.

## Impact

- **Code:**
  - `src/ui/toolbar_panel.cpp` (cue button hover behavior, tooltip removal, button always rendered; Copy Prompt tooltip shortcut text), `src/ui/toolbar_panel.h` (no struct change expected).
  - `src/app/app.cpp` (one-line `scroll_to_line` fix in the jump handler; Clear Cues modal Enter/Esc handling and button label hints; Cmd+P palette toggle; Cmd+Shift+P Copy Prompt rebind; wire palette selections into existing action flags; record folder opens into recents).
  - `src/ui/menubar.cpp` / `src/ui/menubar.h` (add `Open Recent` submenu; surface recent-folder selection via `MenubarActions`).
  - New `src/ui/command_palette.cpp` / `.h` (palette window: filter input, list, dispatch into the shared `ToolbarActions` / `MenubarActions` flags).
  - `src/model/prefs.h` / `src/model/prefs.cpp` (add `std::vector<std::filesystem::path> recent_folders` to `Prefs`; extend the hand-rolled JSON parser/serializer with array support; cap at 10, dedupe by canonical path, most-recent-first).
- **Specs:** `review-cues` and `prompt-generation` get delta specs; `recent-folders` and `command-palette` get new spec files.
- **Dependencies:** No new third-party deps. ImGui's popup + `Begin` machinery suffices for both the hover list (reuse `cue_dropdown` opened on hover) and the palette (a `Begin` window toggled by Cmd+P).
- **Persistence:** `prefs.json` gains a `recent_folders` array; old prefs files without the array load fine (empty list). No migration step needed.
- **Compatibility:** Copy Prompt's shortcut change is the only **BREAKING** UX change; the button and menu still work, only the keyboard binding moves.
