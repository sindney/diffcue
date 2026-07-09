## 1. Switch ImGui submodule to the docking branch

- [x] 1.1 `git -C thirdparty/imgui fetch origin docking`
- [x] 1.2 `git -C thirdparty/imgui checkout docking` and pin to the branch tip (recorded commit: `a23e9fb1b` "Merge branch 'master' into docking")
- [x] 1.3 Confirm `thirdparty/imgui/imgui_internal.h` exposes `DockBuilder*` symbols (grep confirmed at lines 3921-3934)
- [x] 1.4 Clean-build `imgui_static` — compiled the 7 sources cleanly, no errors/warnings
- [x] 1.5 Commit the new submodule gitlink pointer in the parent repo (folded into task 8.1)

## 2. Enable docking on the ImGui context

- [x] 2.1 In `src/platform/window.cpp` `Window::Window`, added `io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;` after the NavEnableKeyboard line
- [x] 2.2 Confirm `ViewportsEnable` is NOT set (single OS window for v1) — not added
- [ ] 2.3 Build and launch; confirm no visual regression yet (docking flag alone doesn't change layout) — **needs manual launch**

## 3. Add the dockspace to the main host window

- [x] 3.1 In `src/app/app.cpp`, submitted `ImGuiID dockspace_id = ImGui::GetID("MainDockSpace"); ImGui::DockSpace(dockspace_id, ImVec2(0, 0));` after toolbar + findbar children
- [ ] 3.2 Verify the dockspace fills the remaining area below toolbar/findbar (resize the window; dockspace should track) — **needs manual launch**

## 4. Seed the default dock layout (one-shot)

- [x] 4.1 Added helper `seed_default_dock_layout(ImGuiID dockspace_id)` in `app.cpp` (anonymous namespace) implementing the DockBuilder sequence (left ~20% Files, bottom ~25% Prompt, center Diff)
- [x] 4.2 Gated with `static bool seeded` + `ImGuiDockNode::IsEmpty()` check (stronger than the null check — `DockSpace` creates the node synchronously, so a null check alone would never fire; `IsEmpty()` distinguishes a fresh dockspace from one with a saved layout)
- [x] 4.3 Called `seed_default_dock_layout(dockspace_id)` immediately after `DockSpace(...)` each frame; no-ops once seeded

## 5. Convert content panels to dockable windows

- [x] 5.1 Replaced the `left` `BeginChild`/`EndChild` with `if (ImGui::Begin("Files", nullptr, ImGuiWindowFlags_NoCollapse)) { ... } ImGui::End();` — kept `factions.open_file` handling
- [x] 5.2 Replaced the `center` `BeginChild`/`EndChild` with `if (ImGui::Begin("Diff", nullptr, ImGuiWindowFlags_NoCollapse)) { ... } ImGui::End();` — kept all cue add/delete handling
- [x] 5.3 Prompt panel already uses `Begin`/`End` internally; renamed caller arg `"Prompt##prompt"` → `"Prompt"` so `DockBuilderDockWindow("Prompt", ...)` matches. (No `NoCollapse` flag plumbed — prompt_panel.h is ImGui-agnostic by convention; acceptable since spec doesn't require it.)
- [x] 5.4 Removed the now-unused `ImGui::SameLine()` and `float bottom_h` between the old `left`/`center` children
- [x] 5.5 The three `Begin` calls happen after the seeding block in the same frame (seeding runs right after `DockSpace`, before `Begin("Files"/"Diff")`)

## 6. Keep toolbar and findbar as non-dockable chrome

- [x] 6.1 `toolbar` and `findbar` remain `BeginChild` regions of `##main` — no change to their code
- [ ] 6.2 Verify dragging/docking content panels does not move the toolbar or findbar — **needs manual launch**

## 7. Build, verify, persist

- [x] 7.1 Full rebuild (`cmake --build build --config Release --target diffcue`) — clean, no errors/warnings; `diffcue.exe` produced
- [ ] 7.2 Launch with no `diffcue.ini` (rename existing) — confirm default layout visually matches the pre-docking arrangement (Files left ~300px, Diff center, Prompt docks bottom when opened) — **needs manual launch**
- [ ] 7.3 Dock `Files` as a tab with `Diff`, float `Prompt`, rearrange, then close & reopen the app — confirm `diffcue.ini` restores the custom layout — **needs manual launch**
- [ ] 7.4 Restore the user's original `diffcue.ini` and confirm upgrade path: empty dockstate → seeding fires → familiar layout appears — **needs manual launch**
- [ ] 7.5 Verify diff viewer still scrolls, gutter aligns, and cue add/delete works while docked/floated (ImGuiColorTextEdit inside a docked window) — **needs manual launch**

## 8. Commit

- [x] 8.1 Stage `thirdparty/imgui` (gitlink), `src/platform/window.cpp`, `src/app/app.cpp`
- [x] 8.2 Commit with message describing the submodule switch + docking enablement (not pushed)
