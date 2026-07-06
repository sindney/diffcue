// cli/args.h — argument parsing for `diffcue [--help] [--version] <folder>`.
//
// Design D8: hand-rolled parser, no CLI11. Surface is tiny.
// Task 4.1: ParsedArgs + parse_args.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace diffcue::cli {

enum class ParseStatus {
    Ok,         // folder provided (or no folder → picker fallback)
    Help,       // --help / -h requested
    Version,    // --version / -V requested
    Error,      // unknown flag / too many positionals / not a directory
};

struct ParsedArgs {
    ParseStatus status = ParseStatus::Ok;
    std::optional<std::filesystem::path> folder;
    std::string error;  // human-readable, written to stderr when status == Error
};

// Parse argv[1..argc). argv[0] is the program name and is skipped.
// On Help/Version, the caller is expected to print the canned message and
// exit(0). On Error, the caller prints `error` to stderr and exit(2).
ParsedArgs parse_args(int argc, char** argv);

// Usage text printed for --help / -h.
std::string usage_text();

}  // namespace diffcue::cli
