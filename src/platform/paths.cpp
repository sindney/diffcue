// platform/paths.cpp — platform-specific filesystem locations.
#include "platform/paths.h"

#include <cstdlib>

namespace diffcue::platform {

std::filesystem::path config_dir() {
#if defined(_WIN32)
    if (const char* a = std::getenv("APPDATA")) return std::filesystem::path(a) / "diffcue";
#elif defined(__APPLE__)
    if (const char* h = std::getenv("HOME")) return std::filesystem::path(h) / "Library/Application Support/diffcue";
#else
    if (const char* h = std::getenv("HOME")) return std::filesystem::path(h) / ".config/diffcue";
#endif
    return ".";
}

}  // namespace diffcue::platform
