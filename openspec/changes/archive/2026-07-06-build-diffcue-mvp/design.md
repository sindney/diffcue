## Context

`diffcue` is a greenfield C++17 desktop app. The repository currently contains only
vendored third-party code (`thirdparty/imgui`, `thirdparty/ImGuiColorTextEdit`) and the
openspec scaffold — no application source yet. The two vendored libraries are central to
the design:

- `thirdparty/imgui` — immediate-mode GUI core plus backends (`backends/imgui_impl_glfw.*`,
  `backends/imgui_impl_opengl3.*`).
- `thirdparty/ImGuiColorTextEdit` — provides `TextEditor` (syntax-highlighted read-only /
  editable code editor with per-line markers) and `TextDiff` (side-by-side diff view built
  on `dtl::Diff`). These cover the diff-viewer requirement almost out of the box.

The user runs an AI coding CLI (Claude Code, etc.) in a terminal; after the CLI edits
files, the user launches `diffcue <folder>` to review. The review loop is:

1. See git-modified files in left panel.
2. Pick a file → side-by-side diff opens.
3. Annotate specific lines with short comments ("cues").
4. Click "Copy Prompt" → a prompt of the form `path:line - <comment>` lines is shown in a
   text pane, copied to clipboard.
5. Paste back into the coding CLI.

Platforms: Windows 10/11 (Win64), macOS 12+, Linux (recent glibc). All three must build
from one CMake tree. ImGui + GLFW are the only viable cross-platform windowing stack here;
the vendored copy already targets `imgui_impl_glfw` + `imgui_impl_opengl3`, so we use
OpenGL3 + GLFW as the rendering/windowing backend. **Everything is statically linked** —
GLFW is vendored under `thirdparty/glfw/` and built as a static lib, so no system glfw3
package is required. `git` is invoked as a subprocess at runtime (no libgit2).

## Goals / Non-Goals

**Goals:**

- Single executable per platform, no external runtime dependencies beyond the OS and `git`.
- **Statically link every project and third-party library** (ImGui, ImGuiColorTextEdit,
  GLFW3, tinyfiledialogs) into the single `diffcue` binary. No third-party DLLs/.so/.dylib
  beside the executable.
- Build from CMake 3.20+ with GCC 11+, Clang 14+, or MSVC 2022 (v143).
- Open any git working folder and show all changed files (tracked + untracked) with status
  badges.
- Side-by-side and inline diff with syntax highlighting, per-line line-ending + encoding
  metadata, and read-only text find (Ctrl+F).
- Per-line cues and a "Copy Prompt" button that produces a CLI-ready prompt.
- Theme system: menubar-driven app-wide ImGui style (parsed from `theme.txt`) and
  `TextEditor` syntax palette (Dark/Light/Mariana) switching.
- Sub-100ms cold start on a typical 1000-file repo (no full-tree indexing beyond what git
  status already does).
- Cross-platform clipboard (GLFW `glfwSetClipboardString` / `glfwGetClipboardString`).
- Cross-platform folder picker when no `<folder>` arg is given.

**Non-Goals (MVP):**

- No inline editing/saving of the working files. Diffcue is read-only as a reviewer.
- No merge / conflict resolution.
- No git push / fetch / branch switching. We only read working-tree state.
- No multi-repo / submodule deep traversal.
- No network features (no telemetry, no LSP, no auto-update).
- No plugin/extension system.
- No integration with any specific coding CLI (we just produce clipboard text).

## Decisions

### D1. Windowing & rendering: GLFW3 + OpenGL3 + ImGui

- **Choice**: Use `imgui_impl_glfw` + `imgui_impl_opengl3` backends (already vendored).
- **Why**: ImGui ships first-class GLFW+OpenGL3 backends, and they are the most portable
  combination (Win32, Cocoa, X11/Wayland). No Vulkan/D3D11 complexity needed for a
  text-oriented tool.
- **Alternatives considered**:
  - SDL2 + OpenGL3 — equally portable, but SDL2 is a heavier dependency for no gain here,
    and ImGuiColorTextEdit examples use GLFW.
  - Win32-native + DirectX11 — would abandon macOS/Linux, violating the cross-platform goal.
  - Qt — heavyweight, introduces moc/uic build steps and a competing widget model; ImGui's
  immediate mode is better suited to a small custom reviewer.

