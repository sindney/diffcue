## Context

`App::refresh_git_status()` (`src/app/app.cpp:138-158`) currently runs on the ImGui render thread. It shells out to `git status --porcelain=v1 -z -uall` via the blocking `popen` wrapper `platform::subprocess::run_capture` (`src/platform/subprocess.cpp:68-86`), then synchronously rebuilds `entries_`, `file_tree_`, `all_hunks_`, and updates cue stale flags. On large repos the `git status` call alone can take hundreds of milliseconds to seconds; during that window the render loop is frozen — no painting, no input, OS beachball. R8 in `openspec/changes/archive/2026-07-06-build-diffcue-mvp/design.md:319-321` called this out and MVP task 9.6 (`tasks.md:86`) was marked `[x]` without implementing the `std::async` half — only the focus-regain trigger landed.

Refresh is triggered from four sites (all on the UI thread):
- `App::open_folder()` (`app.cpp:166`) — initial load.
- Window focus regain (`app.cpp:478-483`) — atomic flag set by GLFW callback, polled next frame.
- Toolbar "Refresh" button (`toolbar_panel.cpp:41` → `app.cpp:643-645`).
- Command palette "Refresh" entry (`command_palette.cpp:54` → `app.cpp:621-622`).

There is no debouncing. The codebase is essentially single-threaded: the only threading primitives are a defensive mutex on the blob cache (`g_blob_mutex` in `git_adapter.cpp:21`) and an atomic window-focus flag (`g_window_focused` in `app.cpp:43`). `CMakeLists.txt` does not link `Threads::Threads`. C++17 is the language standard.

## Goals / Non-Goals

**Goals:**
- The ImGui render loop never blocks on `git status`. Refresh work — including the `git status` subprocess, `build_file_tree`, `collect_all_hunks`, and `CueStore::refresh_stale`'s line-count probe — runs on a background thread.
- The UI thread only ever sees a complete refresh result: it swaps `entries_`/`file_tree_`/`all_hunks_`/cue-stale-flags in one step after the git op finishes. No partial / intermediate state is ever visible.
- Overlapping refresh triggers are coalesced — if N refresh requests arrive while one is in flight, exactly one follow-up refresh runs after the in-flight one completes.
- `App` shutdown cleanly joins the worker so an in-flight `git status` never outlives `App`'s members.
- The user gets visual feedback (toolbar "refreshing" state) that a refresh is running.

**Non-Goals:**
- Cancelling an in-flight `git status` subprocess. There is no portable way to kill a `popen` child mid-stream; we accept that the in-flight run completes and then a coalesced follow-up runs.
- Reloading the currently-open diff (`current_diff_`) on refresh. Today's `refresh_git_status()` does not touch `current_diff_`; this change preserves that. Whether to auto-reload the open file is a separate UX decision.
- Debouncing refresh triggers with a timer. Coalescing via a single-slot "pending" flag is sufficient — rapid bursts collapse to one follow-up refresh, and `git status` itself is the natural rate-limiter.
- A general-purpose thread pool or task queue. One dedicated worker thread handles all refresh work; nothing else in the app needs background execution today.
- Switching from `git` CLI to libgit2. Out of scope — the subprocess layer stays.

## Decisions

### D1. Dedicated worker thread, not `std::async`
**Choice:** A single `std::thread` worker owned by `App`, with a condition-variable wake-up, a single-slot pending-request flag, and a single-slot completed-result slot. The UI thread polls the completed slot once per frame.

**Why not `std::async`:** `std::async` with `std::launch::async` spawns a new thread per refresh. Focus-regain bursts (Alt-Tabbing) would create and tear down threads repeatedly, and the per-call future bookkeeping makes coalescing awkward. A long-lived worker gives us a natural place to put the single-slot request queue and the shutdown handshake, and it amortizes thread creation across the app lifetime.

**Alternative considered:** `std::async` + a `std::future` polled each frame. Rejected because coalescing logic would have to live on the UI thread side (track an outstanding future, decide whether to launch another), and the worker-thread model makes the "only one refresh in flight at a time" invariant structural rather than convention-based.

