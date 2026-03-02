#include "intel_hex_decoder.h"

#include <fstream>
#include <stdexcept>
#include <string>

// ---- public ----------------------------------------------------------------

std::vector<uint8_t> IntelHexDecoder::decodeFile(const std::string& filePath)
{
    std::ifstream file(filePath);
    if (!file.is_open())
        throw std::runtime_error("IntelHexDecoder: cannot open file: " + filePath);

    std::vector<uint8_t> bytes;
    std::string line;

    while (std::getline(file, line))
    {
        // Strip Windows-style carriage return
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Every Intel HEX record begins with ':'
        if (line.empty() || line.front() != ':')
            continue;

        const std::string record = line.substr(1); // drop the leading ':'

        // Minimum valid record: 2 (count) + 4 (addr) + 2 (type) + 2 (csum) = 10 chars
        if (record.size() < 10)
            continue;

        const int byteCount  = std::stoi(record.substr(0, 2), nullptr, 16);
        const int recordType = std::stoi(record.substr(6, 2), nullptr, 16);

        // Record types:
        //  0x00 – data
        //  0x01 – end-of-file
        //  0x02 – extended segment address  (ignored)
        //  0x04 – extended linear address   (ignored)
        if (recordType != 0x00)
            continue;

        const std::string hexData = record.substr(8, static_cast<std::size_t>(byteCount) * 2);
        const auto parsed = parseDataRecord(hexData);
        bytes.insert(bytes.end(), parsed.begin(), parsed.end());
    }

    return bytes;
}

// ---- private ---------------------------------------------------------------

std::vector<uint8_t> IntelHexDecoder::parseDataRecord(const std::string& hexData)
{
    std::vector<uint8_t> bytes;

    // Bytes in an Intel HEX data record are already in memory order
    // (the AVR toolchain writes them low-byte-first for little-endian words),
    // so read them sequentially without swapping.
    for (std::size_t i = 0; i + 2 <= hexData.size(); i += 2)
    {
        const uint8_t byte = static_cast<uint8_t>(std::stoi(hexData.substr(i, 2), nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}
