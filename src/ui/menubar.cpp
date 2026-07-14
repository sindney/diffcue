// ui/menubar.cpp — File/View/Help menubar (tasks 7.3, 7.4, 7.5).
#include "ui/menubar.h"

#include "imgui.h"
#include "ui/theme_loader.h"

namespace diffcue::ui {

namespace {

constexpr const char* kEditorPalettes[] = {"Dark", "Light", "Mariana"};

void render_app_theme_submenu(model::Prefs& prefs) {
    if (ImGui::BeginMenu("App Theme")) {
        const auto& themes = load_themes();
        for (const auto& t : themes) {
            bool selected = (prefs.app_theme == t.name);
            if (ImGui::MenuItem(t.name.c_str(), nullptr, selected)) {
                prefs.app_theme = t.name;
                apply_theme(t.name);
            }
        }
        ImGui::EndMenu();
    }
}

void render_editor_palette_submenu(model::Prefs& prefs) {
    if (ImGui::BeginMenu("Editor Palette")) {
        for (const char* name : kEditorPalettes) {
            bool selected = (prefs.editor_palette == name);
            if (ImGui::MenuItem(name, nullptr, selected)) {
                prefs.editor_palette = name;
                // The App applies the palette to every open TextEditor/TextDiff
                // when it detects the prefs change.
            }
        }
        ImGui::EndMenu();
    }
}

void render_diff_mode_submenu(model::Prefs& prefs) {
    if (ImGui::BeginMenu("Diff Mode")) {
        bool sbs = (prefs.diff_mode == model::DiffMode::SideBySide);
        if (ImGui::MenuItem("Side by Side", nullptr, sbs)) {
            prefs.diff_mode = model::DiffMode::SideBySide;
        }
        if (ImGui::MenuItem("Inline", nullptr, !sbs)) {
            prefs.diff_mode = model::DiffMode::Inline;
        }
        ImGui::EndMenu();
    }
}

}  // namespace

MenubarActions render_menubar(model::Prefs& prefs) {
    MenubarActions actions;
    if (!ImGui::BeginMainMenuBar()) return actions;

    if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Open Folder...", "Ctrl+O")) {
            actions.open_folder_clicked = true;
        }
        if (ImGui::BeginMenu("Open Recent")) {
            if (prefs.recent_folders.empty()) {
                ImGui::MenuItem("No recent folders", nullptr, false, false);
            } else {
                for (int i = 0; i < static_cast<int>(prefs.recent_folders.size()); ++i) {
                    const auto& p = prefs.recent_folders[i];
                    if (ImGui::MenuItem(p.generic_string().c_str())) {
                        actions.open_recent_index = i;
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Clear Recent Folders")) {
                    actions.clear_recent = true;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Command Palette...", "Ctrl+P")) {
            actions.open_palette_clicked = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit", "Ctrl+Q")) {
            actions.quit_clicked = true;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
        render_app_theme_submenu(prefs);
        render_editor_palette_submenu(prefs);
        render_diff_mode_submenu(prefs);
        ImGui::Separator();
        if (ImGui::MenuItem("Show All", nullptr, actions.show_all)) {
            actions.show_all_toggled = true;
            actions.show_all = !actions.show_all;
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("About diffcue")) {
            actions.about_clicked = true;
        }
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
    return actions;
}

}  // namespace diffcue::ui