### D2. Git access: `git` CLI only (libgit2 not used)

- **Choice**: Use the `git` executable on PATH as the sole git backend. No libgit2 link,
  no build-time `find_package(git2)`, no `DIFFCUE_NO_LIBGIT2` fallback path.
- **Why**: The user requirement is that `git` is a hard runtime prerequisite (every
  developer reviewing code already has it installed). Dropping libgit2 removes a build
  dependency, removes a fallback code path to maintain, and keeps the binary smaller. We
  only need `git status --porcelain=v1 -z` (for the file tree) and `git show HEAD:<path>`
  vs working-tree read (for the diff) — both are stable porcelain that the CLI handles
  well across git versions.
- **Runtime check**: on startup, probe `git --version` on PATH; if missing, show a modal
  error dialog "git not found on PATH. diffcue requires git installed." and exit(2).
- **Subprocess shape**: spawn `git -C <root> status --porcelain=v1 -z` once at folder open
  and on every focus-regain refresh; spawn `git show HEAD:<relpath>` per file open (cached
  per session). Use platform popen/Boost.Process-free implementation (a thin
  `platform::run_capture` helper on top of `_popen` / `popen`).
- **Alternatives considered**:
  - libgit2 — would give direct blob access and slightly better perf on huge repos, but
    adds a non-trivial build-time dep and a second code path. User explicitly rejected.
  - libgit2 static-linked always — adds ~2MB to the binary and a static-link story on
    Windows. Not worth it when `git` is already guaranteed present.

### D3. Diff engine: `ImGuiColorTextEdit::TextDiff`

- **Choice**: Use the vendored `TextDiff` class (built on `dtl`) for the actual diff
  computation and side-by-side rendering.
- **Why**: Already present, integrates with `TextEditor`'s rendering, supports line-level
  markers. Avoids writing a Myers-diff implementation.
- **Limitation**: `TextDiff` operates on line arrays; encoding/line-ending display must be
  layered on top by our own model (see D4).

### D4. Encoding & line-ending detection: custom scanner in `model/file_meta`

- **Choice**: A small pure function `detect_meta(bytes_view) -> {encoding, eol}` that
  classifies UTF-8/UTF-8-BOM/UTF-16LE/UTF-16BE/latin1/binary and LF/CRLF/CR/mixed.
- **Why**: "Beyond Compare shows line ending + encoding" is an explicit requirement and
  these are real sources of hidden review failures (e.g., a CLI silently rewrites CRLF→LF).
  No vendored lib does this; it's ~80 lines of code.
- **Rule**: BOM check first, then `NULL` byte → binary, then UTF-8 validity, then assume
  latin1. EOL: count `\r\n`, lone `\r`, lone `\n` — pick the majority, else "mixed".

### D5. Cues model: in-memory `CueStore` + sidecar JSON file

- **Choice**: `CueStore` keeps cues in memory; on every mutation it writes
  `<folder>/.diffcue/cues.json` atomically.
- **Why**: A user may close/reopen diffcue mid-review; cues must survive. A sidecar file
  under `.diffcue/` (gitignored) is the lowest-friction persistence. No database needed.
- **Schema** (single object):
  ```json
  {
    "version": 1,
    "folder": "<abs path>",
    "cues": [
      { "file": "rel/path.cpp", "side": "new", "line": 42,
        "text": "this is wrong, rename to foo", "created": 1783000000 }
    ]
  }
  ```
- **Why JSON**: readable, debuggable, and the follow-up prompt is a trivial projection.

### D6. Prompt generation: deterministic ordering + stable format

- **Choice**: "Copy Prompt" builds:
  ```
  # diffcue review cues

  - path/to/file.cpp:42 - this is wrong, rename to foo
  - path/to/other.h:7 - missing include guard
  ```
  shown in a read-only `TextEditor` pane, then `glfwSetClipboardString` puts the same
  text on the clipboard.
- **Why**: The user pastes into a coding CLI; predictable `path:line - comment` lines are
  trivially parseable and human-readable. Grouping by file with a blank-line separator.
