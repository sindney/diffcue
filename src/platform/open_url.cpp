// platform/open_url.cpp — open a URL in the OS default browser.
#include "platform/open_url.h"

#include <cstdlib>
#include <string>

namespace diffcue::platform {

void open_url(const std::string& url) {
    // Quote the URL so shell metacharacters / spaces don't break the command.
    // Callers pass trusted hardcoded URLs, but quoting keeps it robust.
    std::string cmd;
#if defined(__APPLE__)
    cmd = "open '" + url + "'";
#elif defined(_WIN32)
    // `start "" <url>` — the empty title arg is required when the URL is quoted.
    cmd = "start \"\" \"" + url + "\"";
#else
    cmd = "xdg-open '" + url + "'";
#endif
    // std::system blocks until the launcher exits, but `open` / `xdg-open` /
    // `start` spawn the handler and return immediately, so the UI is not
    // noticeably stalled. Ignore the return value (best-effort).
    std::system(cmd.c_str());
}

}  // namespace diffcue::platform
