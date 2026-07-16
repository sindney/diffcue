// tests/test_file_meta.cpp — unit tests for file_meta (task 6.7).
#include "catch_amalgamated.hpp"
#include "model/file_meta.h"

using namespace diffcue::model;

TEST_CASE("detect_meta: empty → UTF-8, None", "[file_meta]") {
    auto m = detect_meta("");
    REQUIRE(m.encoding == Encoding::Utf8);
    REQUIRE(m.eol == Eol::None);
}

TEST_CASE("detect_meta: plain ASCII LF", "[file_meta]") {
    auto m = detect_meta("hello\nworld\n");
    REQUIRE(m.encoding == Encoding::Utf8);
    REQUIRE(m.eol == Eol::Lf);
}

TEST_CASE("detect_meta: CRLF", "[file_meta]") {
    auto m = detect_meta("hello\r\nworld\r\n");
    REQUIRE(m.eol == Eol::Crlf);
}

TEST_CASE("detect_meta: lone CR", "[file_meta]") {
    auto m = detect_meta("hello\rworld\r");
    REQUIRE(m.eol == Eol::Cr);
}

TEST_CASE("detect_meta: mixed EOL", "[file_meta]") {
    auto m = detect_meta("hello\nworld\r\n");
    REQUIRE(m.eol == Eol::Mixed);
}

TEST_CASE("detect_meta: UTF-8 BOM", "[file_meta]") {
    std::string s;
    s += char(0xEF); s += char(0xBB); s += char(0xBF);
    s += "hello\n";
    auto m = detect_meta(s);
    REQUIRE(m.encoding == Encoding::Utf8Bom);
}

TEST_CASE("detect_meta: UTF-16LE BOM", "[file_meta]") {
    std::string s;
    s += char(0xFF); s += char(0xFE);
    auto m = detect_meta(s);
    REQUIRE(m.encoding == Encoding::Utf16LE);
}

TEST_CASE("detect_meta: UTF-16BE BOM", "[file_meta]") {
    std::string s;
    s += char(0xFE); s += char(0xFF);
    auto m = detect_meta(s);
    REQUIRE(m.encoding == Encoding::Utf16BE);
}

TEST_CASE("detect_meta: binary (NUL byte)", "[file_meta]") {
    // Use the (const char*, size_t) constructor so the embedded NUL is
    // included — the C-string constructor would truncate at the first NUL.
    std::string s("hello\0world\n", 12);
    auto m = detect_meta(s);
    REQUIRE(m.encoding == Encoding::Binary);
}

TEST_CASE("detect_meta: invalid UTF-8 → latin1", "[file_meta]") {
    // 0xFF is not valid UTF-8 (would start a 2-byte seq but 0xFF is invalid).
    std::string s = "caf\xe9\n";  // 'café' in latin1
    auto m = detect_meta(s);
    REQUIRE(m.encoding == Encoding::Latin1);
}

TEST_CASE("encoding_label / eol_label", "[file_meta]") {
    REQUIRE(std::string(encoding_label(Encoding::Utf8Bom)) == "UTF-8-BOM");
    REQUIRE(std::string(eol_label(Eol::Crlf)) == "CRLF");
}

TEST_CASE("is_binary_extension: common binary extensions", "[file_meta]") {
    REQUIRE(is_binary_extension(".dll"));
    REQUIRE(is_binary_extension(".lib"));
    REQUIRE(is_binary_extension(".a"));
    REQUIRE(is_binary_extension(".so"));
    REQUIRE(is_binary_extension(".dylib"));
    REQUIRE(is_binary_extension(".o"));
    REQUIRE(is_binary_extension(".obj"));
    REQUIRE(is_binary_extension(".exe"));
    REQUIRE(is_binary_extension(".pdb"));
    REQUIRE(is_binary_extension(".map"));
    REQUIRE(is_binary_extension(".png"));
    REQUIRE(is_binary_extension(".jpg"));
    REQUIRE(is_binary_extension(".zip"));
    REQUIRE(is_binary_extension(".pdf"));
    REQUIRE(is_binary_extension(".pyc"));
    REQUIRE(is_binary_extension(".wasm"));
}

TEST_CASE("is_binary_extension: works without leading dot", "[file_meta]") {
    REQUIRE(is_binary_extension("dll"));
    REQUIRE(is_binary_extension("png"));
    REQUIRE(is_binary_extension("so"));
}

TEST_CASE("is_binary_extension: case-insensitive", "[file_meta]") {
    REQUIRE(is_binary_extension(".DLL"));
    REQUIRE(is_binary_extension(".Lib"));
    REQUIRE(is_binary_extension(".SO"));
}

TEST_CASE("is_binary_extension: text extensions are not binary", "[file_meta]") {
    REQUIRE_FALSE(is_binary_extension(".cpp"));
    REQUIRE_FALSE(is_binary_extension(".h"));
    REQUIRE_FALSE(is_binary_extension(".txt"));
    REQUIRE_FALSE(is_binary_extension(".md"));
    REQUIRE_FALSE(is_binary_extension(".json"));
    REQUIRE_FALSE(is_binary_extension(".py"));
    REQUIRE_FALSE(is_binary_extension(".xml"));
    REQUIRE_FALSE(is_binary_extension(".sh"));
}

TEST_CASE("is_binary_extension: empty / edge cases", "[file_meta]") {
    REQUIRE_FALSE(is_binary_extension(""));
    REQUIRE_FALSE(is_binary_extension("."));
    REQUIRE_FALSE(is_binary_extension(".cppp"));
    REQUIRE_FALSE(is_binary_extension(".abcdefghij"));
}
