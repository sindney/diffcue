// model/prefs.cpp — prefs JSON sidecar (task 6.6).
//
// Minimal hand-rolled JSON (same style as cue_store). Schema:
//   { "version": 1, "app_theme": "...", "editor_palette": "...",
//     "diff_mode": "side|inline", "recent_folders": ["...", "..."] }
#include "model/prefs.h"

#include <algorithm>
#include <fstream>
#include <sstream>

#include "platform/paths.h"

namespace diffcue::model {

namespace {

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:   out += c;
        }
    }
    return out;
}

void skip_ws(std::string_view s, size_t& i) {
    while (i < s.size() && (s[i]==' '||s[i]=='\t'||s[i]=='\n'||s[i]=='\r')) ++i;
}

bool match(std::string_view s, size_t& i, char c) {
    skip_ws(s, i);
    if (i < s.size() && s[i] == c) { ++i; return true; }
    return false;
}

bool parse_string(std::string_view s, size_t& i, std::string& out) {
    skip_ws(s, i);
    if (i >= s.size() || s[i] != '"') return false;
    ++i;
    out.clear();
    while (i < s.size() && s[i] != '"') {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char e = s[i + 1];
            out += (e == 'n') ? '\n' : (e == 'r') ? '\r' : (e == 't') ? '\t' : e;
            i += 2;
        } else {
            out += s[i++];
        }
    }
    if (i >= s.size()) return false;
    ++i;
    return true;
}

bool parse_number(std::string_view s, size_t& i, int64_t& out) {
    skip_ws(s, i);
    size_t start = i;
    if (i < s.size() && (s[i] == '-' || s[i] == '+')) ++i;
    while (i < s.size() && (s[i] >= '0' && s[i] <= '9')) ++i;
    if (i == start) return false;
    try {
        out = std::stoll(std::string(s.substr(start, i - start)));
        return true;
    } catch (...) {
        return false;
    }
}

// Parse a JSON array of strings: ["a", "b", ...]. Returns the parsed
// strings on success (caller may post-filter). On any malformed input,
// leaves `i` at the failure point and returns what was parsed so far.
bool parse_string_array(std::string_view s, size_t& i, std::vector<std::string>& out) {
    if (!match(s, i, '[')) return false;
    skip_ws(s, i);
    if (i < s.size() && s[i] == ']') { ++i; return true; }  // empty array
    while (i < s.size()) {
        std::string item;
        if (!parse_string(s, i, item)) return false;
        out.push_back(item);
        skip_ws(s, i);
        if (i < s.size() && s[i] == ',') { ++i; continue; }
        if (i < s.size() && s[i] == ']') { ++i; return true; }
        return false;  // neither comma nor close → malformed
    }
    return false;
}

}  // namespace

Prefs load_prefs(const std::filesystem::path& dir) {
    Prefs p;
    p.editor_palette = "Dark";
    p.diff_mode = DiffMode::SideBySide;

    const auto path = dir / "prefs.json";
    std::ifstream f(path);
    if (!f) return p;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    std::string_view s = content;

    size_t i = 0;
    if (!match(s, i, '{')) return p;
    while (i < s.size()) {
        std::string key;
        if (!parse_string(s, i, key)) break;
        if (!match(s, i, ':')) break;
        if (key == "version") {
            int64_t v = 0;
            if (!parse_number(s, i, v)) break;
        } else if (key == "app_theme") {
            if (!parse_string(s, i, p.app_theme)) break;
        } else if (key == "editor_palette") {
            if (!parse_string(s, i, p.editor_palette)) break;
        } else if (key == "diff_mode") {
            std::string v;
            if (!parse_string(s, i, v)) break;
            p.diff_mode = (v == "inline") ? DiffMode::Inline : DiffMode::SideBySide;
        } else if (key == "recent_folders") {
            std::vector<std::string> raw;
            if (!parse_string_array(s, i, raw)) break;
            // Filter out paths that no longer exist on disk (spec: "Missing
            // folder is dropped at load"). Keep order intact.
            p.recent_folders.clear();
            p.recent_folders.reserve(raw.size());
            for (const auto& str : raw) {
                std::error_code ec;
                if (std::filesystem::exists(str, ec)) {
                    p.recent_folders.emplace_back(str);
                }
            }
        } else {
            // Skip unknown value: try string first, then number, then array.
            std::string ds;
            int64_t dn = 0;
            std::vector<std::string> da;
            if (!parse_string(s, i, ds)) {
                if (!parse_number(s, i, dn)) {
                    parse_string_array(s, i, da);
                }
            }
        }
        skip_ws(s, i);
        if (i < s.size() && s[i] == ',') ++i;
        skip_ws(s, i);
        if (i < s.size() && s[i] == '}') break;
    }
    return p;
}

void save_prefs(const std::filesystem::path& dir, const Prefs& prefs) {
    std::error_code ec;
    const auto path = dir / "prefs.json";
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"version\": 1,\n";
    ss << "  \"app_theme\": \"" << json_escape(prefs.app_theme) << "\",\n";
    ss << "  \"editor_palette\": \"" << json_escape(prefs.editor_palette) << "\",\n";
    ss << "  \"diff_mode\": \"" << (prefs.diff_mode == DiffMode::Inline ? "inline" : "side") << "\",\n";
    ss << "  \"recent_folders\": [";
    for (size_t k = 0; k < prefs.recent_folders.size(); ++k) {
        if (k) ss << ", ";
        ss << "\"" << json_escape(prefs.recent_folders[k].string()) << "\"";
    }
    ss << "]\n";
    ss << "}\n";

    const std::string tmp = path.string() + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return;
        f << ss.str();
    }
    std::filesystem::rename(tmp, path, ec);
    if (ec) {
        std::ofstream f(path, std::ios::binary | std::ios::trunc);
        if (f) f << ss.str();
        std::filesystem::remove(tmp, ec);
    }
}

Prefs load_prefs() {
    return load_prefs(platform::config_dir());
}

void save_prefs(const Prefs& prefs) {
    save_prefs(platform::config_dir(), prefs);
}

}  // namespace diffcue::model
