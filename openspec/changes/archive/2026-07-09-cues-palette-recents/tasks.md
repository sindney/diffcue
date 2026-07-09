## 1. Jump-to-cue scroll fix (smallest, highest-value; do first)

- [x] 1.1 In `src/app/app.cpp` `App::run()`, in the `tactions.jump_to_cue_index >= 0` handler (lines ~559–562), add `diff_viewer_.scroll_to_line(c.line);` immediately after the `open_file(c.file);` call. Verify the TextEditor ordering contract holds (SetText runs in `set_diff`, `scroll_to_line` marks a request, `Render` runs later in the frame at line ~631).
- [ ] 1.2 Manually verify: open a folder with cues in 2+ files, click a cue whose file is NOT the currently open file and whose line is past the first viewport — the diff viewer must load the file AND center the cue's line, not stay at the top. Also verify jumping to a cue in the currently open file centers its line.
- [x] 1.3 Build clean (`cmake --build build --config Release` or the existing build command) and fix any compile warnings.

## 2. Clear-cues modal keyboard shortcuts + label hints

- [x] 2.1 In `src/app/app.cpp` `App::run()`, in the `BeginPopupModal("Clear Cues?", ...)` block (lines ~537–549): rename the confirm button label from `"Clear"` to `"Clear (Enter)"` and the cancel button from `"Cancel"` to `"Cancel (Esc)"`.
- [x] 2.2 Inside the modal body, after the existing buttons (or before them), add an `if (ImGui::IsKeyPressed(ImGuiKey_Enter)) { cues_->clear(); ImGui::CloseCurrentPopup(); }` handler. Add `if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { ImGui::CloseCurrentPopup(); }` for explicit cancel. Use `ImGui::SetKeyboardFocusHere()` on the confirm button when the modal first opens so the default Enter behavior aligns with our explicit handler.
- [ ] 2.3 Verify: open the modal via the toolbar `Clear` button; pressing Enter clears cues and closes; pressing Esc closes without clearing; clicking either button still works; the labels display the shortcut hints.
- [x] 2.4 Build clean.

## 3. Refactor folder-open into `App::open_folder(path)` (prerequisite for recents + palette)

- [x] 3.1 Add a private `void App::open_folder(const std::filesystem::path& canonical)` method declaration to `src/app/app.h` (or wherever `App` is declared). The method takes an already-canonicalized path.
- [x] 3.2 Implement `open_folder` in `src/app/app.cpp` by extracting the common body from the three existing sites: CLI init (~lines 110–140), menubar picker (~468–485), and drag-drop (~431–445). The body: set `folder_`, set window title, `load_prefs`, `cues_.emplace(folder_)`, `git::clear_blob_cache()`, `refresh_git_status()`, clear `current_open_path_`, reset `current_diff_`, `diff_viewer_.clear()`, set `not_git_repo_`, `apply_prefs()`.
- [x] 3.3 Replace all three existing sites to call `open_folder(canonical)`. For the picker path, canonicalize the picked path before calling; for drag-drop, canonicalize `*g_dropped_folder`; for CLI init, the path is already canonical.
- [ ] 3.4 Verify the three entrances still work: launch with a CLI folder, use `File ▸ Open Folder...`, and drag-drop a folder onto the window — all three must open the folder correctly with title, prefs, cues, git status, and diff viewer reset.
- [x] 3.5 Build clean.

## 4. Recent-folders persistence in `prefs.json`

- [x] 4.1 Add `std::vector<std::filesystem::path> recent_folders;` to `struct Prefs` in `src/model/prefs.h`.
- [x] 4.2 In `src/model/prefs.cpp`, extend the hand-rolled JSON parser with a `parse_array` helper that reads `"recent_folders": ["path1", "path2", ...]` into a `std::vector<std::string>`. Extend the writer with a `write_array` helper that emits the array. Filter out non-existent paths at load time (`std::filesystem::exists`).
- [x] 4.3 In `App::open_folder` (the method created in task 3.2), after the folder is successfully opened (and verified to be a real directory), record it in `prefs_.recent_folders`: dedupe by canonical path (remove if present), insert at position 0, cap at 10. Then call `model::save_prefs(prefs_)` so the change persists immediately.
- [x] 4.4 Add a unit test (in the existing prefs test file, or create one if none exists) covering: 0, 1, 10, 11+ entries round-trip; dedup moves existing to position 0; cap-at-10 drops the oldest; missing-path filtering at load. The test uses the path-based `load_prefs(dir)` / `save_prefs(dir, prefs)` overloads.
- [x] 4.5 Build clean and run the prefs tests.

## 5. `Open Recent` menubar submenu

