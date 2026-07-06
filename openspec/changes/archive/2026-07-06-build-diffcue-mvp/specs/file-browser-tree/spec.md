## ADDED Requirements

### Requirement: Folder tree of the opened folder

The system SHALL render a left-panel tree view of the opened working folder, lazily
expanding subdirectories on demand (not eager full-tree scan). The tree root SHALL be
the absolute path of the opened folder.

#### Scenario: Open a folder with subdirectories

- **WHEN** the user opens `/home/me/proj` and the folder contains `src/`, `tests/`, and
  `README.md`
- **THEN** the file browser shows the root with three children: `src/`, `tests/`, and
  `README.md`, with directories marked as expandable

#### Scenario: Expand a directory

- **WHEN** the user clicks the expand arrow next to `src/`
- **THEN** the children of `src/` are enumerated and inserted into the tree without
  expanding any sibling directories

### Requirement: Git status annotation per file

Every file in the tree SHALL be annotated with its git status using a short code and a
color. Supported codes MUST include: `M` (modified, yellow), `A` (added, green), `D`
(deleted, red), `R` (renamed, blue), `?` (untracked, gray). The status SHALL come from
`git status --porcelain=v1` semantics (via libgit2 or the `git` CLI fallback).

#### Scenario: Modified file

- **WHEN** `src/main.cpp` has unstaged modifications
- **THEN** the file browser shows `main.cpp [M]` with `[M]` in yellow

#### Scenario: Untracked file

- **WHEN** `notes/draft.txt` exists on disk but is not tracked by git
- **THEN** the file browser shows `draft.txt [?]` with `[?]` in gray

### Requirement: Aggregated git status per folder

Every directory node SHALL display an aggregated status derived from its descendants.
A directory is considered: `M` if any descendant is modified/added/deleted/renamed; or
`?` if it contains only untracked files; or clean (no badge) if all descendants are clean
or ignored.

#### Scenario: Folder with mixed changes

- **WHEN** `src/` contains one modified file and one clean file
- **THEN** the `src/` node shows `[M]` in yellow, indicating some descendant is modified

#### Scenario: Clean folder

- **WHEN** `vendor/` contains only files tracked and unchanged in git
- **THEN** the `vendor/` node shows no status badge

### Requirement: Open file on click

Clicking a file node in the tree SHALL open that file in the diff viewer. Clicking a
directory node SHALL only expand/collapse it; it SHALL NOT open a diff.

#### Scenario: Click a modified file

- **WHEN** the user clicks `src/main.cpp [M]`
- **THEN** the diff viewer loads and displays the file's diff

#### Scenario: Click a directory

- **WHEN** the user clicks the `src/` row (not its expand arrow)
- **THEN** the directory expands or collapses and no diff is loaded

### Requirement: Only show git-relevant files by default

By default the tree SHALL hide files that are git-ignored or git-clean. A toolbar toggle
"Show all" SHALL reveal every entry (including ignored and clean files), preserving the
collapsed/expanded state of currently visible nodes.

#### Scenario: Default view

- **WHEN** the user opens a folder and the default filter is active
- **THEN** only files/directories that are modified, added, deleted, renamed, or
  untracked appear in the tree

#### Scenario: Toggle show all

- **WHEN** the user toggles "Show all" on
- **THEN** clean and ignored files appear with a dim style; toggling off hides them again
  without losing the expansion state of git-relevant directories

### Requirement: File count badge on root

The root node SHALL display the count of git-relevant files under it, e.g.
`/home/me/proj (12)`, so the reviewer can see at a glance how many files need review.

#### Scenario: 12 changed files

- **WHEN** the user opens a folder containing 12 git-relevant files
- **THEN** the root node displays `(12)` next to its path