### D2. Single-slot backpressure on both request and result sides
**Choice:** Two single-slot handoffs between UI thread and worker:
- **Request slot** (`std::atomic<bool> request_pending_` + `std::mutex`/`std::condition_variable` for wake): UI thread sets it to request a refresh. Worker clears it before running. If the UI thread sets it again while the worker is running, the worker sees it set on its next loop iteration and runs once more — this is the coalescing.
- **Result slot** (`std::atomic<bool> result_ready_` + `std::optional<RefreshResult>` under `std::mutex`): worker moves the result in, sets the flag (release). UI thread checks the flag (acquire) each frame; when set, takes the optional and applies it. If the worker finishes a refresh and the previous result is still unconsumed, the worker does **not** start a new refresh — it waits for the UI to consume first. This is intentional backpressure: we never produce faster than the UI consumes.

**Why:** Single-slot keeps the state machine tiny (two atomics, two mutexes, one CV). A queue would let refreshes pile up; the single-slot collapses bursts to at most "one running + one pending".

**Alternative considered:** A `std::queue<RefreshRequest>` + `std::queue<RefreshResult>`. Rejected — queues let stale refreshes stack, and the latest one is the only one that matters.

### D3. Result struct shape
**Choice:**
```cpp
struct RefreshResult {
    std::vector<git::GitEntry> entries;
    std::unique_ptr<model::FileTreeNode> file_tree;
    std::vector<HunkRef> hunks;
    // path -> (side, stale) for every cue that existed when the worker
    // snapshotted. UI thread applies by lookup; tolerant of cue add/remove
    // that happened on the UI thread while the refresh was in flight.
    std::unordered_map<std::string, std::pair<model::Side, bool>> cue_stale;
};
```
The worker snapshots the current `cues_->cues()` (file + side + line only) into a small local vector at refresh start, runs `refresh_stale` against that snapshot, and builds the `cue_stale` map. The UI thread applies the map to the live `cues_` by looking each cue up by `(file, side)`.

**Why map by `(file, side)`, not by index:** If the user adds or removes a cue between the worker's snapshot and the UI-thread apply, indices would be wrong. Keying by `(file, side)` is robust to that — a cue added mid-refresh simply isn't in the map and keeps its current `stale` flag, which is correct (its staleness was just evaluated when it was added).

**Alternative considered:** Have the worker hold a pointer to `cues_` and mutate it directly. Rejected — `cues_` is read by the UI thread every frame (file browser, command palette, toolbar count), so direct mutation from the worker races the renderer.

### D4. `git::clear_blob_cache()` runs on the UI thread, not the worker
**Choice:** The swap-in step on the UI thread calls `git::clear_blob_cache()` immediately before moving the new `entries`/`file_tree`/`hunks` into place. The worker never calls it.

**Why:** `g_blob_mutex` makes `clear_blob_cache()` safe to call from any thread, but if the worker calls it while the UI thread is mid-`build_file_diff()` (which calls `read_blob_old`), the UI thread takes a cache-miss and shells out to `git show HEAD:<path>` again — a perf hit, not a correctness bug, but pointless given we can just clear on the UI thread at the natural swap point. The blob cache is keyed by `(path, HEAD)` and the new `entries_` reflects the new HEAD state, so clearing at swap time is exactly right.

