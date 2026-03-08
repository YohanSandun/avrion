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

// ---------------------------------------------------------------------------
// AVR SREG bit masks
// ---------------------------------------------------------------------------
static constexpr u8 SREG_C = 0x01; // Carry
static constexpr u8 SREG_Z = 0x02; // Zero
static constexpr u8 SREG_N = 0x04; // Negative
static constexpr u8 SREG_V = 0x08; // Overflow
static constexpr u8 SREG_S = 0x10; // Sign (N XOR V)

// ---------------------------------------------------------------------------
// EOR  (0010 01rd dddd rrrr)
//
// Operation : Rd ← Rd XOR Rr
// Flags     : Z = (result == 0), N = result[7], V = 0, S = N XOR V = N
//             C is unaffected.
// ---------------------------------------------------------------------------

// Encode EOR Rd, Rr opcode from register indices.
static u16 encode_eor(u8 d, u8 r)
{
    return static_cast<u16>(0x2400
        | ((d & 0x1F) << 4)
        | ((r & 0x10) << 5)
        | (r & 0x0F));
}

// ---------------------------------------------------------------------------
// Result and register tests
// ---------------------------------------------------------------------------

TEST_CASE("EOR - basic XOR of two values", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(1, 0xAA);
    cpu.set_reg(2, 0x55);
    cpu.exec_eor(encode_eor(1, 2));

    REQUIRE(cpu.reg(1) == 0xFF);
    REQUIRE(cpu.reg(2) == 0x55); // source register unchanged
}

TEST_CASE("EOR - result stored in Rd, Rr unchanged", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(5, 0x0F);
    cpu.set_reg(6, 0xF0);
    cpu.exec_eor(encode_eor(5, 6));

    REQUIRE(cpu.reg(5) == 0xFF);
    REQUIRE(cpu.reg(6) == 0xF0);
}

TEST_CASE("EOR - self-XOR clears register (CLR idiom)", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(3, 0xAB);
    cpu.exec_eor(encode_eor(3, 3));

    REQUIRE(cpu.reg(3) == 0x00);
}

TEST_CASE("EOR - works with upper registers R16-R31", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0b10110011);
    cpu.set_reg(17, 0b01001100);
    cpu.exec_eor(encode_eor(16, 17));

    REQUIRE(cpu.reg(16) == 0xFF);
}

// ---------------------------------------------------------------------------
// Z flag (Zero)
// ---------------------------------------------------------------------------

TEST_CASE("EOR - Z set when result is zero", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0xCD);
    cpu.set_reg(1, 0xCD);
    cpu.exec_eor(encode_eor(0, 1));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("EOR - Z cleared when result is non-zero", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set Z to verify it gets cleared
    cpu.set_reg(0, 0xAA);
    cpu.set_reg(1, 0x55);
    cpu.exec_eor(encode_eor(0, 1));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

// ---------------------------------------------------------------------------
// N flag (Negative)
// ---------------------------------------------------------------------------

TEST_CASE("EOR - N set when bit 7 of result is set", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0xAA); // 0xAA XOR 0x55 = 0xFF  (bit 7 set)
    cpu.set_reg(1, 0x55);
    cpu.exec_eor(encode_eor(0, 1));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("EOR - N cleared when bit 7 of result is clear", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set N to verify it gets cleared
    cpu.set_reg(0, 0x0F);
    cpu.set_reg(1, 0x01); // 0x0F XOR 0x01 = 0x0E  (bit 7 clear)
    cpu.exec_eor(encode_eor(0, 1));

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

// ---------------------------------------------------------------------------
// V flag (Overflow) — always cleared by EOR
// ---------------------------------------------------------------------------

TEST_CASE("EOR - V always cleared", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set V to verify it gets cleared
    cpu.set_reg(0, 0xAA);
    cpu.set_reg(1, 0x55);
    cpu.exec_eor(encode_eor(0, 1));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

// ---------------------------------------------------------------------------
// S flag (Sign = N XOR V)
// Since V is always 0 after EOR, S == N.
// ---------------------------------------------------------------------------

TEST_CASE("EOR - S equals N when result is negative", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0xAA); // result = 0xFF, N=1 → S=1
    cpu.set_reg(1, 0x55);
    cpu.exec_eor(encode_eor(0, 1));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("EOR - S cleared when result is positive", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set S to verify it gets cleared
    cpu.set_reg(0, 0x0F); // result = 0x0E, N=0 → S=0
    cpu.set_reg(1, 0x01);
    cpu.exec_eor(encode_eor(0, 1));

    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

// ---------------------------------------------------------------------------
// C flag — must not be modified by EOR
// ---------------------------------------------------------------------------

TEST_CASE("EOR - C flag not affected", "[eor][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // C set before EOR — must remain set
    cpu.set_sreg(SREG_C);
    cpu.set_reg(0, 0x0F);
    cpu.set_reg(1, 0x01);
    cpu.exec_eor(encode_eor(0, 1));

    REQUIRE((cpu.sreg() & SREG_C) != 0);

    // C clear before EOR — must remain clear
    cpu.set_sreg(0x00);
    cpu.set_reg(0, 0x0F);
    cpu.set_reg(1, 0x01);
    cpu.exec_eor(encode_eor(0, 1));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}