- **Editable in the pane** before copy — user can trim or reword. Clipboard tracks the
  pane's current contents, so any edits are included.

### D7. Build system: CMake 3.20+ with everything statically linked

- **Choice**: A single top-level `CMakeLists.txt` with these targets:
  - `imgui_static` — builds `thirdparty/imgui/*.cpp` + needed backends as a static lib.
  - `imgui_cte_static` — builds `thirdparty/ImGuiColorTextEdit/*.cpp`, depends on imgui.
  - `glfw_static` — builds the vendored `thirdparty/glfw/` sources as a static lib.
  - `tinyfiledialogs_static` — builds `thirdparty/tinyfiledialogs/tinyfiledialogs.c`.
  - `diffcue` executable — `src/**.cpp`, statically links the four libs above.
- **Static-linking policy (mandatory)**:
  - `CMAKE_FIND_LIBRARY_SUFFIXES` prefers `.a` / `.lib` (static) over `.so` / `.dll`.
  - GLFW is built from the vendored copy with `BUILD_SHARED_LIBS=OFF` and
    `GLFW_BUILD_DOCS=OFF`, never from a system `.so`/`.dll`.
  - imgui / ImGuiColorTextEdit / tinyfiledialogs are always vendored-and-static.
  - The resulting `diffcue` executable MUST NOT have any DT_NEEDED entries on Linux
    (verify with `ldd diffcue` showing only `libGL` / `libstdc++` / `libc` / `libm`), and
    no third-party DLLs beside it on Windows.
  - Only OS-provided system libraries (OpenGL driver, libc/libstdc++, Win32/Cocoa/X11)
    remain dynamically linked — these are unavoidable and acceptable.
- ** Cross-platform specifics**:
  - Windows: MSVC v143; link `opengl32.lib`, `gdi32.lib`, `user32.lib`, `shell32.lib`;
    static CRT (`/MT`) instead of `/MD` so the binary is self-contained.
  - macOS: Clang (Xcode 14+); link `-framework Cocoa -framework OpenGL -framework IOKit`;
    `glfw3` is built from the vendored sources so no Homebrew glfw3 needed. Hide the
    metal-layer workaround via `glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GLFW_TRUE)`.
  - Linux: GCC 11+; link `libGL` (system driver) + X11/Wayland libs from the system.
    GLFW's vendored build handles X11 vs Wayland via CMake options.
- **Build modes**: `RelWithDebInfo` default; `DiffCUE_SANITIZE` option for ASan+UBSan.

### D8. CLI: hand-rolled parser (no CLI11 dependency)

- **Choice**: Parse `argv` in `src/cli/args.cpp` (~60 lines). Support:
  - `--help` / `-h` → print usage, exit 0.
  - `--version` / `-V` → print version, exit 0.
  - `<folder>` (single positional) → open folder.
  - Unknown flag → stderr + exit 2.
- **Why**: The surface is tiny; a CLI library would be disproportionate.

### D9. Folder picker: tinyfiledialogs (vendored, single .c/.h pair)

- **Choice**: When no `<folder>` is provided, use `tinyfiledialogs`' `selectFolderDialog`.
- **Why**: ImGui has no native file dialog; tinyfiledialogs is a tiny (~1kloc) portable
  wrapper over Win32 `IFileDialog`, NSOpenPanel, and zenity/kdialog. Vendoring is one
  .c + one .h.
- **Alternatives**: native platform APIs directly — more code, no benefit for one dialog.

### D10. Source layout