### D5. `open_folder()` uses the same async path
**Choice:** `open_folder()` requests an async refresh as its last step, exactly like the other triggers. The file browser shows a "Loading…" placeholder while `file_tree_` is empty (or the previous folder's tree, until the new one lands).

**Why:** Keeping `open_folder()` on the sync path would mean two refresh code paths — one sync, one async — doubling the surface area for bugs and undoing the "UI thread never blocks on git" goal the moment a user opens a large repo.

**Alternative considered:** Make `open_folder()` wait for the first refresh to complete before returning. Rejected — it reintroduces the jank on folder open, which is the worst-case path (the user just picked a huge repo and the app freezes).

### D6. Shutdown handshake
**Choice:** `~App()` does:
1. `shutdown_.store(true, release)`.
2. Notify the worker's condition variable (so it wakes if idle).
3. If a refresh is in flight, the worker is allowed to finish it (we don't kill the `popen`); the worker checks `shutdown_` after each loop iteration and exits.
4. `worker_.join()`.

The `App` members the worker touches (`folder_`, the cue snapshot it took) are kept alive until `join()` returns. The worker never touches `entries_`/`file_tree_`/`cues_` directly (it only writes to the result slot), so there's no use-after-free on shutdown as long as `join()` happens before those members are destroyed.

**Why:** Joining is the only way to guarantee the worker isn't mid-`popen` when `App`'s destructor returns. Allowing the in-flight refresh to finish (rather than abandoning the thread) avoids leaking a `popen` handle.

### D7. Refreshing UI state
**Choice:** `std::atomic<bool> refresh_in_flight_` set by the worker (true on start, false on result publish). The toolbar reads it via an `App::is_refreshing()` getter and renders the Refresh button in a disabled/"⟳" state. No new external state struct on `ToolbarActions` — a getter is enough.

**Why:** Atomic is cheap to read per frame. The worker is the single writer, the UI thread is the single reader — no mutex needed for this flag.

## Risks / Trade-offs

- **[Worker outlives `App` members on abnormal exit]** → Mitigated by D6: explicit `shutdown_` flag + `join()` in `~App()`. The worker only ever reads `folder_` (a `std::filesystem::path` copyable member) and the cue snapshot it took at refresh start; it never dereferences `App`-owned pointers. As long as `join()` completes before `App`'s members are destroyed, there is no UB.
- **[Stale result applied after folder switch]** → If the user opens folder A, then folder B while A's refresh is in flight, A's result must not overwrite B's state. Mitigation: the result struct carries the `folder` it was computed for; the UI thread's apply step compares it to `folder_` and discards mismatches. Also, `open_folder(B)` should clear any pending result slot before requesting B's refresh, so A's late result is dropped.
- **[Cues mutated mid-refresh]** → A cue added on the UI thread while the worker is mid-`refresh_stale` is not in the worker's snapshot, so it won't be in the `cue_stale` map. Mitigated by D3's `(file, side)`-keyed apply: an unmapped cue keeps its current `stale` flag, which is correct (it was just evaluated when added). A cue removed mid-refresh simply has no match on apply — also correct.
- **[Thread start failure]** → `std::thread` constructor can throw `std::system_error` if the OS can't create the thread. Mitigation: catch in `App` constructor, fall back to synchronous refresh (the original code path) and log a warning. The async path is a perf improvement, not a correctness requirement.
- **[Linker error on `std::thread`]** → Mitigated by adding `find_package(Threads REQUIRED)` + `target_link_libraries(diffcue_lib PUBLIC Threads::Threads)` in `CMakeLists.txt`. Without this, `std::thread` may fail to link on some Linux toolchains (macOS with libc++ typically links without `-pthread`, but the CMake change is portable).
- **[No cancellation means a stuck `git status` blocks the worker]** → If `git status` hangs (e.g., NFS repo, corrupt index), the worker is stuck and no further refreshes run until it returns. Trade-off accepted — adding subprocess timeout is a separate concern. The UI thread is unaffected (still responsive), only the refresh is stalled. The "refreshing" indicator stays on, giving the user a visible signal that something is hung.
- **[First-frame jank on folder open]** → With D5, the file browser briefly shows "Loading…" instead of files. This is strictly better than the current behavior (freezing the whole app), but is a visible behavior change for users who previously saw files appear instantly (on small repos where sync refresh was fast). Trade-off accepted.

## Migration Plan

This is an internal refactor — no on-disk format changes, no user-facing API changes, no config migration. Rollout is a single PR; no feature flag needed.

Rollback: revert the PR. The sync refresh code path is preserved in git history and can be restored if the async path introduces regressions. The only build-system change (`Threads::Threads`) is harmless if the async code is removed.

## Open Questions

None — all decisions above are settled for this change. The "should refresh also reload `current_diff_`?" question is explicitly deferred (Non-Goal) and would be a separate change.
