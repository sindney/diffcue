// model/file_meta.h — encoding + EOL detection (task 6.1, design D4).
//
// detect_meta(bytes) classifies a blob's encoding (UTF-8-BOM / UTF-16LE /
// UTF-16BE / UTF-8 / latin1 / binary) and line-ending type (LF / CRLF / CR /
// Mixed / None). These are real sources of hidden review failures that
// Beyond-Compare-style tools surface.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace diffcue::model {

enum class Encoding {
    Utf8Bom,
    Utf16LE,
    Utf16BE,
    Utf8,
    Latin1,
    Binary,
};

enum class Eol {
    Lf,     // \n only
    Crlf,   // \r\n
    Cr,     // \r only (classic Mac)
    Mixed,  // more than one of the above
    None,   // no line endings (single-line / empty)
};

struct FileMeta {
    Encoding encoding = Encoding::Utf8;
    Eol eol = Eol::None;
};

// Classify a blob. `bytes` is the raw file content (may contain NULs).
FileMeta detect_meta(std::string_view bytes);

// Fast path-based heuristic: true if `ext` (e.g. ".dll" or "dll") is a known
// binary file extension. Does not read file content. Case-insensitive.
bool is_binary_extension(std::string_view ext);

const char* encoding_label(Encoding e);
const char* eol_label(Eol e);

}  // namespace diffcue::model
