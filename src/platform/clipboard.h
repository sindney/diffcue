// platform/clipboard.h — GLFW clipboard wrapper (task 3.3).
#pragma once

#include <string>
#include <string_view>

namespace diffcue::platform::clipboard {

// Copy text to the system clipboard via glfwSetClipboardString.
// Requires a valid GLFW window context (the window must have been created).
void copy_to_clipboard(std::string_view text);

// Read text from the system clipboard via glfwGetClipboardString.
// Returns an empty string if the clipboard is empty or unavailable.
std::string get_clipboard_string();

}  // namespace diffcue::platform::clipboard
