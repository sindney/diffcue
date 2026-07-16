// model/file_meta.cpp — encoding + EOL detection (task 6.1, design D4).
//
// Rule (D4): BOM check first, then NULL byte → binary, then UTF-8 validity,
// then assume latin1. EOL: count \r\n, lone \r, lone \n — pick the majority,
// else "mixed".
#include "model/file_meta.h"

#include <cctype>

namespace diffcue::model {

namespace {

// Validate a byte sequence as UTF-8. Returns false on any invalid byte.
bool is_valid_utf8(std::string_view b) {
    size_t i = 0;
    const size_t n = b.size();
    while (i < n) {
        unsigned char c = static_cast<unsigned char>(b[i]);
        if (c <= 0x7F) {
            ++i;
        } else if ((c & 0xE0) == 0xC0) {
            // 2-byte
            if (i + 1 >= n) return false;
            unsigned char c2 = static_cast<unsigned char>(b[i + 1]);
            if ((c2 & 0xC0) != 0x80) return false;
            // overlong check
            if (c < 0xC2) return false;
            i += 2;
        } else if ((c & 0xF0) == 0xE0) {
            if (i + 2 >= n) return false;
            unsigned char c2 = static_cast<unsigned char>(b[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(b[i + 2]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80) return false;
            // overlong / surrogate checks
            if (c == 0xE0 && c2 < 0xA0) return false;
            if (c == 0xED && c2 >= 0xA0) return false;
            i += 3;
        } else if ((c & 0xF8) == 0xF0) {
            if (i + 3 >= n) return false;
            unsigned char c2 = static_cast<unsigned char>(b[i + 1]);
            unsigned char c3 = static_cast<unsigned char>(b[i + 2]);
            unsigned char c4 = static_cast<unsigned char>(b[i + 3]);
            if ((c2 & 0xC0) != 0x80 || (c3 & 0xC0) != 0x80 || (c4 & 0xC0) != 0x80)
                return false;
            if (c == 0xF0 && c2 < 0x90) return false;
            if (c == 0xF4 && c2 > 0x8F) return false;
            i += 4;
        } else {
            return false;
        }
    }
    return true;
}

}  // namespace

FileMeta detect_meta(std::string_view b) {
    FileMeta m;

    // --- Encoding ---
    // BOM checks first.
    if (b.size() >= 3 &&
        static_cast<unsigned char>(b[0]) == 0xEF &&
        static_cast<unsigned char>(b[1]) == 0xBB &&
        static_cast<unsigned char>(b[2]) == 0xBF) {
        m.encoding = Encoding::Utf8Bom;
    } else if (b.size() >= 2 &&
               static_cast<unsigned char>(b[0]) == 0xFF &&
               static_cast<unsigned char>(b[1]) == 0xFE) {
        m.encoding = Encoding::Utf16LE;
    } else if (b.size() >= 2 &&
               static_cast<unsigned char>(b[0]) == 0xFE &&
               static_cast<unsigned char>(b[1]) == 0xFF) {
        m.encoding = Encoding::Utf16BE;
    } else {
        // NULL byte → binary.
        bool has_null = false;
        for (size_t i = 0; i < b.size(); ++i) {
            if (b[i] == '\0') { has_null = true; break; }
        }
        if (has_null) {
            m.encoding = Encoding::Binary;
        } else if (is_valid_utf8(b)) {
            m.encoding = Encoding::Utf8;
        } else {
            m.encoding = Encoding::Latin1;
        }
    }

    // --- EOL ---
    // Count \r\n, lone \r, lone \n.
    size_t crlf = 0, cr = 0, lf = 0;
    for (size_t i = 0; i < b.size(); ++i) {
        if (b[i] == '\r') {
            if (i + 1 < b.size() && b[i + 1] == '\n') {
                ++crlf;
                // skip the \n
            } else {
                ++cr;
            }
        } else if (b[i] == '\n') {
            // \n not preceded by \r (the \r\n case was handled above)
            if (i == 0 || b[i - 1] != '\r') {
                ++lf;
            }
        }
    }

    const size_t kinds = (crlf > 0 ? 1 : 0) + (cr > 0 ? 1 : 0) + (lf > 0 ? 1 : 0);
    if (kinds == 0) {
        m.eol = Eol::None;
    } else if (kinds > 1) {
        m.eol = Eol::Mixed;
    } else if (crlf > 0) {
        m.eol = Eol::Crlf;
    } else if (cr > 0) {
        m.eol = Eol::Cr;
    } else {
        m.eol = Eol::Lf;
    }

    return m;
}

bool is_binary_extension(std::string_view ext) {
    // Strip a leading dot if present (path::extension() includes it).
    if (!ext.empty() && ext[0] == '.') ext.remove_prefix(1);

    // Lowercase into a stack buffer for case-insensitive compare.
    char buf[16];
    if (ext.empty() || ext.size() >= sizeof(buf)) return false;
    for (size_t i = 0; i < ext.size(); ++i)
        buf[i] = static_cast<char>(std::tolower(
            static_cast<unsigned char>(ext[i])));
    std::string_view low(buf, ext.size());

    // Curated list of unambiguously-binary extensions.
    constexpr std::string_view kBinary[] = {
        // Libraries / object code / executables
        "dll", "lib", "a", "so", "dylib", "o", "obj", "exe", "out", "app",
        "pdb", "idb", "ilk", "exp", "map",
        // Images
        "png", "jpg", "jpeg", "gif", "bmp", "ico", "tif", "tiff", "webp",
        // Archives
        "zip", "tar", "gz", "bz2", "xz", "7z", "rar", "jar", "war", "tgz",
        // Office / PDF
        "pdf", "doc", "docx", "xls", "xlsx", "ppt", "pptx",
        // Audio / video
        "mp3", "wav", "ogg", "flac", "aac", "m4a",
        "mp4", "avi", "mkv", "mov", "webm",
        // Compiled bytecode
        "class", "pyc", "pyo", "wasm",
        // Fonts
        "ttf", "otf", "woff", "woff2", "eot",
        // Databases / keystores
        "db", "sqlite", "sqlite3", "mdb", "p12", "pfx", "keystore",
    };
    for (auto b : kBinary) {
        if (low == b) return true;
    }
    return false;
}

const char* encoding_label(Encoding e) {
    switch (e) {
        case Encoding::Utf8Bom:  return "UTF-8-BOM";
        case Encoding::Utf16LE:  return "UTF-16LE";
        case Encoding::Utf16BE:  return "UTF-16BE";
        case Encoding::Utf8:     return "UTF-8";
        case Encoding::Latin1:   return "latin1";
        case Encoding::Binary:   return "binary";
    }
    return "?";
}

const char* eol_label(Eol e) {
    switch (e) {
        case Eol::Lf:    return "LF";
        case Eol::Crlf:  return "CRLF";
        case Eol::Cr:    return "CR";
        case Eol::Mixed: return "Mixed";
        case Eol::None:  return "None";
    }
    return "?";
}

}  // namespace diffcue::model
