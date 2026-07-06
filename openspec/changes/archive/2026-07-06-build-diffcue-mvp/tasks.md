## 1. Project bootstrap

- [x] 1.1 Create top-level `CMakeLists.txt` (CMake 3.20+, C++17, `CXX_EXTENSIONS OFF`) with `project(diffcue VERSION 0.1.0)` and placeholder `add_executable(diffcue src/main.cpp)`
- [x] 1.2 Add `cmake/` helper module `DiffcueDeps.cmake` that wraps the vendored-only deps (no `find_package(glfw3)` / `find_package(git2)` calls — git is a runtime-only prerequisite, not a build dep)
- [x] 1.3 Configure `compile_commands.json` export (`CMAKE_EXPORT_COMPILE_COMMANDS ON`) and `.clangd` / `.vscode` settings.json for dev ergonomics
- [x] 1.4 Extend `.gitignore` for `build/`, `build-*/`, `.diffcue/`, IDE caches, plus a `!thirdparty/` allowlist so vendored deps stay tracked
- [x] 1.5 Confirm `git` is the only runtime prerequisite (no libgit2 in build, no system glfw3 needed) — document in README

## 2. Vendored third-party targets (all static)

- [x] 2.1 Vendor GLFW3 under `thirdparty/glfw/` from the upstream source (subtree or snapshot at a pinned tag, e.g., 3.4)
- [x] 2.2 Vendor `tinyfiledialogs` (`thirdparty/tinyfiledialogs/tinyfiledialogs.c` + `.h`) from upstream
- [x] 2.3 Add `thirdparty/CMakeLists.txt` defining `imgui_static` from `imgui/*.cpp` + `backends/imgui_impl_glfw.cpp` + `backends/imgui_impl_opengl3.cpp`
- [x] 2.4 In the same file, define `imgui_cte_static` from `ImGuiColorTextEdit/TextEditor.cpp` + `ImGuiColorTextEdit/TextDiff.cpp`, depending on `imgui_static`
- [x] 2.5 Define `glfw_static` with `BUILD_SHARED_LIBS=OFF`, `GLFW_BUILD_DOCS=OFF`, `GLFW_BUILD_TESTS=OFF`, `GLFW_BUILD_EXAMPLES=OFF`
- [x] 2.6 Define `tinyfiledialogs_static` from the single `.c` file
- [x] 2.7 Configure `diffcue` to link the four static libs with `-static-libstdc++ -static-libgcc` on Linux and `/MT` (static CRT) on Windows MSVC
- [ ] 2.8 Verify the four static libs build cleanly on Windows (MSVC v143), macOS (Clang 14), and Linux (GCC 11) with `-Werror` on `RelWithDebInfo`
- [ ] 2.9 Smoke-test: an empty `TextEditor` widget renders in a GLFW window on all three platforms; `ldd`/dumpbin shows no third-party dynamic deps

## 3. Platform layer (`src/platform/`)

- [x] 3.1 Implement `platform/window.{h,cpp}` — GLFW3 window (1280x800 default), `glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3)`, OpenGL3 loader via `imgui_impl_opengl3`'s built-in loader
- [x] 3.2 Implement ImGui init/shutdown: `ImGui::CreateContext()`, `ImGui_ImplGlfw_InitForOpenGL`, `ImGui_ImplOpenGL3_Init`, DPI scaling (`io.DisplayFramebufferScale`)
- [x] 3.3 Implement `platform/clipboard.{h,cpp}` — `copy_to_clipboard(std::string_view)` and `get_clipboard_string()` wrapping `glfwSetClipboardString` / `glfwGetClipboardString`
- [x] 3.4 Implement `platform/file_dialog.{h,cpp}` — `pick_folder()` returning `std::optional<std::filesystem::path>` via `tinyfiledialogs_selectFolderDialog`
- [x] 3.5 Implement `platform/subprocess.{h,cpp}` — `run_capture(std::string_view cmd, std::span<std::string> args) -> std::string` wrapping `_popen` (Windows) / `popen` (POSIX); used by the git adapter
- [ ] 3.6 Smoke test: window opens, shows "Hello diffcue", closes cleanly; Ctrl+V after `copy_to_clipboard` works

## 4. CLI interface (`src/cli/`)

