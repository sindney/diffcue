// platform/open_url.h — open a URL in the OS default browser.
#pragma once

#include <string>

namespace diffcue::platform {

// Open `url` in the user's default browser (macOS `open`, Linux `xdg-open`,
// Windows `start`). Best-effort: silently no-ops on failure. Safe to call
// from the UI thread; the launcher command returns immediately.
void open_url(const std::string& url);

}  // namespace diffcue::platform
