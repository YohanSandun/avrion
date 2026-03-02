#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Decodes Intel HEX (.hex) files into a flat byte array.
// Only data records (type 0x00) are processed; EOF and extended
// address records are recognised but their data is skipped.
class IntelHexDecoder
{
public:
    // Opens and decodes the Intel HEX file at `filePath`.
    // Returns the data bytes in the order they appear in the file
    // (i.e. the exact byte sequence that should be loaded into AVR flash).
    // Throws std::runtime_error if the file cannot be opened.
    static std::vector<uint8_t> decodeFile(const std::string& filePath);

private:
    // Converts a raw data field (hex string of 2*N chars) into bytes.
    // Bytes are read sequentially — Intel HEX already stores them in
    // the correct memory order for AVR targets.
    static std::vector<uint8_t> parseDataRecord(const std::string& hexData);
};
