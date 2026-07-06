## Purpose

Attach short per-line comments ("cues"); maintain an ordered cue list; toolbar cue counter; jump-to-cue; edit/delete cues.

## Requirements

### Requirement: Add a cue to a diff line

The system SHALL allow the user to attach a short text comment ("cue") to any visible
line on either side of the diff. Adding a cue SHALL open a small inline text input
prefixed with the target file and line; pressing Enter confirms, Esc cancels.

#### Scenario: Add cue to new-side line

- **WHEN** the user clicks line 42 on the new side of `src/main.cpp` and types "rename
  to foo" then presses Enter
- **THEN** the cue is stored with `{file: "src/main.cpp", side: "new", line: 42,
  text: "rename to foo"}` and a cue marker is rendered on that line

#### Scenario: Cancel cue input

- **WHEN** the user opens the inline cue input and presses Esc
- **THEN** no cue is created and the input closes

### Requirement: Edit an existing cue

The system SHALL allow the user to edit an existing cue's text by clicking its marker
or by selecting it from the cue list. Editing SHALL reuse the same inline input.

#### Scenario: Edit cue text

- **WHEN** the user clicks the cue marker on line 42 and changes the text to "rename
  to bar"
- **THEN** the stored cue's `text` is updated to "rename to bar" and the marker label
  refreshes

### Requirement: Delete a cue

The system SHALL allow the user to delete a cue via a small × button next to the cue
marker or via a Delete button in the cue list entry. Deletion SHALL require no
confirmation (cues are easily re-added).

#### Scenario: Delete via marker

- **WHEN** the user clicks the × button on the cue marker at line 42
- **THEN** the cue is removed from the store and the marker disappears

### Requirement: Cue persistence

The system SHALL persist all cues to `<opened_folder>/.diffcue/cues.json` atomically
on every cue add/edit/delete. On startup, the system SHALL load cues from that file if
it exists and its `folder` field matches the opened folder. Cues from a different folder
SHALL be ignored.

#### Scenario: Reopen a folder

- **WHEN** the user closes diffcue and reopens the same folder
- **THEN** all previously created cues are restored to their original files and lines

#### Scenario: Sidecar missing

- **WHEN** the user opens a folder with no `.diffcue/cues.json`
- **THEN** the cue store starts empty and no error is shown

### Requirement: Cue counter in toolbar

The toolbar SHALL display the current total cue count, e.g., `Cues: 4`. The counter
SHALL update immediately on every add/delete.

#### Scenario: Counter updates on add

- **WHEN** the user adds a cue and the counter previously read `Cues: 3`
- **THEN** the toolbar immediately reads `Cues: 4`

### Requirement: Cue list and jump-to-cue

The toolbar's cue summary (shown when the user clicks the cue counter) SHALL list every
cue as `file:line - text`. Clicking an entry SHALL open that file in the diff viewer
and scroll to the cue's line.

#### Scenario: Jump to a cue in another file

- **WHEN** the user is viewing `a.cpp` and clicks the cue list entry for `b.h:7 - missing
  guard`
- **THEN** the diff viewer loads `b.h` and scrolls to line 7, marking the cue line

### Requirement: Per-file cue marker rendering

Every line in the diff viewer that has a cue SHALL render a visible marker (e.g., a
colored bar in the gutter and a truncated cue text preview next to the line). Hovering
the marker SHALL show the full cue text in a tooltip.

#### Scenario: Hover cue marker

- **WHEN** the user hovers the cue marker on line 42
- **THEN** a tooltip shows the full cue text "rename to bar"

### Requirement: Cue side and line stability

The system SHALL NOT auto-rewrite cue line numbers when the working tree changes. A cue whose `file` is the relative path from the opened folder root, and whose `line` is the line number on the indicated `side` (old or new) at creation time, SHALL be flagged as "stale" in the cue list when the target line no longer exists, and the marker SHALL render in a warning style.

#### Scenario: Stale cue after re-edit

- **WHEN** the user adds a cue at line 42, then externally edits the file so that line
  42 no longer exists, then reloads the diff
- **THEN** the cue appears in the cue list marked "stale" and its marker renders in a
  warning color
