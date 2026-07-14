## MODIFIED Requirements

### Requirement: Add a cue to a diff line

The system SHALL allow the user to attach a short text comment ("cue") to any visible
line on either side of the diff. Adding a cue SHALL open a line-anchored popup prefixed
with the target file and line; pressing Enter confirms, Esc cancels.

The popup's text-input area SHALL be large enough that a typical paragraph cue is
readable in place: at least six visible text lines, the full width of the popup's
content area, with `WordWrap` enabled so a long single line wraps to the next visible
row rather than running off the right edge. `Ctrl+Enter` SHALL insert a literal newline
inside the field; bare `Enter` SHALL submit. The popup SHALL display a vertical
scrollbar when the cue text exceeds the visible area so multi-line content and long
single-line content (such as stack traces) are reachable without resizing the popup.

#### Scenario: Add cue to new-side line

- **WHEN** the user clicks line 42 on the new side of `src/main.cpp` and types "rename
  to foo" then presses Enter
- **THEN** the cue is stored with `{file: "src/main.cpp", side: "new", line: 42,
  text: "rename to foo"}` and a cue marker is rendered on that line

#### Scenario: Long single-line cue is visible in the popup

- **WHEN** the user right-clicks a line and pastes a single line of text that exceeds
  the popup's input width (e.g. a long URL or stack-trace line)
- **THEN** the text wraps onto multiple visible rows inside the popup so the user can
  read and edit it without horizontal scrolling, and the cue is stored verbatim on
  submit (the wrap is visual only; no `\n` is inserted)

#### Scenario: Multi-line cue content is reachable

- **WHEN** the user pastes or types more lines than fit in the visible text area of
  the add-cue popup
- **THEN** a vertical scrollbar appears on the text field and the user can scroll
  to reach the rest of the text without the popup itself being resized

#### Scenario: Cancel cue input

- **WHEN** the user opens the inline cue input and presses Esc
- **THEN** no cue is created and the input closes

### Requirement: Cue list and jump-to-cue

The toolbar's cue counter (`Cues: N`) SHALL be rendered every frame, including when the count is zero (`Cues: 0`). Hovering the cue counter with the mouse for a short dwell (no click required) SHALL open the cue list popup (`cue_dropdown`); the popup SHALL close when the mouse leaves both the button and the popup. The list SHALL show every cue as `file:line - text`. Clicking an entry SHALL open that file in the diff viewer and scroll the viewer so the cue's line is centered in the viewport. The previous plain-text hover tooltip ("Click to list all cues.") SHALL be removed. The cue counter SHALL NOT open the popup on click; hover is the sole trigger.

When a cue's `side` is `new` (or `old`) and the line is present in the new (or old)
file, the scroll target SHALL be the diff-view row whose right (or left) line number
equals the cue's `line`. When the cue's side has no surviving lines (deletions on the
new side or additions on the old side), the scroll target SHALL be the closest
surviving line on the cue's side and the cue SHALL be marked stale.

#### Scenario: Hover opens the cue list

- **WHEN** the user hovers the `Cues: 3` button and holds the mouse still for ~300 ms
- **THEN** the `cue_dropdown` popup appears under the button listing all three cues as `file:line - text` entries

#### Scenario: Click on a cue jumps and scrolls

- **WHEN** the user is viewing `a.cpp`, the cue list is open, and the user clicks the entry for `b.h:7 - missing guard`
- **THEN** the diff viewer loads `b.h` and scrolls so the diff-view row whose right-side line is 7 is centered in the viewport, and the cue marker on that line is rendered

#### Scenario: Jump from the command palette also centers the line

- **WHEN** the user opens the command palette (Cmd+P / Ctrl+P), filters to a cue, and
  presses Enter
- **THEN** the diff viewer loads the cue's file and scrolls so the cue's line is
  centered in the viewport

#### Scenario: Cue from a line beyond the first viewport

- **WHEN** the user clicks a cue whose line is past the first viewport of the
  currently-open file
- **THEN** the diff viewer scrolls so the cue's line is centered in the viewport (not
  merely the top, not unchanged)

#### Scenario: Zero cues still renders the button

- **WHEN** the cue store is empty
- **THEN** the toolbar still renders `Cues: 0`, and hovering it opens an empty `cue_dropdown` popup (or a disabled "No cues" row) rather than hiding the button

#### Scenario: Mouse leaves the popup area

- **WHEN** the cue list is open and the user moves the mouse off both the `Cues: N` button and the `cue_dropdown` popup
- **THEN** the popup closes without selecting any entry
