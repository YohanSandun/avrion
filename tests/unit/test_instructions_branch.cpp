#include <catch2/catch_test_macros.hpp>

#include "cpu/avr_cpu.h"
#include "device/device_config.h"
#include "memory/memory_map.h"

using namespace avrion;

static DeviceConfig make_test_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 32 * 1024;
    c.sram_size_bytes  = 2  * 1024;
    c.sram_base        = 0x0100;
    return c;
}

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

TEST_CASE("JMP - blink.hex first vector: word addr 0x005C -> byte addr 0xB8", "[jmp]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x005C);

    cpu.exec_jmp(0x940C);

    REQUIRE(cpu.pc() == 0x00B8u);
}

TEST_CASE("JMP - word addr 0 -> byte addr 0 (reset vector)", "[jmp]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(2);
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

    cpu.set_pc(6);
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

    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x1000);

    cpu.exec_jmp(0x940C);

    REQUIRE(cpu.pc() == 0x2000u);
}

// ---------------------------------------------------------------------------
// RJMP  (1100 kkkk kkkk kkkk)
//
// Relative jump: PC = PC + k*2, where k is a signed 12-bit word offset.
// PC here is a byte address and is already advanced past the opcode (PC+2)
// before exec_rjmp is called, consistent with the rest of the instruction
// exec model in this codebase.
// Flags: none affected.
// ---------------------------------------------------------------------------

static u16 encode_rjmp(int16_t k)
{
    return static_cast<u16>(0xC000 | (static_cast<u16>(k) & 0x0FFF));
}

TEST_CASE("RJMP - forward jump (positive offset)", "[rjmp]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // PC already past opcode; k=+5 words → +10 bytes
    cpu.set_pc(20);
    cpu.exec_rjmp(encode_rjmp(5));

    REQUIRE(cpu.pc() == 30u);
}

TEST_CASE("RJMP - zero offset stays at current PC", "[rjmp]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(50);
    cpu.exec_rjmp(encode_rjmp(0));

    REQUIRE(cpu.pc() == 50u);
}

TEST_CASE("RJMP - self-loop (k=-1 jumps back to own address)", "[rjmp]")
{
    // Instruction at byte 0; PC is already 2 when exec fires.
    // k=-1 → target = 2 + (-1)*2 = 0
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(2);
    cpu.exec_rjmp(encode_rjmp(-1));

    REQUIRE(cpu.pc() == 0u);
}

TEST_CASE("RJMP - backward jump (negative offset)", "[rjmp]")
{
    // PC=100, k=-5 → target = 100 + (-5)*2 = 90
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(100);
    cpu.exec_rjmp(encode_rjmp(-5));

    REQUIRE(cpu.pc() == 90u);
}

TEST_CASE("RJMP - max positive offset (k=+2047)", "[rjmp]")
{
    // k=0x7FF=2047 → offset_bytes = 4094
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(100);
    cpu.exec_rjmp(encode_rjmp(2047));

    REQUIRE(cpu.pc() == 100u + 4094u);
}

TEST_CASE("RJMP - max negative offset (k=-2048)", "[rjmp]")
{
    // k=-2048 → offset_bytes = -4096; start at 4296 so target = 200
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(4296);
    cpu.exec_rjmp(encode_rjmp(-2048));

    REQUIRE(cpu.pc() == 200u);
}

TEST_CASE("RJMP - flags unaffected", "[rjmp]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    constexpr u8 sentinel = 0b10110101;
    cpu.set_sreg(sentinel);
    cpu.set_pc(20);
    cpu.exec_rjmp(encode_rjmp(3));

    REQUIRE(cpu.sreg() == sentinel);
}
