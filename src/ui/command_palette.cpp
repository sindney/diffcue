// ui/command_palette.cpp — Cmd+P command palette (Sublime/VSCode-style).
//
// A top-center ImGui window with a filter input and a filtered list of
// commands. Selections return a PaletteActions struct; App::run() merges
// it into the existing ToolbarActions / MenubarActions dispatch (no
// duplicated effect logic). The palette handles Esc (close) and
// Up/Down/Enter (navigation); the bare Cmd+P toggle is owned by App so the
// palette does not handle Cmd+P itself (avoids double-toggle).
#include "ui/command_palette.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

#include "imgui.h"

namespace diffcue::ui {

namespace {

struct Entry {
    std::string display;
    PaletteCommand cmd;
    int payload;
};

// File-scope state (the palette is single-instance).
char filter_buf[256] = "";
int selected_index = 0;
std::string last_filter;  // detect filter changes to reset selection

bool icontains(const std::string& haystack, const std::string& needle) {
    if (needle.empty()) return true;
    auto it = std::search(haystack.begin(), haystack.end(),
                          needle.begin(), needle.end(),
                          [](char a, char b) {
                              return std::tolower(static_cast<unsigned char>(a))
                                   == std::tolower(static_cast<unsigned char>(b));
                          });
    return it != haystack.end();
}

void build_command_list(std::vector<Entry>& out,
                        const model::Prefs& prefs,
                        const model::CueStore& cues) {
    out.clear();

    // Static commands (Navigation / Cues / View / App categories).
    out.push_back({"Next Change",         PaletteCommand::NextChange,        -1});
    out.push_back({"Previous Change",     PaletteCommand::PrevChange,         -1});
    out.push_back({"Open Folder...",      PaletteCommand::OpenFolder,         -1});
    out.push_back({"Refresh",             PaletteCommand::Refresh,            -1});
    out.push_back({"Clear Cues",          PaletteCommand::ClearCues,          -1});
    out.push_back({"Copy Prompt",         PaletteCommand::CopyPrompt,         -1});
    out.push_back({"Toggle Diff Mode",    PaletteCommand::ToggleDiffMode,     -1});
    out.push_back({"Toggle Ignore EOL",   PaletteCommand::ToggleIgnoreEOL,    -1});
    out.push_back({"Toggle Find Bar",     PaletteCommand::ToggleFindBar,      -1});
    out.push_back({"Show All",            PaletteCommand::ShowAll,            -1});
    out.push_back({"About diffcue",       PaletteCommand::About,              -1});
    out.push_back({"Quit",                PaletteCommand::Quit,               -1});

    // Per-recent-folder entries.
    for (int i = 0; i < static_cast<int>(prefs.recent_folders.size()); ++i) {
        Entry e;
        e.display = "Open Recent: " + prefs.recent_folders[i].generic_string();
        e.cmd = PaletteCommand::OpenRecent;
        e.payload = i;
        out.push_back(std::move(e));
    }

    // Per-cue entries.
    const auto& cs = cues.cues();
    for (int i = 0; i < static_cast<int>(cs.size()); ++i) {
        Entry e;
        char buf[512];
        std::snprintf(buf, sizeof(buf), "Jump to Cue: %s:%d - %s",
                      cs[i].file.generic_string().c_str(), cs[i].line,
                      cs[i].text.c_str());
        e.display = buf;
        e.cmd = PaletteCommand::JumpToCue;
        e.payload = i;
        out.push_back(std::move(e));
    }
}

}  // namespace

bool render_command_palette(PaletteActions& out,
                            bool& open_flag,
                            const model::Prefs& prefs,
                            const model::CueStore& cues) {
    out.run = false;
    if (!open_flag) return false;

    // Position at top-center of the viewport (Sublime/VSCode-style).
    ImGuiViewport* vp = ImGui::GetMainViewport();
    ImVec2 size(600.0f, 400.0f);
    ImVec2 pos(vp->WorkPos.x + vp->WorkSize.x * 0.5f,
               vp->WorkPos.y + vp->WorkSize.y * 0.15f);
    ImGui::SetNextWindowSize(size, ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Appearing, ImVec2(0.5f, 0.0f));

    ImGui::SetNextWindowFocus();
    if (!ImGui::Begin("Command Palette", &open_flag,
                      ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return false;
    }

    // Focus the filter input when the palette first appears.
    if (ImGui::IsWindowAppearing()) {
        filter_buf[0] = '\0';
        last_filter.clear();
        selected_index = 0;
        ImGui::SetKeyboardFocusHere();
    }
    ImGui::InputTextWithHint("##palette_filter", "Type to filter commands...",
                             filter_buf, sizeof(filter_buf));

    // Build + filter the command list.
    std::vector<Entry> all;
    build_command_list(all, prefs, cues);

    const std::string filter_str(filter_buf);
    std::vector<const Entry*> filtered;
    filtered.reserve(all.size());
    for (const auto& e : all) {
        if (icontains(e.display, filter_str)) {
            filtered.push_back(&e);
        }
    }

    // Reset selection when the filter changes (keep it usable).
    if (filter_str != last_filter) {
        selected_index = 0;
        last_filter = filter_str;
    }
    if (!filtered.empty() && selected_index >= static_cast<int>(filtered.size())) {
        selected_index = static_cast<int>(filtered.size()) - 1;
    }
    if (selected_index < 0) selected_index = 0;

    // Keyboard navigation (Up/Down with wrap, Enter runs, Esc closes).
    if (!filtered.empty()) {
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, false)) {
            selected_index = (selected_index + 1) % static_cast<int>(filtered.size());
        }
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, false)) {
            selected_index = (selected_index - 1 + static_cast<int>(filtered.size()))
                             % static_cast<int>(filtered.size());
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
            const auto* e = filtered[selected_index];
            out.run = true;
            out.command = e->cmd;
            out.payload = e->payload;
            open_flag = false;
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        open_flag = false;
    }

    // List region with clipper for performance with large cue counts.
    ImGui::Separator();
    ImGui::BeginChild("palette_list", ImVec2(0, 0), ImGuiChildFlags_Borders);
    if (filtered.empty()) {
        ImGui::TextDisabled("No matching commands");
    } else {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(filtered.size()));
        while (clipper.Step()) {
            for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
                bool is_selected = (i == selected_index);
                if (ImGui::Selectable(filtered[i]->display.c_str(), is_selected)) {
                    // Mouse click runs the command immediately.
                    out.run = true;
                    out.command = filtered[i]->cmd;
                    out.payload = filtered[i]->payload;
                    open_flag = false;
                }
                if (is_selected) ImGui::SetItemDefaultFocus();
            }
        }
        clipper.End();
    }
    ImGui::EndChild();

    ImGui::End();
    return out.run;
}

}  // namespace diffcue::ui