- [x] 4.1 Implement `cli/args.{h,cpp}` with `struct ParsedArgs { std::optional<std::filesystem::path> folder; bool help=false; bool version=false; }` and `ParsedArgs parse_args(int argc, char** argv)`
- [x] 4.2 Support `--help`/`-h` (print usage to stdout, exit 0) and `--version`/`-V` (print `diffcue <version>` from CMake-configured `DIFFCUE_VERSION`, exit 0)
- [x] 4.3 Validate the positional `<folder>`: exists + is a directory, else stderr + exit 2; reject >1 positional or unknown flags with exit 2
- [ ] 4.4 Unit-test `parse_args` (catch2) covering: help, version, valid folder, missing folder, non-existent path, file-not-folder, unknown flag, too many positionals
- [x] 4.5 Wire `main.cpp` to call `parse_args` and exit early on help/version/error before initializing the GUI

## 5. Git adapter (CLI-only, `src/git/`)

- [x] 5.1 Define `git/git_status.h` with `enum class FileStatus { Modified, Added, Deleted, Renamed, Untracked, Clean }` and `struct GitEntry { std::filesystem::path relpath; FileStatus status; std::optional<std::filesystem::path> renamed_from; }`
- [x] 5.2 Implement `git/git_adapter.{h,cpp}` with `std::vector<GitEntry> list_changes(const std::filesystem::path& root)` — spawns `git -C <root> status --porcelain=v1 -z` via `platform::subprocess::run_capture`, parses the `-z`-delimited entries into `GitEntry`
- [x] 5.3 Add `std::string read_blob_old(const std::filesystem::path& root, const std::filesystem::path& relpath)` spawning `git -C <root> show HEAD:<relpath>` (cached per session); returns empty string if the file is untracked (no HEAD version)
- [x] 5.4 Add `std::string read_blob_new(const std::filesystem::path& root, const std::filesystem::path& relpath)` reading the working-tree file directly via `std::ifstream`
- [x] 5.5 Runtime probe at startup: `git --version` via `run_capture`; if it fails or git is missing, show a modal ImGui error dialog "git not found on PATH. diffcue requires git installed." then exit(2) after the user dismisses
- [ ] 5.6 Integration-test `list_changes` against a temp git repo fixture (init → commit → modify → add → delete → rename → untracked) on at least one platform in CI

## 6. Models (`src/model/`)

- [x] 6.1 Implement `model/file_meta.{h,cpp}` — `struct FileMeta { Encoding encoding; Eol eol; };` with `detect_meta(std::string_view bytes)`. Detect UTF-8-BOM / UTF-16LE / UTF-16BE / UTF-8 / latin1 / binary; detect LF / CRLF / CR / Mixed / None
- [x] 6.2 Implement `model/file_tree.{h,cpp}` — lazy tree node with `std::vector<Node> children` (dirs first, then files), per-node `FileStatus`, aggregated folder status, file-count badge on root. Backed by `git_adapter::list_changes` + lazy `std::filesystem::directory_iterator`
- [x] 6.3 Implement `model/diff_model.{h,cpp}` — `struct FileDiff { path, old_text, new_text, old_meta, new_meta, hunks; }` where `hunks` is the list of changed line ranges from `ImGuiColorTextEdit::TextDiff`; cap at 5000 changed lines (per spec) with a `truncated` flag
- [x] 6.4 Implement `model/cue_store.{h,cpp}` — `struct Cue { std::filesystem::path file; Side side; int line; std::string text; int64_t created; }`, in-memory `std::vector<Cue>`, atomic JSON sidecar read/write at `<folder>/.diffcue/cues.json` (schema with `version` field), stale-flag detection on load
- [x] 6.5 Implement `model/prompt_builder.{h,cpp}` — `std::string build_prompt(const CueStore&)` producing the `# diffcue review cues` header + `- <path>:<line> - <text>` lines grouped by file and sorted by line ascending
- [x] 6.6 Implement `model/prefs.{h,cpp}` — `struct Prefs { std::string app_theme; std::string editor_palette; DiffMode diff_mode; };` read/written to `<folder>/.diffcue/prefs.json`; default to first theme in theme.txt + "Dark" palette + side-by-side mode
- [ ] 6.7 Unit-test `file_meta`, `prompt_builder` (3-cue fixture matching the spec scenario), `cue_store` (sidecar round-trip + stale detection), `prefs` (round-trip + fallback when missing)

## 7. Theme system (`src/ui/theme_loader.{h,cpp}`, `src/ui/menubar.{h,cpp}`)

