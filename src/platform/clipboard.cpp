// platform/clipboard.cpp — GLFW clipboard wrapper (task 3.3).
#include "platform/clipboard.h"

#include <GLFW/glfw3.h>

namespace diffcue::platform::clipboard {

void copy_to_clipboard(std::string_view text) {
    // glfwSetClipboardString requires a null-terminated C string. Build a
    // temporary std::string to guarantee null termination even when the
    // input view doesn't carry one.
    const std::string tmp(text);
    glfwSetClipboardString(nullptr, tmp.c_str());
}

std::string get_clipboard_string() {
    const char* s = glfwGetClipboardString(nullptr);
    return s ? std::string(s) : std::string();
}

}  // namespace diffcue::platform::clipboard
