// model/prompt_builder.h — compose the clipboard prompt from cues (task 6.5, D6).
#pragma once

#include <string>

#include "model/cue_store.h"

namespace diffcue::model {

// Build the follow-up prompt text:
//   # diffcue review cues
//
//   - path/to/file.cpp:42 - this is wrong
//   - path/to/other.h:7 - missing include guard
//
// Cues are grouped by file and sorted by line ascending. Stale cues are
// included (marked) so the reviewer sees them; the user can trim in the
// prompt pane before copying.
std::string build_prompt(const CueStore& store);

}  // namespace diffcue::model
