## Why

When using AI coding CLIs (Claude Code, Cursor agents, etc.) in a "vibe" loop, the user
accepts or iterates on changes by reading diffs in the terminal — which is slow, hides
context (line endings, encoding, surrounding code), and forces the user to manually
re-type file paths + line numbers when they want to give follow-up feedback. There is no
lightweight, fast, cross-platform GUI reviewer that closes the loop: review diff → annotate
lines → emit a structured follow-up prompt back to the CLI. `diffcue` fills that gap.

## What Changes

- **New application**: `diffcue` — a standalone C++17 ImGui desktop app that opens a working
  folder, shows its git-modified files in a left-panel tree (with per-file/folder git
  status), and renders a Beyond-Compare-style diff viewer.
- **Diff viewer** uses `ImGuiColorTextEdit` (`TextEditor` + `TextDiff`) already vendored in
  `thirdparty/`. Per-line display of **line-ending type** (CRLF/LF/CR/mixed) and **encoding**
  (UTF-8 / UTF-8-BOM / UTF-16LE / UTF-16BE / latin1 / binary), because these are real sources
  of hidden review failures.
- **Two diff modes**: side-by-side (2-window) and inline (unified), switchable from the
  toolbar via `TextDiff::SetSideBySideMode`. Default is side-by-side.
- **Text find** (Ctrl+F): simple find bar over the current diff pane with next/prev match
  and highlight-all. Read-only review context — no replace.
- **Top toolbar** with: working-folder breadcrumb, cue counter (e.g. `Cues: 4`), Prev / Next
  Change buttons that jump between diff hunks across all open files, diff-mode switch,
  find toggle.
- **Review cues**: click any line (old or new side) to attach a short comment ("cue"). Cues
  are listed in the toolbar summary; clicking a cue jumps to its line.
- **Prompt generation**: one button ("Copy Prompt") builds a single text prompt that
  concatenates every cue as `path:line - <comment>`, opens it in a read-only text editor
  pane for review/edit, and copies the final text to the system clipboard.
- **Theme system**: a `View → Theme` menubar entry lists app themes parsed from
  `thirdparty/theme.txt` (each `SetupImGui*Style()` function becomes a selectable entry).
  A separate `View → Editor Palette` entry switches the `TextEditor` color palette
  (Dark / Light / Mariana, via `TextEditor::SetPalette`).
- **CLI interface**: `diffcue [--help] [--version] <folder>`. With no args, opens a folder
  picker; with a folder arg, opens directly. Requires `git` on PATH at runtime.
- **Cross-platform build**: CMake 3.20+ project that builds on Windows (Win64), macOS, and
  Linux. **Statically links everything** — ImGui, ImGuiColorTextEdit, GLFW3, tinyfiledialogs
  are all built from vendored sources into the single `diffcue` executable. No dynamic
  linking of project-internal or third-party libraries; only the OS runtime and OpenGL
  driver remain dynamic.

## Capabilities

### New Capabilities

- `diff-viewer`: Side-by-side and inline (unified) diff rendering with syntax highlighting
  (via `ImGuiColorTextEdit`), line-ending + encoding metadata per line, Prev/Next-change
  hunk navigation across the working set, and read-only text find (Ctrl+F).
- `file-browser-tree`: Left-panel tree view of the opened folder, annotated with git status
  (added/modified/deleted/untracked/renamed) at file and aggregated folder levels; clicking
  a file opens it in the diff viewer.
- `review-cues`: Attach short per-line comments ("cues"); maintain an ordered cue list;
  toolbar cue counter; jump-to-cue; edit/delete cues.
- `prompt-generation`: Compose a structured follow-up prompt from all cues
  (`path:line - comment`), show it in an editable text pane, and copy to clipboard.
- `cli-interface`: Argument parsing for `--help`, `--version`, and positional `<folder>`;
  folder-picker fallback when no folder is given.
- `ui-themes`: Menubar-driven theme system — switch app-wide ImGui style (parsed from
  `thirdparty/theme.txt`) and `TextEditor` syntax palette (Dark/Light/Mariana) at runtime.
- `cross-platform-build`: CMake build that produces a runnable `diffcue` executable on
  Win64, macOS, and Linux, **statically linking** ImGui + ImGuiColorTextEdit + GLFW3 +
  tinyfiledialogs from vendored sources. `git` is a runtime prerequisite (no libgit2).

### Modified Capabilities

<!-- None — this is a greenfield project; openspec/specs/ is empty. -->

## Impact

- **New code under `src/`**: app entry, CLI parsing, platform abstraction (GLFW + window),
  git adapter (CLI-only), file model, diff model, cue store, prompt builder, theme loader,
  find bar, UI panels.
- **Vendored third-party** (`thirdparty/`): `imgui/` (core + backends already present),
  `ImGuiColorTextEdit/` (TextEditor + TextDiff + dtl already present), `theme.txt`
  (ImGui style snippets to be parsed), and `glfw/` (vendored from upstream for static
  linking — to be added in this change). `tinyfiledialogs` (single .c + .h) is also vendored.
- **Runtime dependency**: `git` must be on PATH. No libgit2 link (build-time or runtime).
- **No dynamic linking of project/third-party code** — only OS runtime + OpenGL driver.
- **No existing systems affected** — project root currently contains only `thirdparty/`,
  `LICENSE`, `.gitignore`, and this `openspec/` scaffold; no prior source to break.
- **Out of scope for MVP**: multi-repo, inline edit/save, merge conflict resolution,
  network/git-push, plugin/extension API. These can be later changes.
