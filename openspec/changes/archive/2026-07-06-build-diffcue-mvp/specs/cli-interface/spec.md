## ADDED Requirements

### Requirement: Positional folder argument

The system SHALL accept a single positional argument `<folder>` specifying the working
folder to open for review. The folder SHALL be resolved to an absolute path and SHALL
be verified to exist and be a directory before the GUI starts.

#### Scenario: Open a folder

- **WHEN** the user runs `diffcue /home/me/proj`
- **THEN** the GUI opens with `/home/me/proj` as the working folder

#### Scenario: Non-existent folder

- **WHEN** the user runs `diffcue /does/not/exist`
- **THEN** the system prints `error: folder not found: /does/not/exist` to stderr and
  exits with code 2

#### Scenario: Path is a file, not a folder

- **WHEN** the user runs `diffcue /home/me/proj/README.md`
- **THEN** the system prints `error: not a folder: /home/me/proj/README.md` to stderr
  and exits with code 2

### Requirement: --help flag

The system SHALL accept `--help` and `-h`. When passed, the system SHALL print a usage
message to stdout listing every supported flag and the positional `<folder>` argument,
then exit with code 0 without opening a GUI.

#### Scenario: Help output

- **WHEN** the user runs `diffcue --help`
- **THEN** stdout shows usage including `--help`, `--version`, and the `<folder>`
  positional argument, and the exit code is 0

### Requirement: --version flag

The system SHALL accept `--version` and `-V`. When passed, the system SHALL print its
version string to stdout in the form `diffcue <major.minor.patch>` and exit with code 0
without opening a GUI.

#### Scenario: Version output

- **WHEN** the user runs `diffcue --version`
- **THEN** stdout shows `diffcue 0.1.0` (for the 0.1.0 release) and the exit code is 0

### Requirement: No folder argument opens folder picker

When no `<folder>` argument is supplied, the system SHALL open a native folder picker
dialog (via tinyfiledialogs) and use the selected folder as the working folder. If the
user cancels the dialog, the system SHALL exit with code 0 without opening a GUI.

#### Scenario: Pick a folder

- **WHEN** the user runs `diffcue` with no args and selects `/home/me/proj` in the dialog
- **THEN** the GUI opens with `/home/me/proj` as the working folder

#### Scenario: Cancel the dialog

- **WHEN** the user runs `diffcue` with no args and cancels the folder picker dialog
- **THEN** the process exits with code 0 and no window appears

### Requirement: Unknown flag rejection

Any argument that is not a recognized flag, a recognized flag's value, or a single
positional folder SHALL cause the system to print an error to stderr and exit with code 2.

#### Scenario: Unknown flag

- **WHEN** the user runs `diffcue --frobnicate /home/me/proj`
- **THEN** the system prints `error: unknown flag: --frobnicate` to stderr and exits
  with code 2

#### Scenario: Too many positionals

- **WHEN** the user runs `diffcue /home/me/proj /home/me/other`
- **THEN** the system prints `error: expected at most one folder, got 2` to stderr and
  exits with code 2
