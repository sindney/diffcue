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

The toolbar's cue counter (`Cues: N`) SHALL be rendered every frame, including when the count is zero (`Cues: 0`). Hovering the cue counter with the mouse for a short dwell (no click required) SHALL open the cue list popup (`cue_dropdown`); the popup SHALL close when the mouse leaves both the button and the popup. The list SHALL show every cue as `file:line - text`. Clicking an entry SHALL open that file in the diff viewer and scroll the viewer so the cue's line is centered. The previous plain-text hover tooltip ("Click to list all cues.") SHALL be removed. The cue counter SHALL NOT open the popup on click; hover is the sole trigger.

#### Scenario: Hover opens the cue list

- **WHEN** the user hovers the `Cues: 3` button and holds the mouse still for ~300 ms
- **THEN** the `cue_dropdown` popup appears under the button listing all three cues as `file:line - text` entries

#### Scenario: Click on a cue jumps and scrolls

- **WHEN** the user is viewing `a.cpp`, the cue list is open, and the user clicks the entry for `b.h:7 - missing guard`
- **THEN** the diff viewer loads `b.h` and scrolls so line 7 is centered in the viewport, and the cue marker on line 7 is rendered

#### Scenario: Zero cues still renders the button

- **WHEN** the cue store is empty
- **THEN** the toolbar still renders `Cues: 0`, and hovering it opens an empty `cue_dropdown` popup (or a disabled "No cues" row) rather than hiding the button

#### Scenario: Mouse leaves the popup area

- **WHEN** the cue list is open and the user moves the mouse off both the `Cues: N` button and the `cue_dropdown` popup
- **THEN** the popup closes without selecting any entry

### Requirement: Clear all cues confirmation

The toolbar SHALL provide a `Clear` button that, when clicked, opens a centered modal popup titled `Clear Cues?` showing the cue count and two buttons: `Clear (Enter)` and `Cancel (Esc)`. Clicking `Clear (Enter)` or pressing **Enter** while the modal is open SHALL remove every cue from the store and close the modal. Clicking `Cancel (Esc)` or pressing **Esc** while the modal is open SHALL close the modal without modifying any cue. The button labels SHALL include their shortcut hints in parentheses. The modal SHALL NOT require a mouse interaction to confirm or cancel — both actions SHALL be reachable from the keyboard alone.

#### Scenario: Confirm with Enter

- **WHEN** the `Clear Cues?` modal is open with 5 cues and the user presses **Enter**
- **THEN** all 5 cues are deleted from the store and the modal closes

#### Scenario: Cancel with Esc

- **WHEN** the `Clear Cues?` modal is open with 5 cues and the user presses **Esc**
- **THEN** no cue is deleted and the modal closes

#### Scenario: Confirm with the mouse

- **WHEN** the `Clear Cues?` modal is open and the user clicks the `Clear (Enter)` button
- **THEN** all cues are deleted and the modal closes (identical to the Enter-key path)

#### Scenario: Button labels show shortcuts

- **WHEN** the `Clear Cues?` modal renders
- **THEN** the confirm button label contains `Enter` and the cancel button label contains `Esc`, so the keyboard shortcuts are discoverable without leaving the modal

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
