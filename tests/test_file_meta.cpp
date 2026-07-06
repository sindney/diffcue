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