```
src/
  main.cpp
  cli/args.{h,cpp}
  app/app.{h,cpp}            # App class: owns window, models, panels, runs loop
  platform/
    window.{h,cpp}           # GLFW window + ImGui init
    clipboard.{h,cpp}        # glfw clipboard wrapper
    file_dialog.{h,cpp}      # tinyfiledialogs wrapper
    subprocess.{h,cpp}       # run_capture(): popen wrapper for `git` CLI
  git/
    git_adapter.{h,cpp}      # git CLI wrapper: list_changes, read_blob_old, read_blob_new
    git_status.h             # FileStatus enum + structs
  model/
    file_meta.{h,cpp}        # encoding + eol detection
    file_tree.{h,cpp}        # tree of files with status, lazy children
    diff_model.{h,cpp}       # per-file diff data fed to TextDiff
    cue_store.{h,cpp}        # cues CRUD + JSON sidecar
    prompt_builder.{h,cpp}   # cues -> prompt text
  ui/
    menubar.{h,cpp}          # File/View/Help menubar; hosts theme + palette selectors
    theme_loader.{h,cpp}     # parse thirdparty/theme.txt -> list of theme entries
    toolbar_panel.{h,cpp}    # folder, cue counter, prev/next, copy prompt, diff-mode, find
    file_browser_panel.{h,cpp}  # left tree
    diff_viewer_panel.{h,cpp}   # TextDiff host + line-ending/encoding gutter + find bar
    find_bar.{h,cpp}         # Ctrl+F overlay: next/prev match, highlight-all
    prompt_panel.{h,cpp}     # read-only TextEditor showing prompt
resources/
  diffcue.ico                # Windows icon
  Info.plist                 # macOS bundle
```

### D11. Theme system: app-wide ImGui style + `TextEditor` palette

- **Choice**: Two independent theme selectors in the menubar (`View → App Theme` and
  `View → Editor Palette`).
- **App themes**: parse `thirdparty/theme.txt` at startup. Each
  `void SetupImGuiXxxStyle()` block is extracted as a named theme entry
  (`{name: "DarkStyle", fn: &SetupImGuiDarkStyle, ...}`). The menubar lists them by name;
  selecting one calls the function, which applies ImGui style + colors. The default theme
  is the first one in `theme.txt`.
- **TextEditor palettes**: ImGuiColorTextEdit ships `TextEditor::GetDarkPalette()`,
  `GetLightPalette()`, and a Mariana-style palette we vendor as a constant. The menubar
  lists "Dark", "Light", "Mariana"; selecting one calls `editor.SetPalette(palette)`.
- **Why split**: ImGui style (window chrome, buttons, scrollbars) and TextEditor palette
  (syntax colors) are independent dimensions — a user may want a dark chrome with a
  light code pane. Two menus beats one combined menu here.
- **Parser strategy**: lightweight regex-based extractor that splits on
  `void SetupImGui(\w+)\(\)` boundaries and captures the function body verbatim. We
  compile the theme functions into `theme_loader.cpp` by `#include`-ing a generated
  header from `theme.txt` (a CMake `configure_file` / `file(READ)` step strips the
  markdown fences and produces `theme_defs.inc`). This avoids re-implementing each theme
  by hand and stays in sync with `theme.txt` updates.
- **Persistence**: the selected app theme + editor palette are written to
  `<folder>/.diffcue/prefs.json` so they survive reopen. Default to "DarkStyle" +
  "Dark" when no prefs file.
- **Alternatives considered**:
  - Hard-code each theme — duplicates `theme.txt` and drifts.
  - Embed Lua/js — disproportionate for ~15 themes.
- **Limitation**: `theme.txt` themes that reference external fonts/textures (none seen
  in the current file) would not work; we filter those out at parse time.

### D12. Text find (Ctrl+F)

- **Choice**: A `FindBar` overlay at the top of the `DiffViewerPanel`, toggled by Ctrl+F
  (or the toolbar Find button) and dismissed by Esc.
- **Behavior**:
  - Input field + Next (F3) / Prev (Shift+F3) buttons + "Highlight all" checkbox.
  - Search is case-insensitive by default; a `Aa` toggle enables case-sensitive.
  - Searches the *currently focused side* of the diff (old or new pane). When the user
    clicks into the other pane, the search re-runs against that side.
  - Matches are highlighted in-place by `TextEditor::SetCursorPosition` + custom
    `SetBreakpoints`/overlay rendering; "Highlight all" adds a marker on every matching
    line.
  - Wraps around at end of file.
- **Why no replace**: the diff viewer is read-only; replace is out of scope. The find bar
  is purely for navigating the diff during review.
- **Implementation**: ImGuiColorTextEdit doesn't ship a find API. We implement a small
  `FindBar` model: on text change, scan the pane's lines (`TextEditor::GetTextLines()`),
  collect match positions, render highlights via the editor's per-line marker API, and
  use `SetCursorPosition(Coords{line, col})` to jump on Next/Prev.

