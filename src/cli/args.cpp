// cli/args.cpp — hand-rolled argv parser (design D8, tasks 4.1-4.3).
#include "cli/args.h"

#include <iostream>

#include "diffcue/version.h"

namespace diffcue::cli {

namespace {

bool is_help_flag(const std::string& a) {
    return a == "--help" || a == "-h";
}

bool is_version_flag(const std::string& a) {
    return a == "--version" || a == "-V";
}

bool is_flag(const std::string& a) {
    return !a.empty() && a[0] == '-';
}

}  // namespace

std::string usage_text() {
    return
        "diffcue " DIFFCUE_VERSION " - diff reviewer + cue annotator\n"
        "\n"
        "Usage:\n"
        "  diffcue [options] [<folder>]\n"
        "\n"
        "Options:\n"
        "  -h, --help      Print this message and exit.\n"
        "  -V, --version   Print version and exit.\n"
        "\n"
        "<folder> is a git working tree to review. If omitted, the current\n"
        "working directory is used. diffcue requires `git` on PATH.\n";
}

ParsedArgs parse_args(int argc, char** argv) {
    ParsedArgs result;
    std::vector<std::string> positionals;

    for (int i = 1; i < argc; ++i) {
        std::string arg(argv[i]);
        if (is_help_flag(arg)) {
            result.status = ParseStatus::Help;
            return result;
        }
        if (is_version_flag(arg)) {
            result.status = ParseStatus::Version;
            return result;
        }
        if (is_flag(arg)) {
            result.status = ParseStatus::Error;
            result.error = "unknown flag: " + arg;
            return result;
        }
        positionals.push_back(arg);
    }

    if (positionals.size() > 1) {
        result.status = ParseStatus::Error;
        result.error = "too many positional arguments (expected at most one <folder>)";
        return result;
    }

    if (positionals.empty()) {
        // No folder → use the current working directory (design deviation
        // from D9: no folder-picker lib; current dir is the fallback).
        result.status = ParseStatus::Ok;
        std::error_code ec;
        result.folder = std::filesystem::current_path(ec);
        if (ec) {
            result.status = ParseStatus::Error;
            result.error = "cannot resolve current working directory";
        }
        return result;
    }

    const auto& path = positionals[0];
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        result.status = ParseStatus::Error;
        result.error = "path does not exist: " + path;
        return result;
    }
    if (!std::filesystem::is_directory(path, ec)) {
        result.status = ParseStatus::Error;
        result.error = "not a directory: " + path;
        return result;
    }

    result.status = ParseStatus::Ok;
    result.folder = std::filesystem::canonical(path, ec);
    if (ec) {
        // canonical() can fail on edge cases; fall back to the raw path.
        result.folder = std::filesystem::path(path);
    }
    return result;
}

}  // namespace diffcue::cli
