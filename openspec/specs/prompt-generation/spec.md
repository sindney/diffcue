## Purpose

Compose a structured follow-up prompt from all cues (`path:line - comment`), show it in an editable text pane, and copy to clipboard.

## Requirements

### Requirement: Generate prompt from cues

The system SHALL provide a "Copy Prompt" button in the toolbar. When clicked, the system
SHALL build a text prompt by concatenating every cue (grouped by file, sorted by line
ascending) into lines of the form `<relpath>:<line> - <cue text>`. The prompt SHALL begin
with the header line `# diffcue review cues` followed by a blank line.

#### Scenario: Three cues across two files

- **WHEN** the user has cues `a.cpp:42 - fix A`, `a.cpp:7 - fix B`, `b.h:3 - fix C` and
  clicks "Copy Prompt"
- **THEN** the generated prompt is:
  ```
  # diffcue review cues

  - a.cpp:7 - fix B
  - a.cpp:42 - fix A
  - b.h:3 - fix C
  ```

#### Scenario: No cues

- **WHEN** the user clicks "Copy Prompt" with zero cues
- **THEN** the prompt pane shows an empty-state message "No cues yet — click a line in
  the diff to add one." and the clipboard is not modified

### Requirement: Prompt shown in editable text pane

The generated prompt SHALL be displayed in a read-only-by-default `TextEditor` pane
(prompt pane) that supports editing (the user MAY trim or reword before copying). The
pane SHALL be brought to focus when "Copy Prompt" is clicked.

#### Scenario: Edit the prompt before copying

- **WHEN** the generated prompt contains three lines and the user deletes one line in
  the pane
- **THEN** the clipboard (when copied) receives the two-line edited version, not the
  original three-line version

### Requirement: Copy prompt to clipboard

A "Copy to Clipboard" button in the prompt pane SHALL copy the *current contents* of
the prompt pane to the system clipboard via `glfwSetClipboardString`. The toolbar
"Copy Prompt" button SHALL be equivalent to "generate → fill pane → copy pane to
clipboard" in one step.

#### Scenario: One-click copy

- **WHEN** the user clicks "Copy Prompt" in the toolbar
- **THEN** the prompt is generated, displayed in the prompt pane, and the clipboard
  contains the same text as the pane's current contents

#### Scenario: Edit-then-copy

- **WHEN** the user edits the prompt pane then clicks "Copy to Clipboard"
- **THEN** the clipboard contains the edited text

### Requirement: Clipboard confirmation

After a successful copy, the system SHALL show a transient confirmation (e.g., a
toast "Copied N cues to clipboard" for 1.5 seconds) so the user knows the copy
succeeded on platforms where clipboard feedback is unreliable.

#### Scenario: Successful copy

- **WHEN** the clipboard copy succeeds with 4 cues
- **THEN** a toast "Copied 4 cues to clipboard" appears for ~1.5 seconds

### Requirement: Prompt format stability

The prompt format (`- <path>:<line> - <text>` lines, one per cue) SHALL be stable across versions so downstream coding CLIs can rely on it. Any future format change MUST be gated behind a `--prompt-format` CLI flag and MUST default to the v1 format.

#### Scenario: Default format is v1

- **WHEN** the user runs `diffcue` with no `--prompt-format` flag and clicks "Copy Prompt"
- **THEN** the prompt uses the `- <path>:<line> - <text>` line format, one cue per line

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
