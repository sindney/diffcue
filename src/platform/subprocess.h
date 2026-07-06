// platform/subprocess.h — thin popen wrapper for spawning `git` and capturing
// stdout. Task 3.5. Used by the git adapter; no libgit2.
#pragma once

#include <string>
#include <vector>

namespace diffcue::platform::subprocess {

// Spawn `cmd` with `args` and capture stdout. Returns the captured text.
// On error (spawn failure, non-zero exit), returns an empty string and the
// caller is expected to treat the operation as failed. The command is run
// with the current working directory of the diffcue process.
//
// `cmd` is the executable name (looked up on PATH) or an absolute path.
// `args` does NOT include argv[0].
std::string run_capture(const std::string& cmd, const std::vector<std::string>& args);

// Convenience: run `cmd args...` and return true if exit code == 0.
// Used by the startup git probe (task 5.5).
bool run_succeeds(const std::string& cmd, const std::vector<std::string>& args);

}  // namespace diffcue::platform::subprocess
