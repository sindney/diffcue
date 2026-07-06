// platform/file_dialog.cpp — native folder picker via nfd.
#include "platform/file_dialog.h"

#include <cstdio>

#include "nfd.h"

namespace diffcue::platform::file_dialog {

bool init() {
    return NFD_Init() == NFD_OKAY;
}

void quit() {
    NFD_Quit();
}

std::optional<std::filesystem::path> pick_folder(const std::string& default_path) {
    nfdu8char_t* out_path = nullptr;
    const nfdu8char_t* dp = default_path.empty() ? nullptr : default_path.c_str();
    nfdresult_t result = NFD_PickFolderU8(&out_path, dp);
    if (result == NFD_OKAY && out_path) {
        std::filesystem::path p(out_path);
        NFD_FreePathU8(out_path);
        return p;
    }
    if (result == NFD_ERROR) {
        std::fprintf(stderr, "diffcue: NFD_PickFolderU8 error: %s\n",
                     NFD_GetError());
    }
    return std::nullopt;
}

}  // namespace diffcue::platform::file_dialog
