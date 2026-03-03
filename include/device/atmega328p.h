#pragma once
#include "device/device.h"

namespace avrion
{

    class ATmega328P : public Device
    {
    public:
        ATmega328P();

    private:
        inline DeviceConfig make_atmega328p_config()
        {
            DeviceConfig c;
            c.name = "ATmega328P";

            c.flash_size_bytes = 32 * 1024;
            c.sram_size_bytes = 2 * 1024;

            c.gpr_base = 0x0000;
            c.gpr_count = 32;

            c.io_base = 0x0020;
            c.io_size = 0x0040;
            c.ext_io_base = 0x0060;
            c.ext_io_size = 0x00A0;

            c.sram_base = 0x0100;

            // SREG is IO offset 0x3F => data addr 0x5F
            c.sreg_data_addr = 0x005F;

            // SPL at IO offset 0x3D => 0x5D, SPH at 0x3E => 0x5E
            c.spl_data_addr = 0x005D;
            c.sph_data_addr = 0x005E;

            c.reset_vector_flash_addr = 0x0000;

            // GPIO port regions — data-space addresses (IO offset + 0x20)
            // Each port: PIN (offset 0), DDR (offset 1), PORT (offset 2)
            // PORTB: PINB=0x23, DDRB=0x24, PORTB=0x25
            c.io_regions.push_back({ 0x0023, 3, "PORTB" });
            // PORTC: PINC=0x26, DDRC=0x27, PORTC=0x28
            c.io_regions.push_back({ 0x0026, 3, "PORTC" });
            // PORTD: PIND=0x29, DDRD=0x2A, PORTD=0x2B
            c.io_regions.push_back({ 0x0029, 3, "PORTD" });

            return c;
        }
    };

} // namespace avrion