## ADDED Requirements

### Requirement: Command palette open/close

The system SHALL provide a command palette window toggled by the keyboard shortcut **Cmd+P** on macOS and **Ctrl+P** on other platforms. When opened, the palette SHALL appear as a centered ImGui window with a single-line text input (focused on open) and a scrollable list of available commands below it. Pressing **Cmd+P** / **Ctrl+P** again while the palette is open, or pressing **Esc**, SHALL close the palette. The palette SHALL NOT be dockable (it is application chrome, like the toolbar). While the palette is open, it SHALL capture keyboard navigation (Up/Down/Enter/Esc) without leaking to the diff viewer or other panels.

#### Scenario: Open the palette

- **WHEN** the user presses **Cmd+P** (macOS) or **Ctrl+P** (other platforms) while diffcue has keyboard focus
- **THEN** a centered palette window appears with an empty text input focused, and a list of all available commands below it

#### Scenario: Close with Esc

- **WHEN** the palette is open and the user presses **Esc**
- **THEN** the palette closes and no command runs

#### Scenario: Toggle closed

- **WHEN** the palette is open and the user presses **Cmd+P** / **Ctrl+P** again
- **THEN** the palette closes and no command runs

### Requirement: Filterable command list

The palette SHALL list a fixed set of commands grouped into Navigation, Cues, View, and App categories. Typing in the input SHALL filter the list by case-insensitive substring match against the command's display name (and, optionally, its category). The first remaining command SHALL be auto-selected. The list SHALL include at least: Next Change, Previous Change, Open Folder..., Open Recent (one entry per recent folder), Refresh, Clear Cues, Copy Prompt, Jump to Cue (one entry per cue), Toggle Diff Mode, Toggle Ignore EOL, Toggle Find Bar, Show All, About diffcue, and Quit.

#### Scenario: Filter by substring

- **WHEN** the palette is open with 14 commands visible and the user types `cue`
- **THEN** the list narrows to commands whose name contains `cue` (e.g., `Clear Cues`, `Copy Prompt`, `Jump to Cue: <file>:<line>`), with the first match auto-selected

#### Scenario: No matches

- **WHEN** the user types `zzz` and no command name contains `zzz`
- **THEN** the list shows an empty state (e.g., `No matching commands`) and pressing **Enter** does nothing

### Requirement: Keyboard navigation

While the palette is open, **Up** and **Down** SHALL move the selection one row at a time (wrapping at the top and bottom). **Enter** SHALL run the currently selected command and close the palette. **Esc** SHALL close the palette without running anything. The mouse MAY also be used to click a row, which SHALL have the same effect as selecting it with Up/Down and pressing **Enter**.

#### Scenario: Move selection down

- **WHEN** the palette is open with 5 visible commands, the first is selected, and the user presses **Down**
- **THEN** the second command is selected (highlighted)

#### Scenario: Wrap from last to first

- **WHEN** the last command is selected and the user presses **Down**
- **THEN** the selection wraps to the first command

#### Scenario: Run the selected command

- **WHEN** the palette is open, `Next Change` is selected, and the user presses **Enter**
- **THEN** the palette closes and the diff viewer jumps to the next change (identical to clicking the toolbar's `Next` button)

### Requirement: Shared action dispatch

Every palette command SHALL route through the SAME action code path as its toolbar / menubar equivalent. Concretely: palette selections SHALL set the same `ToolbarActions` / `MenubarActions` boolean/int flags that the toolbar and menubar set, and `App::run()` SHALL dispatch those flags through the SAME handlers — NO duplicate implementation of any command's effect exists in the palette. Adding a new command to the palette therefore SHALL NOT require touching the existing effect code, only adding a new flag if one does not already exist.

#### Scenario: Next Change from palette vs toolbar

- **WHEN** the user runs `Next Change` from the palette
- **THEN** the resulting `actions.next_change = true` flag is dispatched by `App::run()`'s existing `if (tactions.next_change)` handler — the palette does not call `scroll_to_next_change` directly

#### Scenario: Open Folder from palette reuses the picker

- **WHEN** the user runs `Open Folder...` from the palette
- **THEN** the same native folder picker is opened and the same `open_folder(path)` code path handles the result — identical to `File ▸ Open Folder...`

#### Scenario: Copy Prompt from palette reuses the toolbar path

- **WHEN** the user runs `Copy Prompt` from the palette
- **THEN** the same `actions.copy_prompt = true` flag is set and the existing Copy Prompt handler generates the prompt, fills the pane, and copies to clipboard — no separate palette-side implementation

### Requirement: Palette shortcut does not collide with Copy Prompt

The palette's **Cmd+P** / **Ctrl+P** shortcut SHALL be distinct from Copy Prompt's **Cmd+Shift+P** / **Ctrl+Shift+P** shortcut. Bare **Cmd+P** / **Ctrl+P** opens the palette; bare **Cmd+Shift+P** / **Ctrl+Shift+P** runs Copy Prompt. Neither shortcut SHALL trigger the other action.

#### Scenario: Bare Cmd+P opens the palette

- **WHEN** the user presses **Cmd+P** with no Shift held
- **THEN** the palette opens and Copy Prompt does NOT run

#### Scenario: Shifted Cmd+P runs Copy Prompt

- **WHEN** the user presses **Cmd+Shift+P**
- **THEN** Copy Prompt runs and the palette does NOT open
