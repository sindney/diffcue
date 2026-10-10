#include "catch_amalgamated.hpp"
#include "ui/command_palette.h"

#include "imgui.h"
#include "imgui_internal.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>

namespace {

struct PaletteFixture {
    ImGuiContext* previous = ImGui::GetCurrentContext();
    ImGuiContext* context = ImGui::CreateContext();
    diffcue::model::Prefs prefs;
    diffcue::model::CueStore cues{std::filesystem::temp_directory_path() /
        ("diffcue_palette_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
    diffcue::ui::PaletteActions actions;
    bool open = true;

    PaletteFixture() {
        auto& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = ImVec2(1000.0f, 800.0f);
        io.DeltaTime = 1.0f / 60.0f;
        io.KeyRepeatDelay = 0.1f;
        io.KeyRepeatRate = 0.04f;
        unsigned char* pixels = nullptr;
        int width = 0, height = 0;
        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        frame(3);
    }

    ~PaletteFixture() {
        ImGui::DestroyContext(context);
        ImGui::SetCurrentContext(previous);
    }

    void frame(int count = 1) {
        for (int i = 0; i < count; ++i) {
            ImGui::NewFrame();
            diffcue::ui::render_command_palette(actions, open, prefs, cues);
            ImGui::Render();
        }
    }

    void hold(ImGuiKey key, int frames) {
        ImGui::GetIO().AddKeyEvent(key, true);
        frame(frames);
        ImGui::GetIO().AddKeyEvent(key, false);
        frame();
    }

    void choose() {
        ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);
        frame();
    }
};

}  // namespace

TEST_CASE("command palette: held arrows repeat", "[command_palette]") {
    for (ImGuiKey key : {ImGuiKey_DownArrow, ImGuiKey_UpArrow}) {
        PaletteFixture fixture;
        fixture.hold(key, 20);
        fixture.choose();
        REQUIRE(fixture.actions.run);
        REQUIRE_FALSE(fixture.open);
        REQUIRE(fixture.actions.command != (key == ImGuiKey_DownArrow
            ? diffcue::ui::PaletteCommand::PrevChange : diffcue::ui::PaletteCommand::Quit));
        REQUIRE(fixture.actions.command != diffcue::ui::PaletteCommand::NextChange);
    }
}

TEST_CASE("command palette: selection scrolls beyond clipped rows", "[command_palette]") {
    for (ImGuiKey key : {ImGuiKey_DownArrow, ImGuiKey_UpArrow}) {
        PaletteFixture fixture;
        for (int i = 0; i < 100; ++i)
            fixture.cues.cues().push_back({"file.cpp", diffcue::model::Side::New, i + 1,
                                          "cue " + std::to_string(i)});
        fixture.frame(3);
        fixture.hold(key, 70);
        ImGuiWindow* list = nullptr;
        for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows)
            if (std::strstr(window->Name, "/palette_list_")) list = window;
        REQUIRE(list != nullptr);
        REQUIRE(list->Scroll.y > 0.0f);
        fixture.choose();
        REQUIRE(fixture.actions.run);
        REQUIRE(fixture.actions.command == diffcue::ui::PaletteCommand::JumpToCue);
        REQUIRE(fixture.actions.payload >= 0);
        REQUIRE(fixture.actions.payload < 100);
    }
}
