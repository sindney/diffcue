## ADDED Requirements

### Requirement: App theme selector in menubar

The system SHALL provide a `View → App Theme` menubar submenu listing every app theme
parsed from `thirdparty/theme.txt`. Each theme in `theme.txt` defined as a
`void SetupImGui<Name>Style()` function SHALL appear as a selectable entry named after
`<Name>`. Selecting an entry SHALL apply that theme (ImGui style + colors) immediately
to all open windows and panels. The currently selected theme SHALL be marked with a
checkmark in the submenu.

#### Scenario: Switch app theme

- **WHEN** the user opens `View → App Theme` and selects "DarkStyle"
- **THEN** all ImGui windows, buttons, and scrollbars immediately re-style to the
  DarkStyle colors and the "App Theme" submenu marks "DarkStyle" as selected

#### Scenario: Default theme on first launch

- **WHEN** the user launches diffcue for the first time (no `.diffcue/prefs.json` exists)
- **THEN** the first theme in `theme.txt` is applied and marked as selected

### Requirement: Text editor palette selector in menubar

The system SHALL provide a `View → Editor Palette` menubar submenu with at least three
entries: "Dark", "Light", and "Mariana". Selecting an entry SHALL call
`TextEditor::SetPalette` with the corresponding built-in palette on every `TextEditor`
and `TextDiff` instance currently open (diff panes, prompt pane). The currently selected
palette SHALL be marked with a checkmark.

#### Scenario: Switch editor palette

- **WHEN** the user opens `View → Editor Palette` and selects "Mariana" while a diff is
  open
- **THEN** both sides of the diff immediately re-render with the Mariana syntax colors
  (keywords, strings, comments)

#### Scenario: Apply to prompt pane

- **WHEN** the user opens the prompt pane (read-only TextEditor) and switches palette to
  "Light"
- **THEN** the prompt pane text re-renders with the Light palette

### Requirement: Theme persistence across sessions

The selected app theme and editor palette SHALL be written to
`<opened_folder>/.diffcue/prefs.json` whenever they change. On startup, the system SHALL
read `prefs.json` and restore the previously selected theme and palette. If the file is
missing or the saved theme name is no longer present in `theme.txt`, the system SHALL
fall back to the first theme in `theme.txt` and the "Dark" editor palette.

#### Scenario: Reopen restores theme

- **WHEN** the user selects "DarkStyle" app theme + "Mariana" editor palette, closes
  diffcue, and reopens the same folder
- **THEN** the app starts with DarkStyle applied and Mariana palette on the diff pane

#### Scenario: Theme removed from theme.txt

- **WHEN** `prefs.json` references app theme "Solarized" but `theme.txt` no longer defines
  it on next launch
- **THEN** the app falls back to the first theme in `theme.txt` and shows a one-time
  notice "Saved theme 'Solarized' not found; using <first>."

### Requirement: theme.txt parsing

The system SHALL parse `thirdparty/theme.txt` at startup, extracting every top-level
function definition matching the pattern `void SetupImGui<Name>Style()` and the body
that follows it (up to the matching closing brace). Each extracted `<Name>` SHALL become
a theme entry exposed by the app theme selector. Themes whose body references undefined
symbols SHALL be skipped with a warning logged to stderr, and SHALL NOT prevent the other
themes from loading.

#### Scenario: All themes parsed

- **WHEN** `theme.txt` contains 15 `SetupImGui*Style()` functions
- **THEN** the `View → App Theme` submenu lists 15 entries, one per function

#### Scenario: Malformed theme skipped

- **WHEN** `theme.txt` contains a `SetupImGuiFooStyle()` function whose body references an
  undefined symbol `Bar()`
- **THEN** the theme "Foo" is skipped, a warning is logged to stderr, and the remaining
  themes still load and appear in the submenu

### Requirement: Independent app theme and editor palette

The app theme and the editor palette SHALL be independent selections — changing one
SHALL NOT change the other. A user MAY combine any app theme with any editor palette.

#### Scenario: Dark chrome with light code

- **WHEN** the user selects "DarkStyle" app theme and "Light" editor palette
- **THEN** the window chrome renders in dark style and the diff pane renders with the
  Light syntax palette simultaneously
