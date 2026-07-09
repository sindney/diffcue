## 1. Build system

- [x] 1.1 Add `find_package(Threads REQUIRED)` to `CMakeLists.txt` after the existing `find_package` calls
- [x] 1.2 Add `Threads::Threads` to `target_link_libraries(diffcue_lib ...)` (PUBLIC so the executable inherits it)
- [x] 1.3 Reconfigure and build cleanly with no unresolved-symbol errors; confirm `std::thread`/`std::mutex`/`std::condition_variable` compile and link

## 2. RefreshResult struct and App worker state

- [x] 2.1 Add `#include <atomic>`, `#include <thread>`, `#include <mutex>`, `#include <condition_variable>`, `#include <unordered_map>` to `src/app/app.cpp` (and `app.h` where declarations need them)
- [x] 2.2 Define `struct App::RefreshResult { std::filesystem::path folder; std::vector<git::GitEntry> entries; std::unique_ptr<model::FileTreeNode> file_tree; std::vector<HunkRef> hunks; std::unordered_map<std::string, std::pair<model::Side,bool>> cue_stale; };` in `app.h` (private nested type)
- [x] 2.3 Add worker-state members to `App` (private): `std::thread worker_; std::mutex worker_mutex_; std::condition_variable worker_cv_; std::atomic<bool> request_pending_{false}; std::atomic<bool> shutdown_{false}; std::mutex result_mutex_; std::optional<RefreshResult> result_; std::atomic<bool> result_ready_{false}; std::atomic<bool> refresh_in_flight_{false};`
- [x] 2.4 Declare private methods on `App`: `void worker_loop(); RefreshResult compute_refresh(); void request_refresh(); void apply_refresh_result(RefreshResult&&); bool try_take_result();`

## 3. Worker thread implementation

- [x] 3.1 Implement `App::compute_refresh()` — move the body of the current `refresh_git_status()` (git status call, `build_file_tree`, `collect_all_hunks`, cue-stale evaluation via `refresh_stale` against a snapshotted cue list) into this function, returning a `RefreshResult` populated on the worker thread. Populate `result.folder` with the current `folder_` so the apply step can detect a stale folder switch.
- [x] 3.2 Implement `App::worker_loop()` — loops while `!shutdown_`: waits on `worker_cv_` until `request_pending_` or `shutdown_`; clears `request_pending_`; sets `refresh_in_flight_`; checks that no prior unconsumed result exists (waits otherwise — single-slot backpressure); calls `compute_refresh()`; moves result into `result_` under `result_mutex_`; sets `result_ready_` (release) and clears `refresh_in_flight_`.
- [x] 3.3 Implement `App::request_refresh()` — locks `worker_mutex_`, sets `request_pending_`, notifies `worker_cv_`. Callable from the UI thread at any of the trigger sites.
- [x] 3.4 Start the worker in the `App` constructor (after `folder_` and `cues_` are initialised, before `open_folder`/first refresh); wrap thread creation in try/catch — on `std::system_error`, fall back to synchronous refresh and log a warning.

## 4. UI-thread result pump

- [x] 4.1 Implement `App::try_take_result()` — if `result_ready_.load(acquire)` is false, return false; otherwise lock `result_mutex_`, move `result_` out, clear `result_ready_`, return true.
- [x] 4.2 Implement `App::apply_refresh_result(RefreshResult&& r)` — if `r.folder != folder_`, discard (folder switched). Otherwise: call `git::clear_blob_cache()`; `entries_ = std::move(r.entries)`; `file_tree_ = std::move(r.file_tree)`; `all_hunks_ = std::move(r.hunks)`; for each cue in `cues_->cues()`, look up `(file.generic_string(), side)` in `r.cue_stale` and set its `stale` flag if present.
- [x] 4.3 Add a per-frame call in `App::run()` near the focus-regain handling block (around `app.cpp:478-483`) to invoke `try_take_result()` and, if true, `apply_refresh_result(std::move(r))`. Place it before the focus-regain/toolbar dispatch so a freshly-arrived result is applied before new requests are evaluated.
- [x] 4.4 In `open_folder()`, before requesting the new folder's refresh, clear `result_.reset()` under `result_mutex_` and `result_ready_.store(false)` so a stale result from the previous folder is dropped on arrival.

