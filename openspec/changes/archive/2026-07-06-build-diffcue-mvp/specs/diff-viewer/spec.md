## ADDED Requirements

### Requirement: Side-by-side diff rendering

The system SHALL render a side-by-side diff for any file selected from the file browser,
with the old (working-tree base) version on the left and the new (working-tree current)
version on the right, using `ImGuiColorTextEdit::TextDiff` for diff computation and
rendering. Both panes SHALL scroll vertically in sync.

#### Scenario: Open a modified file

- **WHEN** the user clicks a modified file in the file browser
- **THEN** the diff viewer displays the file's old and new content side-by-side with
  additions highlighted on the new side and removals highlighted on the old side

#### Scenario: Binary file opened

- **WHEN** the user selects a file detected as binary
- **THEN** the diff viewer SHALL display a "Binary file, no text diff" placeholder instead
  of attempting to render its bytes

### Requirement: Syntax highlighting

The system SHALL apply syntax highlighting to both sides of the diff based on the file
extension, using `ImGuiColorTextEdit`'s built-in language definitions. At minimum the
system MUST recognize: `.cpp`, `.h`, `.hpp`, `.c`, `.cc`, `.py`, `.js`, `.ts`, `.json`,
`.md`, `.txt`, `.cmake`, `.sh`.

#### Scenario: Open a C++ file

- **WHEN** the user opens `src/main.cpp`
- **THEN** both diff panes render with C++ keyword/string/comment highlighting

#### Scenario: Unknown extension

- **WHEN** the user opens a file with an unrecognized extension
- **THEN** the diff viewer renders the file as plain text with no highlighting

### Requirement: Line-ending metadata display

For every line in the diff, the system SHALL detect and display the line-ending style
of the *new* side of that file. Detected styles MUST be one of: `LF`, `CRLF`, `CR`,
`Mixed`, `None` (for the file's final line having no terminator). The label SHALL be
shown once per file in the diff viewer's header bar.

#### Scenario: CRLF file on Windows

- **WHEN** the user opens a file whose new-side bytes contain `\r\n` as the majority line
  terminator
- **THEN** the diff viewer header shows `EOL: CRLF`

#### Scenario: Mixed line endings

- **WHEN** the user opens a file containing both `\n` and `\r\n` as line terminators with
  no clear majority
- **THEN** the diff viewer header shows `EOL: Mixed`

### Requirement: Encoding metadata display

The system SHALL detect and display the encoding of each side of the diff. Detected
encodings MUST be one of: `UTF-8`, `UTF-8-BOM`, `UTF-16LE`, `UTF-16BE`, `latin1`,
`binary`. Both the old-side and new-side encodings SHALL be shown in the diff viewer
header bar.

#### Scenario: BOM added by editor

- **WHEN** the old side is `UTF-8` and the new side is `UTF-8-BOM`
- **THEN** the header shows `Encoding: UTF-8 → UTF-8-BOM`, signaling the BOM change to the
  reviewer

#### Scenario: UTF-16LE file

- **WHEN** the user opens a file detected as UTF-16LE on both sides
- **THEN** the header shows `Encoding: UTF-16LE → UTF-16LE` and the content is transcoded
  to UTF-8 for display

### Requirement: Prev / Next change navigation

The system SHALL provide Prev and Next buttons in the toolbar that jump the diff viewer
to the previous or next changed hunk, respectively. Navigation SHALL wrap across files:
after the last hunk in the current file, Next SHALL move to the first hunk of the next
file in the file browser's modified list. The "current hunk" SHALL be visually marked
(e.g., a distinct background on the changed lines).

#### Scenario: Next within a file

- **WHEN** the user is viewing hunk 1 of file A and clicks Next
- **THEN** the diff viewer scrolls to hunk 2 of file A and marks it current

#### Scenario: Next crosses file boundary

- **WHEN** the user is viewing the last hunk of file A and clicks Next
- **THEN** the file browser selects file B (the next modified file) and the diff viewer
  scrolls to its first hunk

#### Scenario: No changes at all

- **WHEN** the opened folder has zero modified files and the user clicks Next
- **THEN** the button is a no-op (and the toolbar shows "No changes")

### Requirement: Diff size cap

The system SHALL refuse to compute a text diff for files with more than 5000 changed
lines and instead display a "Diff truncated: open in external tool" banner. The file
SHALL still appear in the file browser with its git status intact.

#### Scenario: Very large generated file

- **WHEN** the user selects a file with 12000 changed lines
- **THEN** the diff viewer shows the truncation banner and does not block the UI

### Requirement: Inline and side-by-side diff modes

The system SHALL support two diff rendering modes: side-by-side (two panes) and inline
(unified, single pane). The mode SHALL be switchable at runtime from the toolbar (a
"Diff Mode" dropdown) and from the `View → Diff Mode` menubar entry. The default mode
SHALL be side-by-side. Switching modes SHALL preserve the current scroll position and
cue markers. The selected mode SHALL be persisted to `.diffcue/prefs.json`.

#### Scenario: Switch to inline

- **WHEN** the user is in side-by-side mode viewing line 42 and selects "Inline" from the
  toolbar dropdown
- **THEN** the diff re-renders as a single unified pane with `+`/`-` style line coloring,
  the view stays at line 42, and any cue markers remain visible

#### Scenario: Persisted across reopen

- **WHEN** the user sets the mode to inline, closes diffcue, and reopens the same folder
- **THEN** the diff viewer defaults to inline mode without the user having to switch again

### Requirement: Text find in the diff

The system SHALL provide a find bar (toggled by Ctrl+F or a toolbar Find button) that
searches the currently focused side of the diff (old or new pane) for a user-entered
query string. The find bar SHALL support: Next (F3) / Prev (Shift+F3) navigation with
wraparound, a "Highlight all" checkbox that marks every matching line, and a
case-sensitivity toggle (default off). Esc SHALL dismiss the find bar.

#### Scenario: Find next

- **WHEN** the user presses Ctrl+F, types "foo", and presses F3
- **THEN** the focused diff pane scrolls to the next occurrence of "foo" and highlights it

#### Scenario: Wraparound

- **WHEN** the cursor is past the last match and the user presses F3
- **THEN** the find wraps to the first match in the file

#### Scenario: Highlight all

- **WHEN** the user checks "Highlight all" with query "TODO"
- **THEN** every line containing "TODO" in the focused pane receives a marker

#### Scenario: Case-sensitive toggle

- **WHEN** the user enables case-sensitivity with query "Foo" and the pane contains "foo"
  but no "Foo"
- **THEN** no matches are reported

#### Scenario: Read-only — no replace

- **WHEN** the find bar is open in a diff pane
- **THEN** the find bar exposes no Replace or Replace-All controls; the diff content is
  not modifiable from the find bar
