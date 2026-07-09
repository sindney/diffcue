// model/cue_store.h — in-memory cues + atomic JSON sidecar (task 6.4, D5).
//
// Cues are short per-line comments attached during review. They persist to
// `<folder>/.diffcue/cues.json` so they survive close/reopen. The store
// detects stale cues (whose target line disappeared after an external edit)
// and flags them.
#pragma once

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace diffcue::model {

enum class Side {
    Old,
    New,
};

struct Cue {
    std::filesystem::path file;
    Side side = Side::New;
    int line = 0;              // 1-based
    std::string text;
    int64_t created = 0;       // unix epoch seconds
    bool stale = false;        // set when the target line no longer exists
};

class CueStore {
public:
    // Load cues from `<folder>/.diffcue/cues.json` if it exists. On schema
    // mismatch (version > 1), refuses to load and starts empty.
    explicit CueStore(std::filesystem::path folder);

    const std::vector<Cue>& cues() const { return cues_; }
    std::vector<Cue>& cues() { return cues_; }
    int count() const { return static_cast<int>(cues_.size()); }
    int active_count() const;  // excludes stale cues

    // Add a cue; `created` defaults to now if 0. Writes the sidecar.
    void add(Cue cue);

    // Remove the cue at index. Writes the sidecar.
    void remove(int index);

    // Mark cues whose target line is outside [1, line_count] as stale.
    // `line_count_for(file, side)` returns the current line count or 0 if
    // the file is unknown.
    void refresh_stale(std::function<int(const std::filesystem::path&, Side)> line_count_for);

    // Clear all cues (e.g. user clicked "Clear"). Writes the empty state
    // to the sidecar so the cues don't reappear on next launch.
    void clear();

    const std::filesystem::path& folder() const { return folder_; }

private:
    std::filesystem::path folder_;
    std::filesystem::path sidecar_path_;
    std::vector<Cue> cues_;

    void load();
    void save() const;
};

}  // namespace diffcue::model
