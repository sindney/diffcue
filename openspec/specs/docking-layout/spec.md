## Purpose

Dockable panel workspace for the diffcue main window. The file browser, diff viewer, and prompt panel can be docked, tabbed, floated, and rearranged by the user; a default layout reproduces the previous fixed arrangement on first run. Built on ImGui's `docking` branch via `ImGuiConfigFlags_DockingEnable` + a `DockSpace` hosted inside the `##main` window, with one-shot `DockBuilder` seeding and persistence through the existing `diffcue.ini`.

## Requirements

### Requirement: Docking shall be enabled on the ImGui context
The application SHALL enable `ImGuiConfigFlags_DockingEnable` on the ImGui IO during context initialization so that windows can be docked, tabbed, and floated within the viewport.

#### Scenario: Docking flag is set at startup
- **WHEN** the application starts and creates the ImGui context
- **THEN** `ImGui::GetIO().ConfigFlags` has `ImGuiConfigFlags_DockingEnable` set before the first frame is rendered

### Requirement: A dockspace SHALL host the content panels
The main window SHALL expose an ImGui `DockSpace` covering the work area below the toolbar (and find bar, when open) so that the file browser, diff viewer, and prompt panel can be docked into it.

#### Scenario: Dockspace is present every frame
- **WHEN** the main window is rendered
- **THEN** a `DockSpace` with a stable ID is submitted each frame within the `##main` host window, occupying the region not consumed by the toolbar and find bar

### Requirement: Content panels SHALL be individually dockable
The file browser, diff viewer, and prompt panel SHALL each render as a dockable `ImGui::Begin`/`End` window with a stable, human-readable ID (`Files`, `Diff`, `Prompt`) rather than as `BeginChild` regions, so the user can dock, tab, float, and rearrange them independently.

#### Scenario: File browser is a dockable window
- **WHEN** the file browser panel is rendered
- **THEN** it is submitted via `ImGui::Begin("Files", ...)` / `ImGui::End()` and is dockable within the main dockspace

#### Scenario: Diff viewer is a dockable window
- **WHEN** the diff viewer panel is rendered
- **THEN** it is submitted via `ImGui::Begin("Diff", ...)` / `ImGui::End()` and is dockable within the main dockspace

#### Scenario: Prompt panel is a dockable window
- **WHEN** the prompt panel is open and rendered
- **THEN** it is submitted via `ImGui::Begin("Prompt", ...)` / `ImGui::End()` and is dockable within the main dockspace

### Requirement: Toolbar and find bar SHALL NOT be dockable
The toolbar and find bar SHALL remain as `BeginChild` regions of the `##main` host window (application chrome) and SHALL NOT be dockable, so the primary navigation controls stay anchored at the top of the window.

#### Scenario: Toolbar stays fixed at top
- **WHEN** the user rearranges, floats, or closes docked content panels
- **THEN** the toolbar remains docked to the top of the host window as a non-dockable child region and is unaffected by dock layout changes

### Requirement: A default layout SHALL reproduce the previous arrangement on first run
On the first run (no saved layout in `diffcue.ini`), the application SHALL seed the dockspace via the `ImGui::DockBuilder` API so that `Files` is docked to the left at approximately 300px and `Diff` fills the remaining center space — matching the pre-docking fixed layout. `Prompt` SHALL NOT be docked by the seeder; it opens as a centered floating window (its existing `SetNextWindowPos(center, FirstUseEver)` behavior) and the user may dock it manually by dragging.

#### Scenario: First launch shows the familiar layout
- **WHEN** the application starts with no `diffcue.ini` (or no saved dock layout for the main dockspace)
- **THEN** `Files` appears docked on the left at ~300px width and `Diff` fills the center, visually matching the previous fixed-layout arrangement; `Prompt` (when opened) appears as a centered floating window, not docked

#### Scenario: Default layout is seeded only once
- **WHEN** the application starts and a saved dock layout already exists in `diffcue.ini`
- **THEN** the `DockBuilder` default-layout seeding is skipped and the user's persisted layout is restored unchanged

### Requirement: Dock layout SHALL persist across sessions
The dock layout SHALL serialize through ImGui's existing ini mechanism (`diffcue.ini`) so that the user's docking arrangement is restored on the next launch without additional configuration.

#### Scenario: User customizations are restored on restart
- **WHEN** the user docks, tabs, floats, or resizes panels and then restarts the application
- **THEN** the panel arrangement from the previous session is restored from `diffcue.ini`

### Requirement: ImGui SHALL be on the docking branch
The vendored `thirdparty/imgui` submodule SHALL point at a commit on the `docking` branch of `ocornut/imgui` so that the docking API (`DockSpace`, `DockBuilder`, dockable windows) is available.

#### Scenario: Submodule is on the docking branch
- **WHEN** the submodule is checked out
- **THEN** `git -C thirdparty/imgui branch --contains HEAD` includes `docking` (or HEAD is a commit reachable from the `docking` branch tip)
