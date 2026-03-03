#include <iostream>
#include "device/atmega328p.h"
#include "intel_hex_decoder.h"

#ifndef BLINK_HEX_PATH
#  define BLINK_HEX_PATH "tests/data/blink.hex"
#endif

int main() {
    avrion::ATmega328P dev;

    const std::vector<uint8_t> flash_data = IntelHexDecoder::decodeFile(BLINK_HEX_PATH);
    dev.load_flash(0, flash_data.data(), flash_data.size());
    dev.reset();
   

    std::cout << "Program Loaded!" << std::endl;
    return 0;
}