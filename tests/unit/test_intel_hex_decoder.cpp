#include <catch2/catch_test_macros.hpp>

#include "intel_hex_decoder.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

// Path injected by CMake so the test binary can locate the fixture regardless
// of where it is run from.
#ifndef TEST_DATA_DIR
#  define TEST_DATA_DIR "."
#endif

static const std::string kBlinkHex = TEST_DATA_DIR "/blink.hex";

// ---------------------------------------------------------------------------
// First record of blink.hex:
//   :100000000C945C000C946E000C946E000C946E00CA
//   byte count = 0x10 (16), address = 0x0000, type = 0x00 (data)
//   data bytes (in file order, already correct memory layout):
//     0C 94 5C 00  0C 94 6E 00  0C 94 6E 00  0C 94 6E 00
// ---------------------------------------------------------------------------

TEST_CASE("IntelHexDecoder - result is non-empty for a valid file", "[decoder]")
{
    const auto bytes = IntelHexDecoder::decodeFile(kBlinkHex);
    REQUIRE_FALSE(bytes.empty());
}

TEST_CASE("IntelHexDecoder - total byte count matches blink.hex", "[decoder]")
{
    // 57 data records × 16 bytes + 1 data record × 12 bytes = 924 bytes
    const auto bytes = IntelHexDecoder::decodeFile(kBlinkHex);
    REQUIRE(bytes.size() == 924u);
}

TEST_CASE("IntelHexDecoder - first 16 bytes match first data record", "[decoder]")
{
    const auto bytes = IntelHexDecoder::decodeFile(kBlinkHex);
    REQUIRE(bytes.size() >= 16u);

    const std::vector<uint8_t> expected = {
        0x0C, 0x94, 0x5C, 0x00,
        0x0C, 0x94, 0x6E, 0x00,
        0x0C, 0x94, 0x6E, 0x00,
        0x0C, 0x94, 0x6E, 0x00,
    };
    const std::vector<uint8_t> actual(bytes.begin(), bytes.begin() + 16);
    REQUIRE(actual == expected);
}

TEST_CASE("IntelHexDecoder - last 12 bytes match last data record", "[decoder]")
{
    // Last data record: :0C039000A1F30E940000F1CFF894FFCF11
    const auto bytes = IntelHexDecoder::decodeFile(kBlinkHex);
    REQUIRE(bytes.size() >= 12u);

    const std::vector<uint8_t> expected = {
        0xA1, 0xF3, 0x0E, 0x94,
        0x00, 0x00, 0xF1, 0xCF,
        0xF8, 0x94, 0xFF, 0xCF,
    };
    const std::vector<uint8_t> actual(bytes.end() - 12, bytes.end());
    REQUIRE(actual == expected);
}

TEST_CASE("IntelHexDecoder - throws on missing file", "[decoder]")
{
    REQUIRE_THROWS_AS(
        IntelHexDecoder::decodeFile("nonexistent_file.hex"),
        std::runtime_error
    );
}