- [x] 5.1 Add `int open_recent_index = -1;` to `struct MenubarActions` in `src/ui/menubar.h` (index into `prefs.recent_folders`).
- [x] 5.2 In `src/ui/menubar.cpp`, inside the `File` menu, after `Open Folder...` and before the `Quit` separator, add `if (ImGui::BeginMenu("Open Recent"))`. If `prefs.recent_folders` is empty, render a disabled `ImGui::MenuItem("No recent folders", nullptr, false, false)`. Otherwise, render one `ImGui::MenuItem(path.generic_string().c_str())` per entry (most-recent-first); on click set `actions.open_recent_index = i`.
- [x] 5.3 In `src/app/app.cpp` `App::run()`, after handling `mactions.open_folder_clicked`, handle `mactions.open_recent_index >= 0`: look up the path in `prefs_.recent_folders`; if it doesn't exist on disk, show an error popup (`error: folder not found: <path>`) and remove that entry from `prefs_.recent_folders` + save prefs; otherwise call `open_folder(canonical)`.
- [ ] 5.4 Verify: open 3 distinct folders via any entrance; the `Open Recent` submenu lists them most-recent-first; clicking one opens it and moves it to position 0; opening an 11th distinct folder drops the oldest; deleting a folder on disk and selecting it from the menu shows the error popup and removes the entry.
- [x] 5.5 Build clean.

## 6. Hover-to-show cue list (replacing click-to-open + tooltip)

- [x] 6.1 In `src/ui/toolbar_panel.cpp`, change the cue button render so it ALWAYS renders (drop the `cues.count() > 0 &&` gate around `ImGui::Button(cue_label)`). Keep the `cues.count() > 0` check only for opening the popup (or always allow opening — see 6.3 for the empty-state row).
- [x] 6.2 Remove the `if (cues.count() > 0 && ImGui::IsItemHovered()) ImGui::SetTooltip("Click to list all cues.");` line — the tooltip is gone.
- [x] 6.3 Add a file-scope `static float cue_hover_timer = 0.0f;` accumulator. Each frame: if `ImGui::IsItemHovered()` (the cue button), add `ImGui::GetIO().DeltaTime`; else reset to 0. When the timer crosses ~0.3s AND the popup is not already open, call `ImGui::OpenPopup("cue_dropdown")`. Also keep the immediate-open-on-click behavior: if the button is clicked, open the popup instantly (so click is still a fast path).
- [x] 6.4 In the `BeginPopup("cue_dropdown")` block, handle the empty case: if `cues.count() == 0`, render `ImGui::Selectable("No cues", false, ImGuiSelectableFlags_Disabled);` and skip the loop. Otherwise render the existing per-cue selectables.
- [x] 6.5 Add a per-frame close-when-unhovered check: track whether the button OR the popup is hovered this frame (`ImGui::IsItemHovered()` on the button + a hover check on the popup region). If the popup is open AND neither is hovered AND a one-frame grace period has elapsed, call `ImGui::CloseCurrentPopup()`. (The grace period prevents the popup from closing as the mouse transits from the button into the popup.)
- [ ] 6.6 Verify: hovering the cue button for ~300ms opens the list without a click; moving the mouse into the list keeps it open; clicking an entry jumps and scrolls (task 1); moving the mouse away closes the list; `Cues: 0` button is always visible and hovering shows the "No cues" row.
- [x] 6.7 Build clean.

## 7. Copy Prompt rebind to Cmd+Shift+P