### D13. Diff mode switch: side-by-side ↔ inline

- **Choice**: A toolbar dropdown (or `View → Diff Mode` menubar entry) with two options:
  `Side by Side` and `Inline`. Selecting either calls
  `TextDiff::SetSideBySideMode(bool)` and re-renders.
- **Default**: `Side by Side` (matches "Beyond Compare"-style default).
- **Persistence**: the selected mode is written to `.diffcue/prefs.json` alongside the
  theme selections.
- **Why free**: `TextDiff` already supports both modes natively (verified in
  `TextDiff.h:28-29`), so this is a one-line setter call plus a UI control — no custom
  diff rendering needed.

## Risks / Trade-offs

- **R1. `git` CLI absent at runtime** → We probe PATH at startup and show a modal error
  dialog "git not found on PATH. diffcue requires git installed." then exit(2). Documented
  in README. Acceptable given the user explicitly assumes git is present.
- **R2. ImGuiColorTextEdit `TextDiff` fidelity** → `TextDiff` is line-oriented and may
  not handle intra-line highlights (e.g., word-level diff). Mitigation: ship as-is for
  MVP; if reviewers ask for word-level diff, layer `dtl`'s `diff` over changed lines in
  a later change. Not blocking.
- **R3. OpenGL3 deprecation on macOS** → Apple deprecated OpenGL in favor of Metal.
  Mitigation: GLFW's OpenGL3 backend still works on macOS 12+; we accept the deprecation
  warning and do not migrate to Metal for MVP.
- **R4. Encoding detection false positives** → latin1-vs-UTF-8 with invalid bytes is
  heuristic. Mitigation: display the detected label clearly; provide a per-file
  "re-detect as…" dropdown in the diff viewer toolbar in a later iteration (out of scope
  for MVP). For MVP, the label is informational; the user can still read the diff.
- **R5. Cues JSON schema drift across versions** → The `version` field guards this.
  Mitigation: on load, if `version > 1`, refuse to load and show a notice; cues are
  recreated by the user. Acceptable for a single-user tool.
- **R6. Wayland clipboard quirks** → GLFW's `glfwSetClipboardString` may not propagate
  to all Wayland compositors reliably. Mitigation: document; provide a "Show Prompt"
  pane with Ctrl+A / Ctrl+C fallback. The pane already exists (D6).
- **R7. Large diffs (10k+ line files)** → `TextDiff` performance may degrade. Mitigation:
  cap the diff size in `DiffViewerPanel`; if a file exceeds 5000 changed lines, show a
  banner "Diff truncated, open in external tool" instead of blocking the UI.
- **R8. `git status` on huge repos is slow** → Repos with millions of files can take
  seconds to scan. Mitigation: run `git status` on a background thread (std::async) and
  show a spinner in the file browser; do not block the UI thread.
- **R9. theme.txt parser fragility** → Regex extraction of `void SetupImGui*Style()`
  functions from a markdown file with C++ code fences is heuristic. Mitigation: parser
  is strict (rejects malformed blocks rather than guessing); a CI check verifies that
  the number of parsed themes matches the number of `## ` headers in `theme.txt`.
- **R10. Static-link runtime conflicts** → Statically linking GLFW + ImGui + our code
  with `/MT` on Windows avoids CRT DLL mismatches; on Linux, static libstdc++ is opt-in
  via `-static-libstdc++ -static-libgcc`. Mitigation: documented in CMake; the only
  remaining dynamic deps are the OpenGL driver and the X11/Wayland libs.
- **T1. Trade-off: vendored ImGui vs system ImGui** → Vendoring locks the version but
  guarantees the `TextEditor`/`TextDiff` API matches. Worth it.
- **T2. Trade-off: in-process cues vs separate review server** → In-process is simpler
  and matches the "single tool" feel; loses multi-machine sync, which the user has not
  asked for.
- **T3. Trade-off: `git` CLI vs libgit2** → CLI-only is simpler (one code path, no
  build-time dep, smaller binary) at the cost of subprocess overhead per file open.
  Mitigation: cache `git show HEAD:<path>` results per session. The user explicitly
  preferred CLI-only.
