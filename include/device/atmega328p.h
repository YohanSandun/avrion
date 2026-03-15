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

            c.has_22_bit_pc = false;
            c.clock_hz = 16'000'000;

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

            // XL = R26, XH = R27
            c.x_low_reg = 26;  
            c.x_high_reg = 27;

            // YL = R28, YH = R29
            c.y_low_reg = 28;  
            c.y_high_reg = 29;

            // ZL = R30, ZH = R31
            c.z_low_reg = 30;
            c.z_high_reg = 31;

            c.reset_vector_flash_addr = 0x0000;

            // GPIO port regions — data-space addresses (IO offset + 0x20)
            // Each port: PIN (offset 0), DDR (offset 1), PORT (offset 2)
            // PORTB: PINB=0x23, DDRB=0x24, PORTB=0x25
            c.io_regions.push_back({ 0x0023, 3, "PORTB" });
            // PORTC: PINC=0x26, DDRC=0x27, PORTC=0x28
            c.io_regions.push_back({ 0x0026, 3, "PORTC" });
            // PORTD: PIND=0x29, DDRD=0x2A, PORTD=0x2B
            c.io_regions.push_back({ 0x0029, 3, "PORTD" });

            // Timer0 registers (scattered across IO space)
            // TIFR0 at 0x35 (1 byte) → periph register index 6 (TIFRx)
            c.io_regions.push_back({ 0x0035, 1, "TIMER0", 6 });
            // TCCR0A=0x44, TCCR0B=0x45, TCNT0=0x46, OCR0A=0x47, OCR0B=0x48
            // → periph register indices 0–4 (TCCRxA through OCRxB)
            c.io_regions.push_back({ 0x0044, 5, "TIMER0", 0 });
            // TIMSK0 at 0x6E (1 byte) → periph register index 5 (TIMSKx)
            c.io_regions.push_back({ 0x006E, 1, "TIMER0", 5 });

            // USART0 registers
            // UCSR0A=0xC0, UCSR0B=0xC1, UCSR0C=0xC2 → periph offsets 0-2
            c.io_regions.push_back({ 0x00C0, 3, "USART0", 0 });
            // 0xC3 is reserved; UBRR0L=0xC4, UBRR0H=0xC5, UDR0=0xC6 → periph offsets 3-5
            c.io_regions.push_back({ 0x00C4, 3, "USART0", 3 });

            return c;
        }
    };

} // namespace avrion