- [x] 7.1 In `src/app/app.cpp` `App::run()` keyboard-shortcut block (~lines 459–466 and the existing Cmd+P handler at ~571–576), remove the bare `Cmd+P → copy_prompt` handler. Add two new handlers in this order: (a) `if (ImGui::IsKeyPressed(ImGuiKey_P, false) && ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift) palette_open_ = !palette_open_;` (added in task 8 — leave as a stub or comment here), (b) `if (ImGui::IsKeyPressed(ImGuiKey_P, false) && ImGui::GetIO().KeyCtrl && ImGui::GetIO().KeyShift) tactions.copy_prompt = true;`. Check the shifted combo first so it doesn't fall through to the bare-P branch.
- [x] 7.2 Update the toolbar tooltip at `src/ui/toolbar_panel.cpp:103` from `Shortcut: Ctrl+P` to `Shortcut: Ctrl+Shift+P`.
- [ ] 7.3 Verify: pressing Cmd+Shift+P runs Copy Prompt (generates, fills pane, copies); pressing bare Cmd+P does NOT run Copy Prompt (it opens the palette once task 8 lands; until then it's a no-op).
- [x] 7.4 Build clean.

## 8. Command palette window + dispatch

- [x] 8.1 Create `src/ui/command_palette.h` declaring: `enum class PaletteCommand { NextChange, PrevChange, OpenFolder, OpenRecent, Refresh, ClearCues, CopyPrompt, JumpToCue, ToggleDiffMode, ToggleIgnoreEOL, ToggleFindBar, ShowAll, About, Quit };` and `struct PaletteActions { bool run = false; PaletteCommand command; int payload = -1; /* index for OpenRecent / JumpToCue */ };` and `bool render_command_palette(PaletteActions& out, bool& open_flag, const model::Prefs& prefs, const model::CueStore& cues);` (returns true when a command is run this frame; `open_flag` is the App-owned toggle so the function can close it).
- [x] 8.2 Create `src/ui/command_palette.cpp` implementing the palette: a `Begin("Command Palette", &open_flag, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)` window positioned at the top-center of the viewport via `SetNextWindowPos`. Inside: an `InputText` filter (buffered state internal to the file), a child region with `ImGuiListClipper`-driven `Selectable` rows filtered by case-insensitive substring match against display name. Track `selected_index_` across frames. Handle `Up`/`Down` (with wrap), `Enter` (set `out.run = true; out.command = ...; out.payload = ...;` and close), `Esc` (close without running), and re-pressing Cmd+P (close without running). Build the command list per-frame from: a static array of named commands + per-recent-folder entries (`Open Recent: <path>`) + per-cue entries (`Jump to Cue: <file>:<line> - <text>`).
- [x] 8.3 In `src/app/app.cpp`, add `bool palette_open_ = false;` to `App`. At the top of the frame (before the menubar/toolbar dispatch), if `palette_open_`, call `render_command_palette(paction, palette_open_, prefs_, *cues_)`. Wire the bare `Cmd+P` handler (task 7.1a) to toggle `palette_open_`.
- [x] 8.4 In `App::run()`, after the existing `ToolbarActions`/`MenubarActions` dispatch, add a `PaletteActions` dispatch block that maps `paction.command` onto the SAME handlers: `NextChange → tactions.next_change = true;`, `PrevChange → tactions.prev_change = true;`, `OpenFolder → mactions.open_folder_clicked = true;`, `OpenRecent → mactions.open_recent_index = paction.payload;`, `Refresh → tactions.refresh = true;`, `ClearCues → tactions.clear_cues = true;`, `CopyPrompt → tactions.copy_prompt = true;`, `JumpToCue → tactions.jump_to_cue_index = paction.payload;`, `ToggleDiffMode → tactions.diff_mode_toggled = true;` (flip prefs.diff_mode), `ToggleIgnoreEOL → tactions.ignore_eol_toggled = true;`, `ToggleFindBar → tactions.find_toggled = true;`, `ShowAll → mactions.show_all_toggled = true; mactions.show_all = !show_all_;`, `About → mactions.about_clicked = true;`, `Quit → window_.request_close();`. NO new effect code — every branch sets an existing flag.
- [x] 8.5 Add `command_palette.cpp` to the build (CMake `src/CMakeLists.txt` or wherever the other `src/ui/*.cpp` files are listed).
- [ ] 8.6 Verify the spec's "Shared action dispatch" scenarios: run `Next Change` from the palette and confirm the SAME `tactions.next_change` handler runs (add a temporary log or breakpoint if needed); run `Open Folder...` from the palette and confirm the native picker opens; run `Copy Prompt` from the palette and confirm the prompt is generated + copied. Confirm `Open Recent: <path>` entries appear in the palette filtered by the path substring, and running one opens the folder via `open_folder`. Confirm `Jump to Cue: ...` entries appear and running one fires the same `jump_to_cue_index` handler.
- [ ] 8.7 Verify keyboard: Cmd+P opens; Esc closes; Cmd+P again closes; Up/Down move selection with wrap; Enter runs the highlighted command and closes; typing filters live; "zzz" shows the empty state and Enter does nothing.
- [x] 8.8 Build clean.

## 9. Final integration verification

- [ ] 9.1 Run the full spec verification: walk through every scenario in `specs/review-cues/spec.md`, `specs/prompt-generation/spec.md`, `specs/recent-folders/spec.md`, `specs/command-palette/spec.md` and confirm each one behaves as written. Note any deviation in the change log.
- [ ] 9.2 Delete `prefs.json` (after backing it up) and relaunch diffcue: confirm the empty `recent_folders` list loads cleanly, the `Open Recent` menu shows `No recent folders`, and the palette shows no `Open Recent:` entries. Open a folder; confirm it's recorded and persists across restart.
- [ ] 9.3 Manually edit `prefs.json` to add a non-existent path to `recent_folders` and relaunch: confirm it's filtered out at load and the trimmed list is saved on the next folder open.
- [x] 9.4 Final clean build + run the existing test suite (`ctest` or the project's test command) + the new prefs array test from task 4.4.
