// model/prompt_builder.cpp — compose the clipboard prompt (task 6.5, D6).
#include "model/prompt_builder.h"

#include <algorithm>
#include <map>
#include <vector>

namespace diffcue::model {

namespace {

// Collapse internal newlines/tabs in cue text to single spaces so each cue
// renders as one line in the prompt. A raw newline in the cue text would
// break the `- file:line - text` list format.
std::string flatten(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool prev_ws = false;
    for (char c : s) {
        if (c == '\n' || c == '\r' || c == '\t') {
            if (!prev_ws) { out += ' '; prev_ws = true; }
        } else if (c == ' ') {
            if (!prev_ws) { out += ' '; prev_ws = true; }
        } else {
            out += c;
            prev_ws = false;
        }
    }
    // trim trailing whitespace
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out;
}

}  // namespace

std::string build_prompt(const CueStore& store) {
    // Group cues by file, sort each group by line ascending.
    std::map<std::string, std::vector<const Cue*>> by_file;
    for (const auto& c : store.cues()) {
        by_file[c.file.generic_string()].push_back(&c);
    }
    for (auto& [_, vec] : by_file) {
        std::sort(vec.begin(), vec.end(),
                  [](const Cue* a, const Cue* b) { return a->line < b->line; });
    }

    std::string out;
    for (const auto& [file, vec] : by_file) {
        for (const Cue* c : vec) {
            out += "- ";
            out += file;
            out += ":";
            out += std::to_string(c->line);
            out += " - ";
            out += flatten(c->text);
            if (c->stale) out += "  [stale: line no longer exists]";
            out += "\n";
        }
    }
    return out;
}

}  // namespace diffcue::model
