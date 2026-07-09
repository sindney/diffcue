## Why

The main window lays out its panels with hand-rolled `BeginChild` + `SameLine` calls (`app.cpp:563-598`), so the sidebar and diff viewer are locked in place. Users cannot tear off the file browser, tab the prompt panel, or put the diff viewer on a second monitor — they can only drag the single splitter. Switching to ImGui's docking branch gives users a flexible, persistent workspace while keeping the current default arrangement for everyone who doesn't want to change anything.

## What Changes

- Switch the vendored ImGui submodule (`thirdparty/imgui`) from `master` to the `docking` branch and pin a commit. The docking branch is a superset of master (all master commits merge into it), so this is API-compatible.
- Enable `ImGuiConfigFlags_DockingEnable` on the ImGui IO during context init (`platform/window.cpp`).
- Add a `DockSpace` over the work area of the existing `##main` host window so panels can be docked/tabbed/floated there.
- Convert the file browser (`left`), diff viewer (`center`), and prompt panel into dockable `ImGui::Begin` windows with stable IDs (`Files`, `Diff`, `Prompt`). They render into the dockspace instead of being `BeginChild` regions.
- Keep the toolbar and find bar as non-dockable children of `##main` (they are app chrome, not content panels).
- Bootstrap a default dock layout via the `ImGui::DockBuilder` API on first run (no saved ini) that reproduces the current arrangement: `Files` docked left at ~300px, `Diff` filling the remainder, `Prompt` docked bottom. On subsequent runs ImGui restores the user's saved layout from `diffcue.ini`.
- **BREAKING (build only):** the submodule pointer for `thirdparty/imgui` changes; contributors must run `git submodule update` after pulling.

## Capabilities

### New Capabilities
- `docking-layout`: Dockable panel workspace — the file browser, diff viewer, and prompt panel can be docked, tabbed, floated, and rearranged by the user; a default layout reproduces the previous fixed arrangement on first run.

### Modified Capabilities
<!-- None. The content/behavior of diff-viewer, file-browser-tree, and
     prompt-generation specs is unchanged — only the host container changes
     from a fixed BeginChild to a docked window. -->

## Impact

- **Dependency:** `thirdparty/imgui` submodule switches from `master` to `docking` branch (new pinned commit in `.gitmodules` + submodule pointer). Build is unchanged — `imgui_static` already compiles `imgui*.cpp` + the GLFW/OpenGL3 backends, all of which exist on the docking branch. `imgui_internal.h` (needed for `DockBuilder`) is already vendored.
- **Code:**
  - `src/platform/window.cpp` — add `ImGuiConfigFlags_DockingEnable` to `io.ConfigFlags`.
  - `src/app/app.cpp` — replace the `left`/`center`/prompt `BeginChild` block with a `DockSpace` + three `Begin`/`End` dockable windows; add a one-time `DockBuilder` default-layout routine keyed off the ini "first run" state.
  - `src/ui/file_browser_panel.cpp`, `src/ui/diff_viewer.cpp`, `src/ui/prompt_panel.cpp` — renderers stay the same; their callers just stop wrapping them in `BeginChild`.
- **Persistence:** layout already serializes through ImGui's ini (`diffcue.ini`); docking node state rides in the same file. No new config surface.
- **Platform:** docking is pure ImGui (no GLFW/backend change). The macOS retina / Windows DPI fixes in `window.cpp` are unaffected.
- **Tests:** no unit-testable behavior change (docking is interactive). Manual verification matrix covers default layout, persistence, float/dock round-trip, and the toolbar staying fixed.
