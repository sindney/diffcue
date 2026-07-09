## Why

Today `App::refresh_git_status()` runs `git status --porcelain=v1 -z -uall` synchronously on the UI thread (`src/app/app.cpp:138-158`, `src/git/git_adapter.cpp:42-86`, `src/platform/subprocess.cpp:68-86`). On large repos the `git status` call can take hundreds of milliseconds to seconds, and it is triggered on every window focus regain (`src/app/app.cpp:478-483`) and on every toolbar/palette "Refresh" click. During the call the entire ImGui render loop is blocked, so the window stops painting, dock interactions lag, and the OS shows the "spinning beachball" — exactly the jank R8 in `openspec/changes/archive/2026-07-06-build-diffcue-mvp/design.md:319-321` warned about and that MVP task 9.6 (`tasks.md:86`) marked complete without actually implementing the `std::async` half. This change delivers the original R8 mitigation: refresh runs off the UI thread, and only the final result is synced back to the UI thread in one atomic swap.

## What Changes

- Move `git status` invocation (and the in-memory derivation steps that depend on its output — `model::build_file_tree`, `collect_all_hunks`, and `CueStore::refresh_stale`'s line-count probe) off the UI thread onto a background worker.
- Introduce a single refresh result struct produced by the background worker, containing: `std::vector<git::GitEntry> entries`, `std::unique_ptr<model::FileTreeNode> file_tree`, `std::vector<HunkRef> hunks`, and per-cue stale-flag updates.
- The UI thread polls for a completed result each frame and, when one is present, swaps it into `entries_`/`file_tree_`/`all_hunks_`/`cues_` in a single step. Intermediate git output is never visible to the UI.
- Coalesce concurrent refresh requests: if a refresh is already in flight, a second request is recorded as pending and a single follow-up refresh runs after the in-flight one completes (no stacking, no debouncing timer needed).
- Add `find_package(Threads REQUIRED)` and link `Threads::Threads` to `diffcue_lib` in `CMakeLists.txt` so `std::thread`/`std::future` link portably.
- Surface a lightweight "refreshing..." indicator in the toolbar (spinner or disabled Refresh button state) while a refresh is in flight, so the user gets feedback that the jank is gone and work is happening.
- Keep `git::clear_blob_cache()` callable from the background thread — its `g_blob_mutex` (`src/git/git_adapter.cpp:21`) already makes this safe; the swap-in step on the UI thread will call it before swapping `entries_`.

## Capabilities

### New Capabilities
- `async-refresh`: Background execution of git status refresh with single-shot atomic result sync to the UI thread; request coalescing for overlapping refresh triggers; UI feedback while a refresh is in flight.

### Modified Capabilities
<!-- None. The file-browser-tree, diff-viewer, command-palette, and review-cues specs describe behavior of the data they consume, not how that data is refreshed. Their existing requirements remain valid unchanged. -->

## Impact

- **Code**:
  - `src/app/app.h`, `src/app/app.cpp` — rewrite `refresh_git_status()` into a request/result-pump shape; add result struct, in-flight state, pending-request flag, and a per-frame poll in `App::run()`. Update the three trigger sites (focus-regain `app.cpp:478-483`, toolbar/palette `app.cpp:643-645`, `open_folder` `app.cpp:166`) to request an async refresh instead of calling sync.
  - `src/git/git_adapter.{h,cpp}` — no change to `list_changes` signature; it is already pure and reentrant given the blob-cache mutex.
  - `src/platform/subprocess.cpp` — no change; `run_capture` stays blocking but is now called from the worker thread.
  - `src/ui/toolbar_panel.{h,cpp}` — add a "refreshing" visual state driven by an `is_refreshing` flag passed in via `ToolbarActions`/context.
  - `src/model/cue_store.{h,cpp}` — `refresh_stale` signature unchanged; it is invoked from the worker thread now. The line-count callback it receives already only touches `git::read_blob_new` (file I/O, no shared mutable state).
  - `CMakeLists.txt` — add `Threads::Threads`.
- **APIs**: No public API changes. All changes are internal to the `diffcue_lib` static lib / executable.
- **Dependencies**: No new third-party deps. Uses only `std::thread`/`std::future`/`std::mutex`/`std::atomic` from the C++17 standard library.
- **Risks**:
  - Lifetime of the background worker vs. `App` shutdown — must join the worker in `~App()` so an in-flight `git status` does not outlive `App`'s members.
  - `git::clear_blob_cache()` must be called on the UI thread side of the swap (not on the worker) so that an in-progress `App::build_file_diff()` on the UI thread never sees its blob evicted mid-call.
  - Cues mutated by the worker's `refresh_stale` run must be applied to the `cues_` the UI is currently rendering — see design for the snapshot-and-apply approach.
- **Spec drift closed**: implements the unimplemented `std::async` half of MVP task 9.6 / R8.
