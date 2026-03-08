#include <catch2/catch_test_macros.hpp>

#include "cpu/avr_cpu.h"
#include "device/device_config.h"
#include "memory/memory_map.h"

using namespace avrion;

// Minimal device config sufficient for instruction tests (no peripherals).
static DeviceConfig make_test_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 32 * 1024;
    c.sram_size_bytes  = 2  * 1024;
    c.sram_base        = 0x0100;
    return c;
}

// Write a 16-bit little-endian value into the flash vector at byte address.
static void flash_write16(std::vector<u8>& flash, u32 byte_addr, u16 value)
{
    flash[byte_addr]     = static_cast<u8>(value & 0xFF);
    flash[byte_addr + 1] = static_cast<u8>(value >> 8);
}

// ---------------------------------------------------------------------------
// JMP  (1001 010k kkkk 110k  +  kkkkkkkk kkkkkkkk)
//
// The 22-bit word address k is split across the opcode and the second word:
//   opcode bits [8:4] = k[21:17]
//   opcode bit  [0]   = k[16]
//   second word       = k[15:0]
//
// After decoding: PC_byte = k << 1   (word address → byte address)
// ---------------------------------------------------------------------------

TEST_CASE("JMP - blink.hex first vector: word addr 0x005C → byte addr 0xB8", "[jmp]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // PC = 0; second word at flash[2:3] = 0x005C (little-endian)
    cpu.set_pc(0);
    flash_write16(mem.flash(), 2, 0x005C);

    // Opcode 0x940C: JMP with all k bits in the opcode word = 0
    cpu.exec_jmp(0x940C);

    REQUIRE(cpu.pc() == 0x00B8u);
}

TEST_CASE("JMP - word addr 0 → byte addr 0 (reset vector)", "[jmp]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(0);
    flash_write16(mem.flash(), 2, 0x0000);

    cpu.exec_jmp(0x940C);

    REQUIRE(cpu.pc() == 0x0000u);
}

TEST_CASE("JMP - target is read relative to current PC", "[jmp]")
{
    // If PC = 4 the second word must be read from flash byte 6, not byte 2.
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(4);
    flash_write16(mem.flash(), 6, 0x005C); // correct slot for this PC
    flash_write16(mem.flash(), 2, 0xFFFF); // wrong slot — must not be used

    cpu.exec_jmp(0x940C);

    REQUIRE(cpu.pc() == 0x00B8u);
}

TEST_CASE("JMP - word addr in second word only (large address)", "[jmp]")
{
    // word addr = 0x1000 → byte addr = 0x2000
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(0);
    flash_write16(mem.flash(), 2, 0x1000);

    cpu.exec_jmp(0x940C);

    REQUIRE(cpu.pc() == 0x2000u);
}
