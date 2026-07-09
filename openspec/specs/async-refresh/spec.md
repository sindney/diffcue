## Purpose

Run `git status` refresh on a background worker so the UI thread never blocks on git; sync the result back to the UI thread in a single atomic step once the git op finishes. Coalesce overlapping refresh requests, surface a refreshing state in the toolbar, and join the worker cleanly on shutdown.

## Requirements

### Requirement: Git status refresh runs off the UI thread

The system SHALL execute `git status` and all in-memory derivation steps that depend on its output (file-tree build, hunk collection, cue stale-flag evaluation) on a background worker thread. The UI render thread SHALL NOT block on `git status` or any refresh-derivation step.

#### Scenario: Large repo refresh does not freeze the UI

- **WHEN** the user opens a repository with tens of thousands of changed files and a refresh is triggered
- **THEN** the UI continues to paint at its normal frame rate during the refresh and remains responsive to input (toolbar buttons, dock interactions, command palette)

#### Scenario: Refresh button click during an in-flight refresh

- **WHEN** a refresh is already running on the background worker and the user clicks the toolbar "Refresh" button again
- **THEN** the UI thread does not block and the in-flight refresh continues to completion; the second click is recorded as a pending request and a single follow-up refresh runs after the in-flight one finishes

### Requirement: Refresh result is applied to the UI thread in a single atomic step

The system SHALL produce the entire result of a refresh (changed-file entries, file tree, hunk list, and per-cue stale-flag updates) as one opaque struct on the background worker. The UI thread SHALL swap this struct into the live app state in a single step, and SHALL NOT observe any intermediate or partial refresh state. Intermediate git output SHALL NOT be visible to any UI component at any time.

#### Scenario: File browser never shows a half-built tree

- **WHEN** a refresh is in progress and the file browser is rendered
- **THEN** the file browser renders either the previous complete file tree or the new complete file tree, never a mix of the two or a partially-populated tree

#### Scenario: Cue stale flags update together with file list

- **WHEN** a refresh completes and its result is applied
- **THEN** the changed-file entries, file tree, hunk list, and cue stale flags all update in the same frame; there is no frame in which the new file list is visible but cue stale flags still reflect the previous refresh

### Requirement: Overlapping refresh requests are coalesced

The system SHALL ensure that at most one refresh runs on the background worker at any time. If one or more additional refresh requests arrive while a refresh is in flight, the system SHALL coalesce them into a single follow-up refresh that runs after the in-flight refresh completes. The system SHALL NOT queue more than one pending follow-up refresh.

#### Scenario: Rapid burst of focus-regain events

- **WHEN** the user Alt-Tabs away from and back to the diffcue window five times in rapid succession while no refresh is running
- **THEN** the worker runs one refresh; if any of the focus events arrive after the refresh starts, they are coalesced into at most one additional refresh that runs after the first completes

#### Scenario: Refresh request arrives while result is unconsumed

- **WHEN** the background worker finishes a refresh, the UI thread has not yet consumed the result, and a new refresh request arrives
- **THEN** the worker waits for the UI thread to consume the previous result before starting the new refresh; the system does not overwrite an unconsumed result

### Requirement: UI feedback while a refresh is in flight

The system SHALL surface a visible "refreshing" state in the toolbar while a refresh is running on the background worker. The state SHALL clear when the result has been applied to the UI thread (or when the worker has no work to do).

#### Scenario: Toolbar shows refreshing state

- **WHEN** a refresh is triggered (by toolbar button, command palette, focus-regain, or folder open) and the background worker is running it
- **THEN** the toolbar renders the Refresh button in a refreshing visual state (e.g. disabled with a "Refreshing..." label)

#### Scenario: Refreshing state clears on completion

- **WHEN** the background worker finishes the refresh and the UI thread has applied the result
- **THEN** the toolbar Refresh button returns to its normal idle visual state on the next frame

### Requirement: Folder open does not block the UI thread

The system SHALL trigger an async refresh as the final step of opening a folder, using the same background-worker path as all other refresh triggers. The UI thread SHALL NOT block waiting for the first refresh to complete after a folder is opened.

#### Scenario: Open a very large repository

- **WHEN** the user opens a repository whose `git status` takes multiple seconds to complete
- **THEN** the diffcue window appears immediately, the UI remains responsive, and the file browser shows a loading state until the first refresh result is applied

### Requirement: Stale result is never applied after a folder switch

The system SHALL discard a refresh result whose target folder no longer matches the currently-opened folder. When a folder switch occurs while a refresh for a previous folder is in flight, the result from the previous folder SHALL be dropped and SHALL NOT overwrite the state of the newly-opened folder.

#### Scenario: Switch folders while a refresh is in flight

- **WHEN** the user opens folder A, a refresh for A is running on the worker, and the user then opens folder B
- **THEN** when A's refresh completes, its result is discarded because it does not match the currently-opened folder B, and B's state is not overwritten by A's result

### Requirement: Clean shutdown joins the background worker

The system SHALL join the background worker thread during `App` shutdown. The destructor SHALL set a shutdown flag, wake the worker if idle, allow any in-flight refresh to complete, and join the worker thread before returning. No `git status` subprocess SHALL outlive `App`'s members.

#### Scenario: Quit while a refresh is in flight

- **WHEN** the user quits diffcue (window close or Ctrl+Q) while a refresh is running on the background worker
- **THEN** the in-flight refresh is allowed to complete, the worker thread is joined, and the process exits without leaking the `git status` subprocess or accessing destroyed `App` members

### Requirement: Portable linking of the threading standard library

The build system SHALL link the C++ threading standard library (`Threads::Threads`) into the `diffcue_lib` target so that `std::thread`, `std::future`, `std::mutex`, and `std::condition_variable` link portably across the supported platforms (macOS, Linux).

#### Scenario: Clean build on macOS and Linux

- **WHEN** the project is configured with CMake and built on macOS or Linux
- **THEN** the build succeeds without unresolved-symbol errors for `std::thread`-related symbols, and the resulting `diffcue` executable starts and runs the background worker
