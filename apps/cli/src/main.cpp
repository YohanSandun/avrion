#include <iostream>
#include <exception>
#include "device/atmega328p.h"
#include "intel_hex_decoder.h"

#ifndef BLINK_HEX_PATH
#  define BLINK_HEX_PATH "tests/data/blink.hex"
#endif

int main() {
    try {
        avrion::ATmega328P dev;

        const std::vector<uint8_t> flash_data = IntelHexDecoder::decodeFile(BLINK_HEX_PATH);
        dev.load_flash(0, flash_data.data(), flash_data.size());
        
        dev.run_cycles(1000000);

        dev.reset();

        std::cout << "Program loaded into flash." << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "avr_cli error: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "avr_cli error: unknown fatal error" << std::endl;
        return 1;
    }
}