- [x] 7.1 Implement `ui/theme_loader.{h,cpp}` — parse `thirdparty/theme.txt`, extract every `void SetupImGui<Name>Style()` function and its body into a `struct ThemeEntry { std::string name; void (*apply)(); };`. Generate `theme_defs.inc` via CMake (`file(READ)` + `string(REGEX REPLACE)` to strip markdown fences) and `#include` it from `theme_loader.cpp` so the extracted functions are compiled in
- [x] 7.2 Provide `std::vector<ThemeEntry> load_themes()` returning the parsed entries; log skipped entries (those referencing undefined symbols) to stderr
- [x] 7.3 Implement `ui/menubar.{h,cpp}` with `File` (Open Folder, Quit), `View` (App Theme submenu, Editor Palette submenu, Diff Mode submenu, Show All toggle), `Help` (About) entries
- [x] 7.4 Wire `View → App Theme`: list `theme_loader::load_themes()` entries; on select, call the entry's `apply()` function and persist to `prefs.json`; mark current with a checkmark
- [x] 7.5 Wire `View → Editor Palette`: three entries (Dark/Light/Mariana) backed by `TextEditor::GetDarkPalette()` / `GetLightPalette()` / a vendored Mariana constant; on select, call `editor.SetPalette()` on every open `TextEditor`/`TextDiff` and persist to `prefs.json`
- [x] 7.6 On startup, apply saved `prefs.app_theme` (or first theme) and `prefs.editor_palette` (or "Dark") before rendering any panel
- [ ] 7.7 Unit-test `theme_loader::load_themes` against the real `theme.txt` — assert the parsed count matches the number of `SetupImGui*Style()` headers in the file

## 8. UI panels (`src/ui/`)

- [x] 8.1 Implement `ui/toolbar_panel.{h,cpp}` — folder breadcrumb, `Cues: N` counter (click → dropdown cue list), Prev/Next-change buttons, Diff Mode dropdown (Side by Side / Inline), Find button (toggles FindBar), "Copy Prompt" button, "Show all" toggle
- [x] 8.2 Implement `ui/file_browser_panel.{h,cpp}` — tree view consuming `model/file_tree`, status badges (color + short code), expand/collapse on click, open-file-on-click, "Show all" filter, file-count badge on root
- [x] 8.3 Implement `ui/diff_viewer_panel.{h,cpp}` — host `ImGuiColorTextEdit::TextDiff`, header bar with `EOL:` and `Encoding: old → new` labels, syntax highlighting by extension, binary placeholder, truncation banner, current-hunk visual marker, click-to-add-cue on a line, cue marker rendering + tooltip
- [x] 8.4 Wire Diff Mode dropdown to `TextDiff::SetSideBySideMode(bool)`; preserve scroll position and cue markers on switch; persist to `prefs.json`
- [x] 8.5 Implement `ui/find_bar.{h,cpp}` — input field + Next (F3) / Prev (Shift+F3) + "Highlight all" checkbox + case-sensitivity `Aa` toggle; searches the focused diff pane; wraps around; dismissed by Esc; no Replace controls (read-only per spec)
- [x] 8.6 Wire Ctrl+F → toggle `FindBar`; F3/Shift+F3 → next/prev; maintain match positions by scanning `TextEditor::GetTextLines()` and render highlights via per-line markers
- [x] 8.7 Implement `ui/prompt_panel.{h,cpp}` — read-only-by-default `TextEditor` showing the generated prompt; "Copy to Clipboard" button; transient toast on successful copy (1.5s)
- [x] 8.8 Wire toolbar Prev/Next: collect all hunks across all modified files (in file-browser order), jump within file then to next file, mark current hunk, no-op when zero changes
- [x] 8.9 Wire toolbar cue counter: click → dropdown lists `file:line - text` entries; click entry → opens that file + scrolls to line

## 9. App wiring (`src/app/`, `src/main.cpp`)

- [x] 9.1 Implement `app/app.{h,cpp}` — `App` class owns `Window`, `FileTree`, `CueStore`, `Prefs`, current `FileDiff`, `ThemeLoader`, the four panels, and `FindBar`; `run()` loop = poll events → new frame → render menubar → render panels → render → swap buffers
- [x] 9.2 Layout: menubar on top, left = `file_browser_panel` (resizable, 300px default), top toolbar = `toolbar_panel` (40px), center = `diff_viewer_panel` (fills remaining), bottom-right docked = `prompt_panel` (shown when "Copy Prompt" is clicked, hidden via close button)
- [x] 9.3 Wire folder-open flow: `parse_args` → if no folder, `platform::pick_folder()` → if folder chosen, `git_adapter::list_changes` → build `FileTree` → reset `CueStore` from sidecar → load `Prefs` → apply saved theme + palette + diff mode → render
- [x] 9.4 Wire cue add flow: `DiffViewerPanel` line click → inline text input → Enter → `CueStore::add` → re-render marker + update toolbar counter
- [x] 9.5 Wire "Copy Prompt" flow: `ToolbarPanel` button → `prompt_builder::build_prompt` → fill `PromptPanel` → `clipboard::copy_to_clipboard` → show toast
- [x] 9.6 Handle external file changes: on `GLFW_FOCUS` regain, re-scan git status (on a background `std::async` thread, per R8) and refresh `FileTree`; mark cues whose target line disappeared as stale
- [ ] 9.7 End-to-end smoke test on Windows: `diffcue <repo>` opens, file tree populates, clicking a modified file shows diff with EOL/Encoding labels, adding a cue persists to `.diffcue/cues.json`, "Copy Prompt" produces correct clipboard text, switching Diff Mode preserves position, Ctrl+F finds text, switching theme via menubar restyles the app

