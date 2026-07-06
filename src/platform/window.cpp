// platform/window.cpp — GLFW3 window + ImGui init/shutdown.
//
// Single ini file (diffcue.ini in the user config dir) stores both ImGui
// layout and the GLFW window size/position. We disable ImGui's automatic
// disk I/O and handle it ourselves via Load/SaveIniSettingsFromMemory.
#include "platform/window.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32) || defined(__CYGWIN__)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#  include <GL/gl.h>
#elif defined(__APPLE__)
#  include <OpenGL/gl.h>
#else
#  include <GL/gl.h>
#endif
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace diffcue::platform {

namespace {

void glfw_error_callback(int code, const char* desc) {
    std::fprintf(stderr, "GLFW error %d: %s\n", code, desc ? desc : "(null)");
}

// User config dir: %APPDATA%\diffcue (Win), ~/Library/Application Support/diffcue (Mac), ~/.config/diffcue (Linux)
std::string get_ini_dir() {
#if defined(_WIN32)
    if (const char* a = std::getenv("APPDATA")) return std::string(a) + "\\diffcue";
#elif defined(__APPLE__)
    if (const char* h = std::getenv("HOME")) return std::string(h) + "/Library/Application Support/diffcue";
#else
    if (const char* h = std::getenv("HOME")) return std::string(h) + "/.config/diffcue";
#endif
    return ".";
}

std::string get_ini_path() { return get_ini_dir() + "/diffcue.ini"; }

}  // namespace

Window::Window(int width, int height, const std::string& title) {
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() != GLFW_TRUE) { std::fprintf(stderr, "diffcue: glfwInit failed\n"); return; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
    // GLFW_SCALE_FRAMEBUFFER (3.4+) replaces the macOS-only
    // GLFW_COCOA_RETINA_FRAMEBUFFER alias. Either makes the framebuffer
    // match the backing display scale (typically 2x on retina), which is
    // what we want so the OpenGL viewport / ImGui draw data are produced
    // at native pixel density.
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
#endif

    // 1. Create the window (default size — resized later from saved ini).
    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) { std::fprintf(stderr, "diffcue: glfwCreateWindow failed\n"); glfwTerminate(); return; }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1);

    // 2. DPI.
    float sx, sy;
    glfwGetWindowContentScale(window_, &sx, &sy);
    dpi_scale_ = (sx > sy ? sx : sy);
    if (dpi_scale_ < 1.0f) dpi_scale_ = 1.0f;

    // 3. ImGui context — disable automatic ini disk I/O; we handle it.
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    // 4. Load ini: pass full file to ImGui (ignores [Diffcue] section),
    //    then parse [Diffcue] for window geometry and resize/reposition.
    {
        std::ifstream f(get_ini_path());
        if (f) {
            std::stringstream ss; ss << f.rdbuf();
            std::string content = ss.str();
            ImGui::LoadIniSettingsFromMemory(content.c_str(), content.size());

            size_t pos = content.find("[Diffcue]");
            if (pos != std::string::npos) {
                int vx = -1, vy = -1, vw = -1, vh = -1;
                std::istringstream sec(content.substr(pos));
                std::string line; std::getline(sec, line);  // [Diffcue]
                while (std::getline(sec, line)) {
                    if (line.empty() || line[0] == '[') break;
                    if (line.rfind("X=",0)==0) vx = std::atoi(line.c_str()+2);
                    else if (line.rfind("Y=",0)==0) vy = std::atoi(line.c_str()+2);
                    else if (line.rfind("W=",0)==0) vw = std::atoi(line.c_str()+2);
                    else if (line.rfind("H=",0)==0) vh = std::atoi(line.c_str()+2);
                }
                if (vw >= 320 && vh >= 240) glfwSetWindowSize(window_, vw, vh);
                if (vx >= 0 && vy >= 0)      glfwSetWindowPos(window_, vx, vy);
            }
        }
    }

    // 5. HiDPI / retina handling — set the framebuffer scale and load a
    // crisp font at the matching rasterizer density. Style values stay
    // in points (1pt == 1 logical pixel).
    io.DisplayFramebufferScale = ImVec2(dpi_scale_, dpi_scale_);

    // 13pt text on screen, atlas rasterized at 13 * dpi_scale_ framebuffer
    // pixels. AddFontDefaultVector gives a scalable atlas; the default
    // AddFontDefault would pick the 13px-only bitmap font and look blocky
    // on retina.
    if (dpi_scale_ > 1.0f) {
        ImFontConfig font_cfg;
        font_cfg.SizePixels = 13.0f;
        font_cfg.RasterizerDensity = dpi_scale_;
        io.Fonts->AddFontDefaultVector(&font_cfg);
    } else {
        io.Fonts->AddFontDefault();
    }

    ImGui::StyleColorsDark();
    // (No ScaleAllSizes / FontScaleMain — the framebuffer scale above
    //  is the single source of truth for DPI.)

    // 6. Backend init.
    if (!ImGui_ImplGlfw_InitForOpenGL(window_, true))
        std::fprintf(stderr, "diffcue: ImGui_ImplGlfw_InitForOpenGL failed\n");
    if (!ImGui_ImplOpenGL3_Init("#version 330"))
        std::fprintf(stderr, "diffcue: ImGui_ImplOpenGL3_Init failed\n");
}

Window::~Window() {
    if (!window_) return;

    // Save: get ImGui settings + window geometry → single ini file.
    size_t len = 0;
    const char* ini_data = ImGui::SaveIniSettingsToMemory(&len);
    int wx, wy, ww, wh;
    glfwGetWindowPos(window_, &wx, &wy);
    glfwGetWindowSize(window_, &ww, &wh);

    std::error_code ec;
    std::filesystem::create_directories(get_ini_dir(), ec);
    std::ofstream f(get_ini_path(), std::ios::binary | std::ios::trunc);
    if (f) {
        f.write(ini_data, len);
        f << "\n[Diffcue]\nX=" << wx << "\nY=" << wy
          << "\nW=" << ww << "\nH=" << wh << "\n";
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
}

bool Window::should_close() const { return window_ ? glfwWindowShouldClose(window_) != 0 : true; }
void Window::request_close() { if (window_) glfwSetWindowShouldClose(window_, GLFW_TRUE); }

void Window::set_title(const std::string& title) {
    if (window_) glfwSetWindowTitle(window_, title.c_str());
}
void Window::poll_events() { glfwPollEvents(); }

void Window::new_frame() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void Window::render() {
    ImGui::Render();
    int fb_w, fb_h;
    glfwGetFramebufferSize(window_, &fb_w, &fb_h);
    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.10f, 0.10f, 0.12f, 1.00f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window_);
}

}  // namespace diffcue::platform