## 5. Rewire trigger sites to request async refresh

- [x] 5.1 Replace the body of `App::refresh_git_status()` to call `request_refresh()` only. Keep the public signature so callers (`open_folder`, focus-regain, toolbar, palette) are unchanged. (Alternatively rename and inline — pick whichever minimises call-site churn; document the choice in code.)
- [x] 5.2 Verify focus-regain block (`app.cpp:478-483`) still calls `refresh_git_status()` (now async) — no code change beyond what 5.1 does.
- [x] 5.3 Verify toolbar/palette dispatch (`app.cpp:643-645`) still calls `refresh_git_status()` — no code change.
- [x] 5.4 Verify `open_folder()` (`app.cpp:166`) still calls `refresh_git_status()` as its last substantive step — no code change beyond the result-slot clear in 4.4.

## 6. Toolbar refreshing UI state

- [x] 6.1 Add `bool App::is_refreshing() const` returning `refresh_in_flight_.load(relaxed)`. Declare it in `app.h`.
- [x] 6.2 In `ui::render_toolbar` (`src/ui/toolbar_panel.cpp:41`), pass the refreshing flag via the existing `App` context (or `ToolbarActions`) and render the Refresh button disabled and/or with a "⟳" label while `is_refreshing()` is true. Keep the click handler emitting `actions.refresh = true` so a click during refresh is coalesced (not lost) — the request simply queues.
- [x] 6.3 Verify the flag clears promptly when a refresh completes (next frame after `try_take_result` consumes).

## 7. Shutdown

- [x] 7.1 Implement `App::~App()` (or extend existing destructor): set `shutdown_.store(true, release)`; notify `worker_cv_`; if `worker_.joinable()` call `worker_.join()`.
- [x] 7.2 Verify no use-after-free: the worker only reads `folder_` (a value member) and its own local `RefreshResult` — confirm via read of `worker_loop()`/`compute_refresh()` that no `App`-owned pointer is dereferenced after `shutdown_` is observed.
- [x] 7.3 Manually test: trigger a refresh on a large repo and quit mid-refresh; confirm the process exits cleanly without leaking the `git status` subprocess (check `ps aux | grep git` after exit).

## 8. Tests

- [x] 8.1 Add a unit test that asserts `request_refresh()` while a refresh is in flight does not spawn a second worker thread (single in-flight invariant). Use a fake slow `git status` (e.g. a test double or a repo large enough to take >100ms).
- [x] 8.2 Add a unit/integration test that triggers two rapid refreshes and asserts at most one follow-up refresh runs (coalescing).
- [x] 8.3 Add a test that opens folder A, triggers a refresh, switches to folder B before A's result arrives, and asserts B's state is not overwritten by A's late result (folder-switch guard from 4.2).
- [x] 8.4 Add a test that asserts the UI-thread apply step is atomic: after `apply_refresh_result`, `entries_`, `file_tree_`, `all_hunks_`, and cue `stale` flags are all consistent with the same refresh (no partial state).
- [x] 8.5 Add a smoke test that builds, launches the app on a large repo, and verifies the UI remains responsive (frame time stays under ~50ms) during a refresh. Can be a manual test script if automated instrumentation is impractical.

## 9. Build verification and cleanup

- [x] 9.1 Run `cmake --build build --config Release` from a clean build directory; confirm no warnings or errors related to threading.
- [x] 9.2 Run the full test suite (`ctest --test-dir build`) and confirm all tests pass.
- [x] 9.3 Manual smoke test on the diffcue repo itself and on a large repo (e.g. a checkout of a major open-source project): verify no UI jank on focus-regain, toolbar, palette, and folder-open paths; verify the refreshing indicator appears and clears.
- [x] 9.4 Remove any debug logging added during implementation; ensure no leftover `std::cout`/`fprintf` debug prints.
