## Context

diffcue's UI is a single fullscreen ImGui host window (`##main` in `app.cpp:477`) that manually stacks regions with `BeginChild` + `SameLine`:

```
##main (fills viewport work area)
├─ toolbar        (BeginChild, AutoResizeY)   — app chrome, 32px
├─ findbar        (BeginChild, AutoResizeY)   — conditional
├─ row:
│  ├─ left        (BeginChild, 300x0, ResizeX) → file browser
│  └─ center      (BeginChild, 0x0)           → diff viewer
└─ prompt         (conditional)               → prompt panel
```

ImGui is vendored as a git submodule at `thirdparty/imgui` on `master` (commit `776bf2ab0`). The build (`thirdparty/CMakeLists.txt`) compiles `imgui*.cpp` + `imgui_impl_glfw.cpp` + `imgui_impl_opengl3.cpp` into `imgui_static`. `imgui_internal.h` (needed for `DockBuilder`) is already vendored. Layout persists through ImGui's ini (`diffcue.ini`, handled in `window.cpp:84-116`).

Constraints:
- The macOS retina / Windows DPI fixes in `window.cpp` (section 5) must remain untouched.
- `ImGuiColorTextEdit` (the diff viewer's code editor) must keep rendering correctly inside a docked window — it uses `ImGui::BeginChild` internally for the editor surface, which works inside a docked parent.
- No new config file / settings surface; layout rides in the existing ini.

## Goals / Non-Goals

**Goals:**
- Switch ImGui to the `docking` branch with minimal build disruption.
- Make the file browser, diff viewer, and prompt panel dockable/tabbed/floatable.
- Default layout reproduces today's fixed arrangement so existing users see no visual change on first run.
- Persist user layout customizations through the existing ini.

**Non-Goals:**
- Multi-viewport / `ImGuiConfigFlags_ViewportsEnable` (tearing windows out to OS-level windows). Separate concern; the docking branch supports it but we keep a single OS window for now.
- Making the toolbar / find bar dockable (they are app chrome).
- Adding a "reset layout" menu item (can be a follow-up; the DockBuilder seeding only fires on first run).
- Changing the content/behavior of the file browser, diff viewer, or prompt panel renderers.

## Decisions

### Decision 1: Switch the submodule to `docking`, pin a recent commit
**Choice:** `git -C thirdparty/imgui checkout docking` then pin to the docking-branch tip at implementation time; update `.gitmodules` is not needed (submodule URL is unchanged), only the gitlink pointer in the parent repo changes.

**Why not a specific tagged commit:** ImGui's docking branch is not tagged; it is a long-lived branch that periodically merges master. Pinning to the tip-at-implementation-time commit is the conventional approach. The branch is a superset of master (master merges into it), so no API we use is removed.

**Alternative considered:** Vendor the docking sources as a direct copy (not a submodule). Rejected per project convention — ImGui is a medium/large dep that stays a submodule (see project memory on dependency management).

### Decision 2: Enable docking via `ImGuiConfigFlags_DockingEnable` in `window.cpp`
**Choice:** Add `io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;` right after `ImGui::CreateContext()` / before backend init in `Window::Window` (section 3 of `window.cpp`). Do NOT enable `ViewportsEnable`.

**Why:** This is the single ImGui-blessed toggle. Placing it next to the existing `io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard` keeps all config-flag setup together.

### Decision 3: Keep `##main` host window; add a `DockSpace` inside it
**Choice:** Keep the existing fullscreen `##main` host window (it gives us the no-decoration, viewport-filling, no-nav-focus shell). Render the toolbar and find bar as today (top children). Below them, submit `ImGui::DockSpace(dockspace_id, ImVec2(0,0))` to fill the remaining area. The three content panels become `ImGui::Begin("Files"/"Diff"/"Prompt", ...)` windows that dock into this space.

**Why not a bare `DockSpaceOverViewport`:** `DockSpaceOverViewport` would cover the whole viewport and fight the toolbar. We want the toolbar/findbar pinned above the dockspace, so we host the dockspace inside `##main` below those children.

**Layout math:** After rendering toolbar (auto-resize height `th`) and findbar (height `fh`, 0 when closed), the dockspace gets `ImVec2(0, 0)` with the host window's available height already reduced by those children — ImGui's `DockSpace` with `ImVec2(0,0)` fills remaining space. The prompt panel is no longer a `BeginChild` below the row; it is a docked `Begin` window (default-docked bottom).

### Decision 4: Default layout via `DockBuilder`, gated on "first run"
**Choice:** On the first frame where the dockspace ID has no saved ini state, call `DockBuilder` to split: create a left node (~300px) for `Files`, a bottom node for `Prompt`, and the central node for `Diff`. Gate on a static `bool layout_built = false;` plus a check that the dockspace's root node is empty (`DockBuilderGetNode(dockspace_id)` is null or has no child windows) so re-seeding does not clobber a user's saved layout.

**Why DockBuilder over manual `DockSpace` flags:** DockBuilder is the only way to programmatically place windows into specific dock nodes. It lives in `imgui_internal.h` (already vendored). The seeding is a one-shot.

**Pseudocode:**
```cpp
ImGuiID ds = ImGui::GetID("MainDockSpace");
ImGui::DockSpace(ds, ImVec2(0,0));
static bool seeded = false;
if (!seeded) {
    seeded = true;
    if (ImGui::DockBuilderGetNode(ds) == nullptr) {
        ImGui::DockBuilderRemoveNode(ds);
        ImGui::DockBuilderAddNode(ds, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(ds, ImGui::GetContentRegionAvail());
        ImGuiID id_left, id_main;
        ImGui::DockBuilderSplitNode(ds, ImGuiDir_Left, 0.20f, &id_left, &id_main); // ~300/total
        ImGuiID id_main_v, id_bottom;
        ImGui::DockBuilderSplitNode(id_main, ImGuiDir_Down, 0.25f, &id_bottom, &id_main_v);
        ImGui::DockBuilderDockWindow("Files", id_left);
        ImGui::DockBuilderDockWindow("Diff", id_main_v);
        ImGui::DockBuilderDockWindow("Prompt", id_bottom);
        ImGui::DockBuilderFinish(ds);
    }
}
```

**Window order matters:** `Begin("Files")` / `Begin("Diff")` / `Begin("Prompt")` must be called after the DockBuilder seeding block in the same frame (DockBuilder patches pending window docks by window name).

### Decision 5: Panel windows use `NoCollapse` + retain auto-resize behavior
**Choice:** The docked content windows open with `ImGuiWindowFlags_NoCollapse` (mirrors the old modal feel) but otherwise default flags (so they can be docked/tabbed/floated). The diff viewer and prompt panel renderers already call `BeginChild` internally for their scroll areas, so no size handling changes are needed — they fill whatever dock node they're in.

### Decision 6: Prompt panel visibility stays a bool toggle
**Choice:** Keep the existing `prompt_open_` bool. When closed, we simply don't `Begin("Prompt")`. When the user toggles it on (Ctrl+P / toolbar), the window reappears docked at its last position (or the seeded bottom node on first open). We do NOT give the prompt window an `open*` close button that sets `prompt_open_=false` — keep the existing control flow. (Optional: pass `&prompt_open_` so the X button also closes it; low-cost, good UX.)

## Risks / Trade-offs

- **[Risk] Docking branch divergence from master** → The docking branch periodically merges master but can lag by hours/days. Mitigation: pin a commit that includes the latest master merge; the ImGui API surface we use is stable on both branches.
- **[Risk] `DockBuilder` is in `imgui_internal.h` (internal API)** → It has been stable for years and is the documented way to seed layouts, but could change. Mitigation: isolate the seeding in one function (`build_default_dock_layout()`) so a future ImGui bump only touches one spot.
- **[Risk] ImGuiColorTextEdit inside a docked window** → The text editor uses `BeginChild` for its surface and reads `GetContentRegionAvail()`. This works inside a docked parent (dock nodes are just windows). Mitigation: verify the diff viewer still scrolls and the gutter aligns after docking; no code change expected.
- **[Risk] Ini format change for existing users** → Existing `diffcue.ini` files have window settings for `##main` children (`left`, `center`, `toolbar`, `findbar`) but no dock layout. On first run after upgrade, the dockspace is empty → seeding fires → familiar layout appears. Old child-window entries are simply ignored. No migration needed.
- **[Risk] User closes a panel and can't reopen it** → Docked windows can be closed via their tab X (if we allow it). Mitigation: do not pass a close button to `Files`/`Diff` (always-on panels); optionally pass `&prompt_open_` for `Prompt` since it already has a toggle. Add a View menu later if needed.
- **[Trade-off] Single OS window (no viewports)** → Users cannot drag a panel onto a second monitor. Acceptable for v1; `ViewportsEnable` is a clean follow-up that the docking branch already supports.

## Migration Plan

1. Switch submodule: `git -C thirdparty/imgui checkout docking && git -C thirdparty/imgui checkout <pinned-commit>`, then commit the new gitlink in the parent repo.
2. Reconfigure/build: `cmake --build build --config Release` (no CMake change needed — source list is identical).
3. Smoke test: launch, confirm default layout matches old arrangement, dock/float/tab each panel, restart and confirm persistence.
4. Rollback: revert the parent-repo commit (gitlink goes back to master commit) and `git submodule update`. No persisted-state cleanup needed (old ini entries are forward-compatible).

## Open Questions

- Pin the docking tip at implementation time, or wait for the next ImGui release tag on the docking branch? (Default: pin tip-at-implementation; revisit if a tagged release lands.)
- Should the prompt panel get a close (X) button wired to `prompt_open_`, or stay toggle-only via Ctrl+P / toolbar? (Design defaults to toggle-only for parity with today; trivial to add later.)
