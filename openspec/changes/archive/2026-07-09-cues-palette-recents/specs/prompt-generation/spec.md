## ADDED Requirements

### Requirement: Copy Prompt keyboard shortcut

The Copy Prompt action (toolbar button + menubar equivalent + command-palette entry) SHALL be invocable via the keyboard shortcut **Cmd+Shift+P** on macOS and **Ctrl+Shift+P** on other platforms. The shortcut SHALL produce the exact same effect as clicking the toolbar's `Copy Prompt` button (generate → fill prompt pane → copy pane to clipboard). The shortcut SHALL NOT collide with the command palette's **Cmd+P** / **Ctrl+P** binding. The toolbar's `Copy Prompt` hover tooltip SHALL display the current shortcut.

#### Scenario: Trigger Copy Prompt from the keyboard

- **WHEN** the user presses **Cmd+Shift+P** (macOS) or **Ctrl+Shift+P** (other platforms) while diffcue has keyboard focus and the cue store has cues
- **THEN** the prompt is generated from all cues, the prompt pane is filled and focused, and the clipboard receives the prompt text — identical to clicking `Copy Prompt`

#### Scenario: Shortcut does not open the palette

- **WHEN** the user presses **Cmd+Shift+P** (no other modifiers held)
- **THEN** Copy Prompt runs and the command palette does NOT open (the palette is bound to bare **Cmd+P**)

#### Scenario: Tooltip reflects the binding

- **WHEN** the user hovers the `Copy Prompt` toolbar button
- **THEN** the tooltip text mentions `Ctrl+Shift+P` (or `Cmd+Shift+P` on macOS) as the shortcut
