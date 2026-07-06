## ADDED Requirements

### Requirement: CMake-based build

The project SHALL be buildable with CMake 3.20 or newer using the standard
`cmake -S . -B build && cmake --build build` workflow. The top-level `CMakeLists.txt`
SHALL define targets `imgui_static`, `imgui_cte_static`, `glfw_static`,
`tinyfiledialogs_static`, and the `diffcue` executable.

#### Scenario: Fresh build on Windows

- **WHEN** the developer runs `cmake -S . -B build -G "Visual Studio 17 2022" -A x64`
  then `cmake --build build --config Release`
- **THEN** the build produces `build/Release/diffcue.exe` with no third-party DLLs beside
  it in the output directory

#### Scenario: Fresh build on Linux

- **WHEN** the developer runs `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` then
  `cmake --build build`
- **THEN** the build produces `build/diffcue` and `ldd build/diffcue` shows no entries
  for imgui, imgui_cte, glfw, or tinyfiledialogs (only libc/libstdc++/libGL/X11)

#### Scenario: Fresh build on macOS

- **WHEN** the developer runs `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release` then
  `cmake --build build`
- **THEN** the build produces `build/diffcue` (or `build/diffcue.app` when bundled) with
  no third-party `.dylib` dependencies

### Requirement: C++17 standard

All targets SHALL be compiled with C++17 as the minimum language standard. The
CMakeLists.txt SHALL set `CXX_STANDARD 17`, `CXX_STANDARD_REQUIRED ON`, and
`CXX_EXTENSIONS OFF`.

#### Scenario: Compiler lacks C++17

- **WHEN** the developer configures with a compiler that does not support C++17
- **THEN** CMake configuration fails with a clear error about C++17 support

### Requirement: Supported compilers

The build SHALL succeed with: MSVC v143 (Visual Studio 2022), GCC 11 or newer, and
Clang 14 or newer. CI SHALL verify all three on their respective platforms.

#### Scenario: MSVC v143

- **WHEN** the developer builds with Visual Studio 2022 on Windows
- **THEN** the build succeeds without warnings-as-errors failures

#### Scenario: GCC 11

- **WHEN** the developer builds with `g++-11` on Linux
- **THEN** the build succeeds without warnings-as-errors failures

#### Scenario: Clang 14

- **WHEN** the developer builds with `clang++-14` on macOS or Linux
- **THEN** the build succeeds without warnings-as-errors failures

### Requirement: Vendored ImGui and ImGuiColorTextEdit

The targets `imgui_static` and `imgui_cte_static` SHALL be built from the vendored
sources in `thirdparty/imgui/` and `thirdparty/ImGuiColorTextEdit/` respectively. The
build SHALL NOT use a system-installed ImGui even when one is present, to guarantee API
compatibility with ImGuiColorTextEdit.

#### Scenario: System ImGui present but ignored

- **WHEN** the developer has `libimgui` installed system-wide and configures the build
- **THEN** the build still compiles the vendored `thirdparty/imgui/*.cpp` files and links
  against `imgui_static`

### Requirement: Vendored GLFW3 built as static library

GLFW3 SHALL be vendored under `thirdparty/glfw/` and built as a static library
(`glfw_static` target) with `BUILD_SHARED_LIBS=OFF`, `GLFW_BUILD_DOCS=OFF`,
`GLFW_BUILD_TESTS=OFF`, and `GLFW_BUILD_EXAMPLES=OFF`. The `diffcue` executable SHALL link
`glfw_static` statically. The build SHALL NOT use a system-installed glfw3 even when one
is present, to guarantee a self-contained binary.

#### Scenario: No system glfw3 needed

- **WHEN** the developer configures the build on a machine without glfw3 dev packages
  installed
- **THEN** the build still succeeds because GLFW is built from the vendored sources

#### Scenario: System glfw3 ignored

- **WHEN** the developer has `libglfw3` installed system-wide and configures the build
- **THEN** the build still compiles the vendored `thirdparty/glfw/` and links `glfw_static`