## 10. Testing

- [x] 10.1 Add `tests/CMakeLists.txt` with Catch2 (vendored single-header in `thirdparty/catch2/`) and `enable_testing()`
- [x] 10.2 Unit tests for `cli/args`, `model/file_meta`, `model/prompt_builder`, `model/cue_store` (sidecar round-trip + stale detection), `model/prefs`, `ui/theme_loader` (count matches `theme.txt` headers)
- [x] 10.3 Integration test for `git_adapter::list_changes` using a temp git repo fixture (init → commit → modify → add → delete → rename → untracked)
- [x] 10.4 Integration test for `platform::subprocess::run_capture` — capture `git --version` output and assert it contains "git version"
- [ ] 10.5 Visual regression: capture screenshots of a known fixture repo for Win/macOS/Linux and store under `tests/fixtures/screenshots/` for manual comparison (no automated image diff for MVP)
- [x] 10.6 Add `DIFFCUE_SANITIZE` CMake option (ASan + UBSan on GCC/Clang; `/fsanitize=address` on MSVC) and run all unit tests under it

## 11. CI & packaging

- [x] 11.1 Add `.github/workflows/build.yml` matrix (windows-2022 / macos-12 / ubuntu-22.04) — no system glfw3/libgit2 install needed (all vendored); configure with `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release`, build, and upload the `diffcue` artifact
- [x] 11.2 Add a `Release` build that sets `DIFFCUE_VERSION` from git tag (or `0.1.0-dev` if untagged) via `configure_file` into `src/version.h`
- [x] 11.3 macOS: produce a signed `.app` bundle with `Info.plist` and an icon; document notarization as a follow-up (MVP ships unsigned)
- [x] 11.4 Windows: produce an icon-embedded `diffcue.exe` with `/MT` static CRT; document optional MSIX packaging as a follow-up
- [x] 11.5 Linux: produce a portable `diffcue` binary; document AppImage as a follow-up
- [ ] 11.6 Final validation matrix run on all three platforms: `diffcue --help`, `diffcue --version`, `diffcue <fixture>` end-to-end (open → review 3 cues → copy prompt → verify clipboard content → switch diff mode → Ctrl+F find → switch theme)

## 12. README and documentation

- [x] 12.1 Write `README.md` covering: (a) **Why** — diffcue closes the review loop for AI coding CLIs (review diff → annotate → emit prompt to clipboard); (b) **How to build** — platform prerequisites (only `git` at runtime; CMake 3.20+ and a C++17 compiler at build time), the `cmake -S . -B build && cmake --build build` quick start, and platform-specific notes (MSVC `/MT`, macOS frameworks, Linux ldd output); (c) **Code dependencies** — vendored list with upstream URLs and versions: ImGui (MIT), ImGuiColorTextEdit (MIT, provides `TextEditor` + `TextDiff`), GLFW3 (zlib, vendored at pinned tag 3.4), tinyfiledialogs (zlib), Catch2 (Boost) for tests, and `thirdparty/theme.txt` (collection of ImGui style snippets); (d) **Usage** — `diffcue <folder>`, `--help`, `--version`, no-arg folder picker; (e) **Project layout** — one-paragraph pointer to `src/`, `thirdparty/`, `openspec/`
- [x] 12.2 Add `THIRD_PARTY_NOTICES.md` listing each vendored library's full license text (ImGui, ImGuiColorTextEdit, GLFW, tinyfiledialogs, Catch2)
- [x] 12.3 Add `docs/USAGE.md` with screenshots of the menubar (App Theme submenu, Editor Palette submenu, Diff Mode switch), the find bar, and the prompt pane, plus the expected `.diffcue/` sidecar files (`cues.json`, `prefs.json`) schema
