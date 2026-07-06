// platform/subprocess.cpp — _popen / popen wrapper (task 3.5).
#include "platform/subprocess.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sstream>

#if defined(_WIN32)
#  include <windows.h>
#  define DIFFCUE_POPEN  _popen
#  define DIFFCUE_PCLOSE _pclose
#else
#  include <unistd.h>
#  define DIFFCUE_POPEN  popen
#  define DIFFCUE_PCLOSE pclose
#endif

namespace diffcue::platform::subprocess {

namespace {

// Quote one argument for the shell. On Windows _popen runs through cmd.exe,
// so we wrap args containing spaces in double quotes and escape embedded
// quotes. On POSIX popen runs through /bin/sh, where single quotes are the
// safe wrapper.
std::string shell_quote(const std::string& arg) {
#if defined(_WIN32)
    if (arg.empty() || arg.find_first_of(" \t\"") == std::string::npos) {
        return arg;
    }
    std::string out = "\"";
    for (char c : arg) {
        if (c == '"') out += "\\\"";
        else out += c;
    }
    out += "\"";
    return out;
#else
    if (arg.empty()) {
        return "''";
    }
    if (arg.find_first_of(" \t'\"\\$`") == std::string::npos) {
        return arg;
    }
    std::string out = "'";
    for (char c : arg) {
        if (c == '\'') out += "'\\''";
        else out += c;
    }
    out += "'";
    return out;
#endif
}

std::string build_command(const std::string& cmd, const std::vector<std::string>& args) {
    std::string full = shell_quote(cmd);
    for (const auto& a : args) {
        full += " ";
        full += shell_quote(a);
    }
    return full;
}

}  // namespace

std::string run_capture(const std::string& cmd, const std::vector<std::string>& args) {
    const std::string full = build_command(cmd, args);
    FILE* pipe = DIFFCUE_POPEN(full.c_str(), "r");
    if (!pipe) {
        return {};
    }
    std::string output;
    std::array<char, 4096> buffer{};
    while (true) {
        size_t n = std::fread(buffer.data(), 1, buffer.size(), pipe);
        if (n == 0) break;
        output.append(buffer.data(), n);
    }
    int rc = DIFFCUE_PCLOSE(pipe);
    if (rc != 0) {
        return {};
    }
    return output;
}

bool run_succeeds(const std::string& cmd, const std::vector<std::string>& args) {
    const std::string full = build_command(cmd, args);
    FILE* pipe = DIFFCUE_POPEN(full.c_str(), "r");
    if (!pipe) {
        return false;
    }
    // Drain so the child doesn't block on a full pipe.
    std::array<char, 4096> buffer{};
    while (std::fread(buffer.data(), 1, buffer.size(), pipe) > 0) {
        // discard
    }
    return DIFFCUE_PCLOSE(pipe) == 0;
}

}  // namespace diffcue::platform::subprocess
