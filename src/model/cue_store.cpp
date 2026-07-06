// model/cue_store.cpp — cues + atomic JSON sidecar (task 6.4, D5).
//
// Minimal hand-rolled JSON for the simple cues schema (no vendored JSON
// lib). Writes atomically: temp file + rename. The schema carries a
// `version` field (R5) so future drift can be detected and refused.
#include "model/cue_store.h"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>

namespace diffcue::model {

namespace {

// --- minimal JSON string escaping ---
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
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

const char* side_str(Side s) { return s == Side::Old ? "old" : "new"; }

bool side_from_str(std::string_view s, Side& out) {
    if (s == "old") { out = Side::Old; return true; }
    if (s == "new") { out = Side::New; return true; }
    return false;
}

// --- minimal JSON parser for our flat schema ---
// We don't use a general JSON parser; we hand-scan for the fields we expect.
// This is deliberately strict and bails on anything unexpected.

void skip_ws(std::string_view s, size_t& i) {
    while (i < s.size() && (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')) ++i;
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
            switch (e) {
                case '"':  out += '"';  break;
                case '\\': out += '\\'; break;
                case 'n':  out += '\n'; break;
                case 'r':  out += '\r'; break;
                case 't':  out += '\t'; break;
                case '/':  out += '/';  break;
                default:   out += e;    break;
            }
            i += 2;
        } else {
            out += s[i++];
        }
    }
    if (i >= s.size()) return false;
    ++i;  // consume closing quote
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

}  // namespace

CueStore::CueStore(std::filesystem::path folder)
    : folder_(std::move(folder)),
      sidecar_path_(folder_ / ".diffcue" / "cues.json") {
    load();
}

int CueStore::active_count() const {
    int n = 0;
    for (const auto& c : cues_) if (!c.stale) ++n;
    return n;
}

void CueStore::add(Cue cue) {
    if (cue.created == 0) {
        cue.created = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }
    cues_.push_back(std::move(cue));
    save();
}

void CueStore::remove(int index) {
    if (index < 0 || index >= static_cast<int>(cues_.size())) return;
    cues_.erase(cues_.begin() + index);
    save();
}

void CueStore::clear() {
    cues_.clear();
    save();
}

void CueStore::refresh_stale(std::function<int(const std::filesystem::path&, Side)> line_count_for) {
    for (auto& c : cues_) {
        int n = line_count_for(c.file, c.side);
        c.stale = (n == 0 || c.line < 1 || c.line > n);
    }
}

void CueStore::load() {
    std::ifstream f(sidecar_path_);
    if (!f) return;
    std::ostringstream ss;
    ss << f.rdbuf();
    std::string content = ss.str();
    std::string_view s = content;

    size_t i = 0;
    if (!match(s, i, '{')) return;

    int version = 0;
    while (i < s.size()) {
        std::string key;
        if (!parse_string(s, i, key)) break;
        if (!match(s, i, ':')) break;

        if (key == "version") {
            int64_t v = 0;
            if (!parse_number(s, i, v)) break;
            version = static_cast<int>(v);
            if (version > 1) {
                // Refuse to load a newer schema (R5).
                cues_.clear();
                return;
            }
        } else if (key == "folder") {
            std::string folder;
            if (!parse_string(s, i, folder)) break;
            // we don't enforce folder match; cues are file-relative
        } else if (key == "cues") {
            if (!match(s, i, '[')) break;
            while (i < s.size() && s[i] != ']') {
                Cue c;
                if (!match(s, i, '{')) break;
                while (i < s.size() && s[i] != '}') {
                    std::string ck;
                    if (!parse_string(s, i, ck)) { i = s.size(); break; }
                    if (!match(s, i, ':')) { i = s.size(); break; }
                    if (ck == "file") {
                        std::string v;
                        if (!parse_string(s, i, v)) { i = s.size(); break; }
                        c.file = v;
                    } else if (ck == "side") {
                        std::string v;
                        if (!parse_string(s, i, v)) { i = s.size(); break; }
                        side_from_str(v, c.side);
                    } else if (ck == "line") {
                        int64_t v = 0;
                        if (!parse_number(s, i, v)) { i = s.size(); break; }
                        c.line = static_cast<int>(v);
                    } else if (ck == "text") {
                        if (!parse_string(s, i, c.text)) { i = s.size(); break; }
                    } else if (ck == "created") {
                        int64_t v = 0;
                        if (!parse_number(s, i, v)) { i = s.size(); break; }
                        c.created = v;
                    } else {
                        // skip unknown value
                        i = s.size(); break;
                    }
                    skip_ws(s, i);
                    if (i < s.size() && s[i] == ',') ++i;
                }
                if (!match(s, i, '}')) { cues_.clear(); return; }
                cues_.push_back(std::move(c));
                skip_ws(s, i);
                if (i < s.size() && s[i] == ',') ++i;
            }
            if (!match(s, i, ']')) { cues_.clear(); return; }
        } else {
            // skip unknown value (best-effort)
            break;
        }
        skip_ws(s, i);
        if (i < s.size() && s[i] == ',') ++i;
        skip_ws(s, i);
        if (i < s.size() && s[i] == '}') break;
    }
}

void CueStore::save() const {
    std::error_code ec;
    std::filesystem::create_directories(sidecar_path_.parent_path(), ec);

    std::ostringstream ss;
    ss << "{\n  \"version\": 1,\n";
    ss << "  \"folder\": \"" << json_escape(folder_.generic_string()) << "\",\n";
    ss << "  \"cues\": [\n";
    for (size_t k = 0; k < cues_.size(); ++k) {
        const Cue& c = cues_[k];
        ss << "    {"
           << "\"file\": \"" << json_escape(c.file.generic_string()) << "\", "
           << "\"side\": \"" << side_str(c.side) << "\", "
           << "\"line\": " << c.line << ", "
           << "\"text\": \"" << json_escape(c.text) << "\", "
           << "\"created\": " << c.created
           << "}";
        if (k + 1 < cues_.size()) ss << ",";
        ss << "\n";
    }
    ss << "  ]\n}\n";

    // Atomic write: temp file + rename.
    const std::string tmp = sidecar_path_.string() + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return;
        f << ss.str();
    }
    std::filesystem::rename(tmp, sidecar_path_, ec);
    if (ec) {
        // rename failed (cross-device?); fall back to direct write.
        std::ofstream f(sidecar_path_, std::ios::binary | std::ios::trunc);
        if (f) f << ss.str();
        std::filesystem::remove(tmp, ec);
    }
}

}  // namespace diffcue::model
