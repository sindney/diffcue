## Purpose

Persist and surface the user's most recently opened folders (up to 10) in a File ▸ Open Recent submenu and the command palette; opening any folder records it.

## Requirements

### Requirement: Persist recent folders

The system SHALL maintain a most-recently-first, deduplicated list of the user's opened folders in `prefs.json` under a `recent_folders` key (array of absolute path strings). The list SHALL hold at most 10 entries; when an 11th distinct folder is opened, the oldest entry SHALL be dropped. Opening a folder by ANY entrance (CLI positional argument, `Open Folder...` picker, drag-and-drop, or `Open Recent` selection) SHALL record that folder in the list: if the folder's canonical path is already present, it SHALL be moved to position 0; otherwise it SHALL be inserted at position 0. Folders that no longer exist on disk at app-startup time SHALL be filtered out of the loaded list.

#### Scenario: First folder open

- **WHEN** the user opens `/home/me/proj-a` for the first time
- **THEN** `prefs.json`'s `recent_folders` becomes `["/home/me/proj-a"]`

#### Scenario: Reopen a folder moves it to the top

- **WHEN** `recent_folders` is `["/a", "/b", "/c"]` and the user opens `/c`
- **THEN** `recent_folders` becomes `["/c", "/a", "/b"]`

#### Scenario: Cap at 10 entries

- **WHEN** `recent_folders` already has 10 entries and the user opens a new distinct folder `/k`
- **THEN** `recent_folders` becomes `["/k", <the previous 9 entries in their previous order>]` — the 10th-oldest entry is dropped

#### Scenario: Missing folder is dropped at load

- **WHEN** `prefs.json` lists `/a`, `/deleted`, `/b` and `/deleted` no longer exists on disk when diffcue starts
- **THEN** the in-memory `recent_folders` is `["/a", "/b"]` and the next save persists that trimmed list

#### Scenario: Drag-drop also records

- **WHEN** the user drags a folder onto the diffcue window to open it
- **THEN** that folder is recorded in `recent_folders` exactly as if it had been opened via the picker

### Requirement: Open Recent submenu

The `File` menu SHALL contain an `Open Recent` submenu that lists every entry in `recent_folders` (most-recent-first), up to the 10-entry cap. Each entry's label SHALL be the folder's absolute path. Selecting an entry SHALL open that folder via the SAME code path as `Open Folder...` (canonicalize, set title, load prefs, init cue store for that folder, clear blob cache, refresh git status, reset current file/diff, apply prefs). If the list is empty, the submenu SHALL show a disabled `No recent folders` row. If the user selects an entry whose folder no longer exists on disk, the system SHALL show an error popup (`error: folder not found: <path>`) and remove that entry from the list; no other state SHALL change.

#### Scenario: Open a recent folder

- **WHEN** `recent_folders` is `["/a", "/b"]` and the user clicks `File ▸ Open Recent ▸ /b`
- **THEN** diffcue opens `/b` (same effect as `Open Folder...` picking `/b`) and `/b` is moved to position 0 of the list

#### Scenario: Empty recent list

- **WHEN** `recent_folders` is empty and the user opens `File ▸ Open Recent`
- **THEN** the submenu shows a single disabled `No recent folders` row and no entry can be selected

#### Scenario: Stale entry removed on selection

- **WHEN** the user clicks `File ▸ Open Recent ▸ /deleted` and `/deleted` no longer exists
- **THEN** an error popup `error: folder not found: /deleted` is shown, `/deleted` is removed from `recent_folders`, and the currently open folder is unchanged

### Requirement: Recent folders shared with the command palette

Every entry in `recent_folders` SHALL also be reachable from the command palette (see the `command-palette` capability): typing a substring of the folder path filters the recent-folder commands, and running one SHALL have the same effect as selecting it from `Open Recent`. The palette SHALL NOT maintain a separate recent list — both entrances read from the same `recent_folders` source.

#### Scenario: Open a recent folder via the palette

- **WHEN** the user opens the command palette and runs the entry for `/b` (filtered by typing "b")
- **THEN** diffcue opens `/b` identically to clicking `File ▸ Open Recent ▸ /b`, and `/b` is moved to position 0 of `recent_folders`
