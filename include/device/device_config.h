#pragma once
#include "core/types.h"
#include <string_view>
#include <vector>

namespace avrion {

struct IoRegion {
  u16 data_base = 0;
  u16 length = 0;

  // A stable identifier so Device can wire the right peripheral
  // e.g., "PORTB", "TIMER0", "UART0"
  std::string_view id;

  // Offset added to the region-relative offset before passing to peripheral.
  // Allows mapping non-contiguous register groups to the same peripheral.
  // E.g., if TIMSK0 is at 0x6E but should be seen as register index 5,
  // set periph_offset = 5.
  u16 periph_offset = 0;
};

struct DeviceConfig {
  std::string_view name;

  bool has_22_bit_pc = false; // e.g., AVR8 has 16-bit PC, AVR32 has 22-bit PC

  u32 clock_hz = 0;           // CPU clock frequency in Hz (e.g. 16000000 for 16 MHz)

  // Bytes per interrupt vector table entry.
  // 2 = RJMP (devices with <= 8 KB flash).
  // 4 = JMP  (devices with >  8 KB flash, e.g. ATmega328P).
  u8 interrupt_vector_bytes = 4;

  // Flash (program memory)
  u32 flash_size_bytes = 0;

  // Data-space layout (unified)
  u16 gpr_base = 0x0000;        // usually 0x0000
  u16 gpr_count = 32;           // AVR8 = 32

  u16 io_base = 0x0020;         // start of IO regs in DATA space
  u16 io_size = 0x0040;         // 64 bytes typical
  u16 ext_io_base = 0x0060;     // common
  u16 ext_io_size = 0x00A0;     // device-specific

  u16 sram_base = 0x0100;       // common for many ATmega
  u16 sram_size_bytes = 0;

  u16 x_low_reg = 0;       
  u16 x_high_reg = 0;      
  u16 z_low_reg = 0;
  u16 z_high_reg = 0;

  // Special register addresses in DATA space
  u16 sreg_data_addr = 0;
  u16 spl_data_addr = 0;        // stack pointer low
  u16 sph_data_addr = 0;        // stack pointer high (0 if not present)

  // Reset vector in FLASH byte address (usually 0)
  u32 reset_vector_flash_addr = 0;

  // IO regions to map to peripherals
  std::vector<IoRegion> io_regions;

  u16 data_end() const {
    return static_cast<u16>(sram_base + sram_size_bytes - 1);
  }
};

} // namespace avrion