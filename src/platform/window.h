// platform/window.h — GLFW3 window + ImGui init (tasks 3.1, 3.2).
//
// Owns the GLFWwindow* and the ImGui context lifetime. The window is
// created at construction time and destroyed at destruction; callers drive
// the frame loop via new_frame() / render().
#pragma once

#include <memory>
#include <string>

struct GLFWwindow;

namespace diffcue::platform {

class Window {
public:
    Window(int width = 1280, int height = 720, const std::string& title = "diffcue");
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // True if the window was created successfully and should keep running.
    bool valid() const { return window_ != nullptr; }

    // True when the user has asked to close (X button, Alt+F4, etc.).
    bool should_close() const;

    // Mark the window as requesting close (e.g. from File -> Quit).
    void request_close();

    // Poll OS events. Call once per frame before new_frame().
    void poll_events();

    // Start a new ImGui frame. Call after poll_events() and before render().
    void new_frame();

    // Render ImGui draw data + swap the OpenGL buffers. Call at end of frame.
    void render();

    // The raw GLFW handle, for callers that need to register callbacks.
    GLFWwindow* handle() { return window_; }

    // Update the window title.
    void set_title(const std::string& title);

    // DPI scale derived from io.DisplayFramebufferScale at init time.
    float dpi_scale() const { return dpi_scale_; }

private:
    GLFWwindow* window_ = nullptr;
    float dpi_scale_ = 1.0f;
};

}  // namespace diffcue::platform