### Requirement: Static linking of all project and third-party code

The `diffcue` executable SHALL statically link `imgui_static`, `imgui_cte_static`,
`glfw_static`, and `tinyfiledialogs_static`. No project-internal or third-party code SHALL
be dynamically linked. The only dynamic links permitted are OS-provided system libraries
(the OpenGL driver, libc/libstdc++/libm on Linux, Win32 system DLLs on Windows, and
Cocoa/IOKit/OpenGL frameworks on macOS).

#### Scenario: Linux ldd output

- **WHEN** the developer runs `ldd build/diffcue` on Linux
- **THEN** the output lists only `libc.so`, `libstdc++.so`, `libm.so`, `libGL.so`, and
  X11/Wayland libraries — never `libimgui`, `libglfw`, or `libtinyfiledialogs`

#### Scenario: Windows output directory

- **WHEN** the developer builds on Windows and lists the `Release/` directory
- **THEN** the directory contains only `diffcue.exe` (and the `.pdb` if symbols are
  enabled) — no `imgui.dll`, `glfw3.dll`, or `tinyfiledialogs.dll`

#### Scenario: MSVC static CRT

- **WHEN** the developer builds on Windows with MSVC in Release configuration
- **THEN** the build uses `/MT` (static CRT) rather than `/MD` so the executable does not
  depend on the Visual C++ Redistributable runtime DLLs

### Requirement: `git` runtime prerequisite (no libgit2)

The build SHALL NOT link libgit2 at build time. At runtime, `diffcue` SHALL invoke the
`git` executable on PATH for all git operations. On startup, `diffcue` SHALL probe
`git --version`; if `git` is not on PATH, the system SHALL display a modal error dialog
"git not found on PATH. diffcue requires git installed." and exit with code 2.

#### Scenario: Build without libgit2

- **WHEN** the developer configures the build on a machine without libgit2 dev packages
- **THEN** the build succeeds with no `find_package(git2)` lookup and no
  `DIFFCUE_NO_LIBGIT2` define needed

#### Scenario: Runtime without git

- **WHEN** the user launches `diffcue` on a machine without `git` on PATH
- **THEN** the application shows a modal error dialog and exits with code 2 without
  opening a window

### Requirement: Single executable output

The build SHALL produce a single `diffcue` executable per platform. No additional shared
libraries from the project itself SHALL be required at runtime (project-internal code
is statically linked). System shared libraries (the OpenGL driver, libc/libstdc++/libm,
Win32/Cocoa/X11 frameworks) MAY be dynamically linked as they are part of the OS.

#### Scenario: Run on a clean machine

- **WHEN** the developer copies the `diffcue` executable to a clean machine with `git`
  installed but no imgui/glfw/tinyfiledialogs libraries
- **THEN** the executable runs successfully

### Requirement: Cross-platform rendering backend

The executable SHALL render via the GLFW3 + OpenGL3 backend (`imgui_impl_glfw` +
`imgui_impl_opengl3`) on all three platforms. No platform-specific rendering backend
(DirectX, Metal, Vulkan) SHALL be used.

#### Scenario: Same backend across platforms

- **WHEN** the developer builds and runs `diffcue` on Windows, macOS, and Linux
- **THEN** all three use the GLFW3 + OpenGL3 backend as the rendering surface

### Requirement: Warnings-as-errors on Release

The build SHALL treat compiler warnings as errors in `Release` and `RelWithDebInfo`
configurations (`-Werror` on GCC/Clang, `/WX` on MSVC). The `Debug` configuration SHALL
NOT enable warnings-as-errors to keep the inner loop fast.

#### Scenario: Release rejects warnings

- **WHEN** the developer introduces an unused-variable warning and builds in `Release`
- **THEN** the build fails

#### Scenario: Debug tolerates warnings

- **WHEN** the developer introduces an unused-variable warning and builds in `Debug`
- **THEN** the build succeeds with the warning printed
