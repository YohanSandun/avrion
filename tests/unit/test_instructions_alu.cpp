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
static constexpr u8 SREG_H = 0x20; // Half Carry

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

// ---------------------------------------------------------------------------
// DEC  (1001 010d dddd 1010)
//
// Operation : Rd <- Rd - 1
// Flags     : Z, N, V, S (N XOR V)
// ---------------------------------------------------------------------------

// Encode DEC Rd
static u16 encode_dec(u8 d)
{
    return static_cast<u16>(0x940A | ((d & 0x1F) << 4));
}

TEST_CASE("DEC - decrements register value", "[dec][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(5, 0x02);
    cpu.exec_dec(encode_dec(5));

    REQUIRE(cpu.reg(5) == 0x01);
}

TEST_CASE("DEC - zero result sets Z flag", "[dec][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(7, 0x01);
    cpu.set_sreg(0x00);
    cpu.exec_dec(encode_dec(7));

    REQUIRE(cpu.reg(7) == 0x00);
    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("DEC - negative result sets N flag", "[dec][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(4, 0x00); // 0x00 - 1 = 0xFF (negative)
    cpu.exec_dec(encode_dec(4));

    REQUIRE(cpu.reg(4) == 0xFF);
    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("DEC - overflow (Rd was 0x80) sets V and S", "[dec][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(3, 0x80);
    cpu.set_sreg(0x00);
    cpu.exec_dec(encode_dec(3)); // 0x80 - 1 = 0x7F -> signed overflow

    REQUIRE(cpu.reg(3) == 0x7F);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0); // S = N XOR V -> 1
}

TEST_CASE("DEC - only destination modified, adjacent registers unchanged", "[dec][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(9,  0xAA); // decoy below
    cpu.set_reg(10, 0x05); // target
    cpu.set_reg(11, 0xBB); // decoy above

    cpu.exec_dec(encode_dec(10));

    REQUIRE(cpu.reg(10) == 0x04);
    REQUIRE(cpu.reg(9)  == 0xAA);
    REQUIRE(cpu.reg(11) == 0xBB);
}

TEST_CASE("DEC - C flag not modified", "[dec][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_C);
    cpu.set_reg(8, 0x02);
    cpu.exec_dec(encode_dec(8));

    REQUIRE((cpu.sreg() & SREG_C) != 0);
}
// ---------------------------------------------------------------------------
// SUB  (0001 10rd dddd rrrr)
//
// Operation : Rd <- Rd - Rr
// Flags     : C, Z, N, V, S (N XOR V), H
// ---------------------------------------------------------------------------

// Encode SUB Rd, Rr
static u16 encode_sub(u8 d, u8 r)
{
    return static_cast<u16>(0x1800
        | ((d & 0x1F) << 4)
        | ((r & 0x10) << 5)
        | (r & 0x0F));
}

TEST_CASE("SUB - basic subtraction stores result and preserves source", "[sub][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(2, 0x20);
    cpu.set_reg(3, 0x05);
    cpu.set_reg(4, 0x99); // decoy

    cpu.exec_sub(encode_sub(2, 3)); // 0x20 - 0x05 = 0x1B

    REQUIRE(cpu.reg(2) == 0x1B);
    REQUIRE(cpu.reg(3) == 0x05);
    REQUIRE(cpu.reg(4) == 0x99);
}

TEST_CASE("SUB - zero result sets Z flag", "[sub][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(6, 0x07);
    cpu.set_reg(7, 0x07);
    cpu.set_sreg(SREG_Z); // Z preserved by SBC when result is zero
    cpu.exec_sub(encode_sub(6, 7));

    REQUIRE(cpu.reg(6) == 0x00);
    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("SUB - borrow sets C and negative sets N", "[sub][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(8, 0x00);
    cpu.set_reg(9, 0x01);
    cpu.set_sreg(0x00);
    cpu.exec_sub(encode_sub(8, 9)); // 0x00 - 0x01 = 0xFF

    REQUIRE(cpu.reg(8) == 0xFF);
    REQUIRE((cpu.sreg() & SREG_C) != 0);
    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("SUB - signed overflow sets V and S accordingly", "[sub][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x80 - 0x01 = 0x7F => signed overflow
    cpu.set_reg(10, 0x80);
    cpu.set_reg(11, 0x01);
    cpu.set_sreg(0x00);
    cpu.exec_sub(encode_sub(10, 11));

    REQUIRE(cpu.reg(10) == 0x7F);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("SUB - H flag (borrow from bit 3) is set correctly", "[sub][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x00 - 0x08 = 0xF8 -> borrow from bit 3
    cpu.set_reg(12, 0x00);
    cpu.set_reg(13, 0x08);
    cpu.set_sreg(0x00);
    cpu.exec_sub(encode_sub(12, 13));

    REQUIRE((cpu.sreg() & SREG_H) != 0);
}

TEST_CASE("SUB - T and I flags preserved", "[sub][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T and I set
    cpu.set_reg(14, 0x10);
    cpu.set_reg(15, 0x01);
    cpu.exec_sub(encode_sub(14, 15));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
}
// ---------------------------------------------------------------------------
// SBC  (0000 10rd dddd rrrr)
//
// Operation : Rd <- Rd - Rr - C
// Flags     : C, Z, N, V, S (N XOR V), H
// ---------------------------------------------------------------------------

// Encode SBC Rd, Rr
static u16 encode_sbc(u8 d, u8 r)
{
    return static_cast<u16>(0x0800
        | ((d & 0x1F) << 4)
        | ((r & 0x10) << 5)
        | (r & 0x0F));
}

TEST_CASE("SBC - basic subtraction includes carry and preserves source", "[sbc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(2, 0x05);
    cpu.set_reg(3, 0x02);
    cpu.set_sreg(0x00); // C clear
    cpu.exec_sbc(encode_sbc(2, 3)); // 0x05 - 0x02 - 0 = 0x03

    REQUIRE(cpu.reg(2) == 0x03);
    REQUIRE(cpu.reg(3) == 0x02);

    // now with carry set: subtract one more
    cpu.set_reg(2, 0x05);
    cpu.set_sreg(SREG_C);
    cpu.exec_sbc(encode_sbc(2, 3)); // 0x05 - 0x02 - 1 = 0x02
    REQUIRE(cpu.reg(2) == 0x02);
}

TEST_CASE("SBC - zero result sets Z flag (and cleared when non-zero)", "[sbc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(6, 0x07);
    cpu.set_reg(7, 0x07);
    cpu.set_sreg(SREG_Z);
    cpu.exec_sbc(encode_sbc(6, 7)); // 0x07 - 0x07 - 0 = 0x00

    REQUIRE(cpu.reg(6) == 0x00);
    REQUIRE((cpu.sreg() & SREG_Z) != 0);

    // non-zero clears Z
    cpu.set_reg(6, 0x02);
    cpu.set_reg(7, 0x01);
    cpu.set_sreg(SREG_Z); // pre-set Z
    cpu.exec_sbc(encode_sbc(6, 7)); // 0x02 - 0x01 = 0x01
    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("SBC - borrow sets C and negative sets N", "[sbc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(8, 0x00);
    cpu.set_reg(9, 0x01);
    cpu.set_sreg(0x00);
    cpu.exec_sbc(encode_sbc(8, 9)); // 0x00 - 0x01 = 0xFF

    REQUIRE(cpu.reg(8) == 0xFF);
    REQUIRE((cpu.sreg() & SREG_C) != 0);
    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("SBC - signed overflow sets V and S accordingly", "[sbc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x80 - 0x01 = 0x7F => signed overflow
    cpu.set_reg(10, 0x80);
    cpu.set_reg(11, 0x01);
    cpu.set_sreg(0x00);
    cpu.exec_sbc(encode_sbc(10, 11));

    REQUIRE(cpu.reg(10) == 0x7F);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("SBC - H flag (borrow from bit 3) is set correctly", "[sbc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x00 - 0x08 = 0xF8 -> borrow from bit 3
    cpu.set_reg(12, 0x00);
    cpu.set_reg(13, 0x08);
    cpu.set_sreg(0x00);
    cpu.exec_sbc(encode_sbc(12, 13));

    REQUIRE((cpu.sreg() & SREG_H) != 0);
}

TEST_CASE("SBC - T and I flags preserved", "[sbc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T and I set
    cpu.set_reg(14, 0x10);
    cpu.set_reg(15, 0x01);
    cpu.exec_sbc(encode_sbc(14, 15));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
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

// ---------------------------------------------------------------------------
// AND  (0010 00rd dddd rrrr)
//
// Operation : Rd ← Rd & Rr
// Flags     : Z = (result == 0), N = result[7], V = 0, S = N XOR V = N
//             C and H are unaffected.
// ---------------------------------------------------------------------------

// Encode AND Rd, Rr opcode from register indices.
static u16 encode_and(u8 d, u8 r)
{
    return static_cast<u16>(0x2000
        | ((d & 0x1F) << 4)
        | ((r & 0x10) << 5)
        | (r & 0x0F));
}

// ---------------------------------------------------------------------------
// Result and register tests
// ---------------------------------------------------------------------------

TEST_CASE("AND - basic AND of two values", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(1, 0xAA);
    cpu.set_reg(2, 0x55);
    cpu.exec_and(encode_and(1, 2));

    REQUIRE(cpu.reg(1) == 0x00);
    REQUIRE(cpu.reg(2) == 0x55); // source register unchanged
}

TEST_CASE("AND - result stored in Rd, Rr unchanged", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(5, 0x0F);
    cpu.set_reg(6, 0xF0);
    cpu.exec_and(encode_and(5, 6));

    REQUIRE(cpu.reg(5) == 0x00);
    REQUIRE(cpu.reg(6) == 0xF0);
}

TEST_CASE("AND - self-AND preserves register (no change)", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(3, 0xAB);
    cpu.exec_and(encode_and(3, 3));

    REQUIRE(cpu.reg(3) == 0xAB);
}

TEST_CASE("AND - works with upper registers R16-R31", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0b10110011);
    cpu.set_reg(17, 0b01001100);
    cpu.exec_and(encode_and(16, 17));

    REQUIRE(cpu.reg(16) == 0b00000000);
}

// ---------------------------------------------------------------------------
// Z flag (Zero)
// ---------------------------------------------------------------------------

TEST_CASE("AND - Z set when result is zero", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x00);
    cpu.set_reg(17, 0x00);
    cpu.exec_and(encode_and(16, 17));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("AND - Z cleared when result is non-zero", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set Z
    cpu.set_reg(16, 0x01);
    cpu.set_reg(17, 0x01);
    cpu.exec_and(encode_and(16, 17));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

// ---------------------------------------------------------------------------
// N flag (Negative)
// ---------------------------------------------------------------------------

TEST_CASE("AND - N set when bit 7 of result is set", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x80);
    cpu.set_reg(17, 0xFF);
    cpu.exec_and(encode_and(16, 17)); // result = 0x80 → bit 7 set

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("AND - N cleared when bit 7 of result is clear", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set N
    cpu.set_reg(16, 0x7F);
    cpu.set_reg(17, 0xFF);
    cpu.exec_and(encode_and(16, 17)); // result = 0x7F → bit 7 clear

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

// ---------------------------------------------------------------------------
// V flag (Overflow) — always cleared by AND
// ---------------------------------------------------------------------------

TEST_CASE("AND - V always cleared", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set V
    cpu.set_reg(16, 0x80);
    cpu.set_reg(17, 0xFF);
    cpu.exec_and(encode_and(16, 17));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

// ---------------------------------------------------------------------------
// S flag (Sign = N XOR V)
// Since V is always 0 after AND, S == N.
// ---------------------------------------------------------------------------

TEST_CASE("AND - S set when N=1 (result bit 7 set)", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x80);
    cpu.set_reg(17, 0xFF);
    cpu.exec_and(encode_and(16, 17)); // N=1, V=0 → S=1

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("AND - S cleared when N=0 (result bit 7 clear)", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set S
    cpu.set_reg(16, 0x01);
    cpu.set_reg(17, 0x01);
    cpu.exec_and(encode_and(16, 17)); // N=0, V=0 → S=0

    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

// ---------------------------------------------------------------------------
// C flag — must not be modified by AND
// ---------------------------------------------------------------------------

TEST_CASE("AND - C flag not affected", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // C set before AND — must remain set
    cpu.set_sreg(SREG_C);
    cpu.set_reg(16, 0x01);
    cpu.set_reg(17, 0x01);
    cpu.exec_and(encode_and(16, 17));

    REQUIRE((cpu.sreg() & SREG_C) != 0);

    // C clear before AND — must remain clear
    cpu.set_sreg(0x00);
    cpu.set_reg(16, 0x01);
    cpu.set_reg(17, 0x01);
    cpu.exec_and(encode_and(16, 17));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

// ---------------------------------------------------------------------------
// H flag — must not be modified by AND
// ---------------------------------------------------------------------------

TEST_CASE("AND - H flag not affected", "[and][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // H set before AND — must remain set
    cpu.set_sreg(SREG_H);
    cpu.set_reg(16, 0x01);
    cpu.set_reg(17, 0x01);
    cpu.exec_and(encode_and(16, 17));

    REQUIRE((cpu.sreg() & SREG_H) != 0);

    // H clear before AND — must remain clear
    cpu.set_sreg(0x00);
    cpu.set_reg(16, 0x01);
    cpu.set_reg(17, 0x01);
    cpu.exec_and(encode_and(16, 17));

    REQUIRE((cpu.sreg() & SREG_H) == 0);
}

// ---------------------------------------------------------------------------
// OR  (0010 10rd dddd rrrr)
//
// Operation : Rd ← Rd | Rr
// Flags     : Z = (result == 0), N = result[7], V = 0, S = N
// ---------------------------------------------------------------------------

// Encode OR Rd, Rr opcode from register indices.
static u16 encode_or(u8 d, u8 r)
{
    return static_cast<u16>(0x2800
        | ((d & 0x1F) << 4)
        | ((r & 0x10) << 5)
        | (r & 0x0F));
}

TEST_CASE("OR - basic OR of two values", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(1, 0xAA);
    cpu.set_reg(2, 0x55);
    cpu.exec_or(encode_or(1, 2));

    REQUIRE(cpu.reg(1) == 0xFF);
    REQUIRE(cpu.reg(2) == 0x55); // source register unchanged
}

TEST_CASE("OR - result stored in Rd, Rr unchanged", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(5, 0x0F);
    cpu.set_reg(6, 0xF0);
    cpu.exec_or(encode_or(5, 6));

    REQUIRE(cpu.reg(5) == 0xFF);
    REQUIRE(cpu.reg(6) == 0xF0);
}

TEST_CASE("OR - self-OR preserves register (no change)", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(3, 0xAB);
    cpu.exec_or(encode_or(3, 3));

    REQUIRE(cpu.reg(3) == 0xAB);
}

TEST_CASE("OR - works with upper registers R16-R31", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0b10110011);
    cpu.set_reg(17, 0b01001100);
    cpu.exec_or(encode_or(16, 17));

    REQUIRE(cpu.reg(16) == 0b11111111);
}

TEST_CASE("OR - Z set when result is zero", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0x00);
    cpu.set_reg(1, 0x00);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("OR - Z cleared when result is non-zero", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set Z
    cpu.set_reg(0, 0xAA);
    cpu.set_reg(1, 0x55);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("OR - N set when bit 7 of result is set", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0xAA); // 0xAA | 0x55 = 0xFF  (bit 7 set)
    cpu.set_reg(1, 0x55);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("OR - N cleared when bit 7 of result is clear", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set N
    cpu.set_reg(0, 0x0F);
    cpu.set_reg(1, 0x01); // 0x0F | 0x01 = 0x0F  (bit 7 clear)
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

TEST_CASE("OR - V always cleared", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set V
    cpu.set_reg(0, 0xAA);
    cpu.set_reg(1, 0x55);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

TEST_CASE("OR - S equals N when result is negative", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0xAA); // result = 0xFF, N=1 → S=1
    cpu.set_reg(1, 0x55);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("OR - S cleared when result is positive", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set S
    cpu.set_reg(0, 0x0F); // result = 0x0F, N=0 → S=0
    cpu.set_reg(1, 0x01);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

TEST_CASE("OR - C flag not affected", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // C set before OR — must remain set
    cpu.set_sreg(SREG_C);
    cpu.set_reg(0, 0x0F);
    cpu.set_reg(1, 0x01);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_C) != 0);

    // C clear before OR — must remain clear
    cpu.set_sreg(0x00);
    cpu.set_reg(0, 0x0F);
    cpu.set_reg(1, 0x01);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

TEST_CASE("OR - H flag not affected", "[or][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // H set before OR — must remain set
    cpu.set_sreg(SREG_H);
    cpu.set_reg(0, 0x0F);
    cpu.set_reg(1, 0x01);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_H) != 0);

    // H clear before OR — must remain clear
    cpu.set_sreg(0x00);
    cpu.set_reg(0, 0x0F);
    cpu.set_reg(1, 0x01);
    cpu.exec_or(encode_or(0, 1));

    REQUIRE((cpu.sreg() & SREG_H) == 0);
}

// ---------------------------------------------------------------------------
// ADD  (0000 11rd dddd rrrr)
//
// Operation : Rd ← Rd + Rr
// Flags     : C, Z, N, V, S (N XOR V), H
// ---------------------------------------------------------------------------

// Encode ADD Rd, Rr opcode from register indices.
static u16 encode_add(u8 d, u8 r)
{
    return static_cast<u16>(0x0C00
        | ((d & 0x1F) << 4)
        | ((r & 0x10) << 5)
        | (r & 0x0F));
}

TEST_CASE("ADD - basic addition stores sum and preserves source", "[add][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(1, 0x10);
    cpu.set_reg(2, 0x20);
    cpu.exec_add(encode_add(1, 2));

    REQUIRE(cpu.reg(1) == 0x30);
    REQUIRE(cpu.reg(2) == 0x20);
}

TEST_CASE("ADD - result zero sets Z and may set C (overflow)", "[add][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(10, 0xFF);
    cpu.set_reg(11, 0x01);
    cpu.exec_add(encode_add(10, 11)); // 0xFF + 0x01 = 0x00

    REQUIRE(cpu.reg(10) == 0x00);
    REQUIRE((cpu.sreg() & SREG_Z) != 0);
    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("ADD - half carry H set on carry from bit 3", "[add][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(6, 0x08);
    cpu.set_reg(7, 0x08);
    cpu.exec_add(encode_add(6, 7)); // 0x08 + 0x08 = 0x10 -> H set

    REQUIRE((cpu.sreg() & SREG_H) != 0);
}

TEST_CASE("ADD - V (overflow) set on signed overflow (positive+positive->negative)", "[add][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(4, 0x7F);
    cpu.set_reg(5, 0x01);
    cpu.exec_add(encode_add(4, 5)); // 0x7F + 0x01 = 0x80 -> V set

    REQUIRE((cpu.sreg() & SREG_V) != 0);
}

TEST_CASE("ADD - N set when result bit 7 is set", "[add][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(8, 0x40);
    cpu.set_reg(9, 0x40);
    cpu.exec_add(encode_add(8, 9)); // 0x40 + 0x40 = 0x80 -> N set

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("ADD - S = N XOR V behaviour", "[add][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // positive + positive -> negative: V=1, N=1 -> S=0
    cpu.set_reg(12, 0x7F);
    cpu.set_reg(13, 0x01);
    cpu.exec_add(encode_add(12, 13)); // result 0x80

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE(((cpu.sreg() & SREG_S) == 0));
}

TEST_CASE("ADD - T and I flags preserved", "[add][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T and I set
    cpu.set_reg(2, 0x01);
    cpu.set_reg(3, 0x02);
    cpu.exec_add(encode_add(2, 3));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
}

TEST_CASE("ADD - works with R31 (highest register)", "[add][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(31, 0x10);
    cpu.set_reg(30, 0x20);
    cpu.exec_add(encode_add(31, 30));

    REQUIRE(cpu.reg(31) == 0x30);
    REQUIRE(cpu.reg(30) == 0x20);
}

// ---------------------------------------------------------------------------
// ADC  (0001 11rd dddd rrrr)
//
// Operation : Rd ← Rd + Rr + C
// Flags     : C, Z, N, V, S (N XOR V), H
// ---------------------------------------------------------------------------

// Encode ADC Rd, Rr opcode from register indices.
static u16 encode_adc(u8 d, u8 r)
{
    return static_cast<u16>(0x1C00
        | ((d & 0x1F) << 4)
        | ((r & 0x10) << 5)
        | (r & 0x0F));
}

TEST_CASE("ADC - basic addition with carry-in", "[adc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(1, 0x10);
    cpu.set_reg(2, 0x20);
    cpu.set_sreg(SREG_C); // carry-in = 1
    cpu.exec_adc(encode_adc(1, 2));

    REQUIRE(cpu.reg(1) == 0x31);
    REQUIRE(cpu.reg(2) == 0x20);
}

TEST_CASE("ADC - zero result with carry produces Z and C", "[adc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(10, 0xFF);
    cpu.set_reg(11, 0x00);
    cpu.set_sreg(SREG_C); // +1 -> 0x100
    cpu.exec_adc(encode_adc(10, 11));

    REQUIRE(cpu.reg(10) == 0x00);
    REQUIRE((cpu.sreg() & SREG_Z) != 0);
    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("ADC - half carry H set on carry from bit 3", "[adc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(6, 0x08);
    cpu.set_reg(7, 0x07);
    cpu.set_sreg(SREG_C); // 0x08 + 0x07 + 1 = 0x10 -> half carry from bit3
    cpu.exec_adc(encode_adc(6, 7));

    REQUIRE((cpu.sreg() & SREG_H) != 0);
}

TEST_CASE("ADC - V (overflow) set on signed overflow (positive+positive+carry->negative)", "[adc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(4, 0x7F);
    cpu.set_reg(5, 0x00);
    cpu.set_sreg(SREG_C); // +1 -> 0x80
    cpu.exec_adc(encode_adc(4, 5));

    REQUIRE((cpu.sreg() & SREG_V) != 0);
}

TEST_CASE("ADC - N set when result bit 7 is set", "[adc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(8, 0x40);
    cpu.set_reg(9, 0x40);
    cpu.set_sreg(0x00);
    cpu.exec_adc(encode_adc(8, 9)); // 0x40 + 0x40 + 0 = 0x80 -> N set

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("ADC - S = N XOR V behavior", "[adc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x7F + 0x00 + 1 = 0x80 -> N=1, V=1 -> S=0
    cpu.set_reg(12, 0x7F);
    cpu.set_reg(13, 0x00);
    cpu.set_sreg(SREG_C);
    cpu.exec_adc(encode_adc(12, 13));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE(((cpu.sreg() & SREG_S) == 0));
}

TEST_CASE("ADC - T and I flags preserved", "[adc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T and I set
    cpu.set_reg(2, 0x01);
    cpu.set_reg(3, 0x02);
    cpu.exec_adc(encode_adc(2, 3));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
}

TEST_CASE("ADC - works with R31 (highest register)", "[adc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(31, 0x10);
    cpu.set_reg(30, 0x20);
    cpu.set_sreg(SREG_C);
    cpu.exec_adc(encode_adc(31, 30));

    REQUIRE(cpu.reg(31) == 0x31);
    REQUIRE(cpu.reg(30) == 0x20);
}

// ---------------------------------------------------------------------------
// CPI  (0011 KKKK dddd KKKK)
//
// Operation : Rd - K  (result discarded, only flags updated)
// Operands  : Rd ∈ R16–R31, K ∈ 0–255
// Flags     : C, Z, N, V, S (N XOR V), H
// ---------------------------------------------------------------------------

// Encode CPI Rd, K opcode.
static u16 encode_cpi(u8 reg_d, u8 k)
{
    u8 d = reg_d - 16;  // 4-bit offset stored in bits 7:4
    return static_cast<u16>(0x3000
        | ((k & 0xF0) << 4)   // K[7:4] → bits 11:8
        | ((d & 0x0F) << 4)   // d[3:0] → bits  7:4
        | (k & 0x0F));        // K[3:0] → bits  3:0
}

// ---------------------------------------------------------------------------
// Register preservation — CPI must NOT write back to Rd
// ---------------------------------------------------------------------------

TEST_CASE("CPI - register value not modified", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(20, 0x55);
    cpu.exec_cpi(encode_cpi(20, 0x10));

    REQUIRE(cpu.reg(20) == 0x55);
}

// ---------------------------------------------------------------------------
// Z flag (Zero)
// ---------------------------------------------------------------------------

TEST_CASE("CPI - Z set when Rd equals K", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x42);
    cpu.exec_cpi(encode_cpi(16, 0x42));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("CPI - Z cleared when Rd does not equal K", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set Z
    cpu.set_reg(16, 0x42);
    cpu.exec_cpi(encode_cpi(16, 0x41));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

// ---------------------------------------------------------------------------
// C flag (Carry / Borrow)
// ---------------------------------------------------------------------------

TEST_CASE("CPI - C set when Rd < K (borrow)", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x05);
    cpu.exec_cpi(encode_cpi(16, 0x10)); // 5 < 16 → borrow

    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("CPI - C cleared when Rd >= K", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set C
    cpu.set_reg(16, 0x10);
    cpu.exec_cpi(encode_cpi(16, 0x05)); // 16 >= 5 → no borrow

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

TEST_CASE("CPI - C cleared when Rd equals K", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set C
    cpu.set_reg(16, 0x20);
    cpu.exec_cpi(encode_cpi(16, 0x20));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

// ---------------------------------------------------------------------------
// N flag (Negative)
// ---------------------------------------------------------------------------

TEST_CASE("CPI - N set when result bit 7 is set", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x00);
    cpu.exec_cpi(encode_cpi(16, 0x01)); // 0x00 - 0x01 = 0xFF → bit 7 set

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("CPI - N cleared when result bit 7 is clear", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set N
    cpu.set_reg(16, 0x10);
    cpu.exec_cpi(encode_cpi(16, 0x05)); // 0x10 - 0x05 = 0x0B → bit 7 clear

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

// ---------------------------------------------------------------------------
// V flag (Overflow)
// ---------------------------------------------------------------------------

TEST_CASE("CPI - V set on signed overflow (positive minus large unsigned overflows to negative)", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x7F (127) - 0x80 (128) = 0xFF: d7=0, k7=1, r7=1 → overflow
    cpu.set_reg(16, 0x7F);
    cpu.exec_cpi(encode_cpi(16, 0x80));

    REQUIRE((cpu.sreg() & SREG_V) != 0);
}

TEST_CASE("CPI - V set on signed overflow (negative minus positive overflows to positive)", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x80 (-128) - 0x01 (1) = 0x7F (127): d7=1, k7=0, r7=0 → overflow
    cpu.set_reg(16, 0x80);
    cpu.exec_cpi(encode_cpi(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_V) != 0);
}

TEST_CASE("CPI - V cleared when no signed overflow", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set V
    cpu.set_reg(16, 0x40);
    cpu.exec_cpi(encode_cpi(16, 0x01)); // 0x40 - 0x01 = 0x3F, d7=0, k7=0 → no overflow

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

// ---------------------------------------------------------------------------
// S flag (Sign = N XOR V)
// ---------------------------------------------------------------------------

TEST_CASE("CPI - S clear when N=0 and V=0", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x0F - 0x01 = 0x0E: N=0, V=0 → S=0
    cpu.set_reg(16, 0x0F);
    cpu.exec_cpi(encode_cpi(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

TEST_CASE("CPI - S set when N=1 and V=0", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x80 - 0x81 = 0xFF: d7=1, k7=1, r7=1 → V=0, N=1 → S=1
    cpu.set_reg(16, 0x80);
    cpu.exec_cpi(encode_cpi(16, 0x81));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_V) == 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("CPI - S set when N=0 and V=1", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x80 - 0x01 = 0x7F: d7=1, k7=0, r7=0 → V=1, N=0 → S=1
    cpu.set_reg(16, 0x80);
    cpu.exec_cpi(encode_cpi(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_N) == 0);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("CPI - S clear when N=1 and V=1", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x7F - 0x80 = 0xFF: d7=0, k7=1, r7=1 → V=1, N=1 → S=0
    cpu.set_reg(16, 0x7F);
    cpu.exec_cpi(encode_cpi(16, 0x80));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

// ---------------------------------------------------------------------------
// H flag (Half Carry / Borrow from bit 3)
// ---------------------------------------------------------------------------

TEST_CASE("CPI - H set when borrow from bit 3", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x00 - 0x08: d3=0, k3=1 → borrow at bit 3 → H=1
    cpu.set_reg(16, 0x00);
    cpu.exec_cpi(encode_cpi(16, 0x08));

    REQUIRE((cpu.sreg() & SREG_H) != 0);
}

TEST_CASE("CPI - H cleared when no borrow from bit 3", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set H
    // 0x08 - 0x01 = 0x07: d3=1, k3=0, r3=0 → no borrow at bit 3 → H=0
    cpu.set_reg(16, 0x08);
    cpu.exec_cpi(encode_cpi(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_H) == 0);
}

// ---------------------------------------------------------------------------
// Register range — CPI operates on R16–R31
// ---------------------------------------------------------------------------

TEST_CASE("CPI - works with R16 (lowest CPI register)", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x10);
    cpu.exec_cpi(encode_cpi(16, 0x10));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
    REQUIRE(cpu.reg(16) == 0x10); // not modified
}

TEST_CASE("CPI - works with R31 (highest CPI register)", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(31, 0xAB);
    cpu.exec_cpi(encode_cpi(31, 0xAB));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
    REQUIRE(cpu.reg(31) == 0xAB); // not modified
}

// ---------------------------------------------------------------------------
// T and I flags must not be modified
// ---------------------------------------------------------------------------

TEST_CASE("CPI - T and I flags preserved", "[cpi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T (bit 6) and I (bit 7) set
    cpu.set_reg(16, 0x42);
    cpu.exec_cpi(encode_cpi(16, 0x10));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
}

// ---------------------------------------------------------------------------
// CPC  (0000 01rd dddd rrrr)
//
// Operation : Rd - Rr - C  (result discarded, only flags updated)
// Operands  : Rd, Rr ∈ R0–R31
// Flags     : C, Z (only CLEARED, never set), N, V, S (N XOR V), H
// ---------------------------------------------------------------------------

// Encode CPC Rd, Rr opcode from register indices.
static u16 encode_cpc(u8 d, u8 r)
{
    return static_cast<u16>(0x0400
        | ((d & 0x1F) << 4)
        | ((r & 0x10) << 5)
        | (r & 0x0F));
}

// ---------------------------------------------------------------------------
// Register preservation — CPC must NOT write back to Rd or Rr
// ---------------------------------------------------------------------------

TEST_CASE("CPC - Rd and Rr not modified", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(4, 0xAB);
    cpu.set_reg(5, 0x01);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE(cpu.reg(4) == 0xAB);
    REQUIRE(cpu.reg(5) == 0x01);
}

// ---------------------------------------------------------------------------
// Z flag — only CLEARED on non-zero result; preserved (not set) on zero
// ---------------------------------------------------------------------------

TEST_CASE("CPC - Z preserved when result is zero and Z was set", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x05 - 0x05 - C(0) = 0 → Z unchanged (was 1, stays 1)
    cpu.set_sreg(SREG_Z);
    cpu.set_reg(4, 0x05);
    cpu.set_reg(5, 0x05);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("CPC - Z not set when result is zero and Z was clear", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x05 - 0x05 - C(0) = 0 → Z unchanged (was 0, stays 0)
    cpu.set_sreg(0x00);
    cpu.set_reg(4, 0x05);
    cpu.set_reg(5, 0x05);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("CPC - Z cleared when result is non-zero", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_Z); // pre-set Z
    cpu.set_reg(4, 0x10);
    cpu.set_reg(5, 0x01);
    cpu.exec_cpc(encode_cpc(4, 5)); // 0x10 - 0x01 - 0 = 0x0F ≠ 0

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("CPC - Z cleared when carry-in causes non-zero result", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x06 - 0x05 - C(1) = 0 would equal zero...
    // but 0x05 - 0x05 - C(1) = 0xFF ≠ 0 → Z cleared
    cpu.set_sreg(SREG_Z | SREG_C);
    cpu.set_reg(4, 0x05);
    cpu.set_reg(5, 0x05);
    cpu.exec_cpc(encode_cpc(4, 5)); // 0x05 - 0x05 - 1 = 0xFF

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("CPC - Z preserved across back-to-back zero results (16-bit use case)", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // Simulate 16-bit compare: low bytes equal (CP sets Z), then high bytes equal (CPC must keep Z set)
    cpu.set_sreg(SREG_Z); // Z set by preceding CP
    cpu.set_reg(2, 0xAB);
    cpu.set_reg(3, 0xAB);
    cpu.exec_cpc(encode_cpc(2, 3)); // 0xAB - 0xAB - 0 = 0 → Z stays set

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

// ---------------------------------------------------------------------------
// C flag (Carry / Borrow)
// ---------------------------------------------------------------------------

TEST_CASE("CPC - C set when Rd < Rr (borrow, no carry-in)", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(4, 0x05);
    cpu.set_reg(5, 0x10);
    cpu.exec_cpc(encode_cpc(4, 5)); // 5 < 16 → borrow

    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("CPC - C cleared when Rd > Rr (no borrow, no carry-in)", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set C
    cpu.set_reg(4, 0x10);
    cpu.set_reg(5, 0x05);
    cpu.exec_cpc(encode_cpc(4, 5)); // 16 > 5 → no borrow

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

TEST_CASE("CPC - C set when carry-in causes borrow", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // Rd == Rr but carry-in = 1 → 0x05 - 0x05 - 1 = 0xFF → borrow
    cpu.set_sreg(SREG_C);
    cpu.set_reg(4, 0x05);
    cpu.set_reg(5, 0x05);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("CPC - C cleared when carry-in covered by Rd > Rr", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x08 - 0x05 - 1 = 0x02 → no borrow
    cpu.set_sreg(SREG_C);
    cpu.set_reg(4, 0x08);
    cpu.set_reg(5, 0x05);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

// ---------------------------------------------------------------------------
// N flag (Negative)
// ---------------------------------------------------------------------------

TEST_CASE("CPC - N set when result bit 7 is set", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x00 - 0x01 - 0 = 0xFF → bit 7 set
    cpu.set_reg(4, 0x00);
    cpu.set_reg(5, 0x01);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("CPC - N cleared when result bit 7 is clear", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set N
    // 0x10 - 0x05 - 0 = 0x0B → bit 7 clear
    cpu.set_reg(4, 0x10);
    cpu.set_reg(5, 0x05);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

// ---------------------------------------------------------------------------
// V flag (Overflow)
// ---------------------------------------------------------------------------

TEST_CASE("CPC - V set on signed overflow (positive minus negative overflows to negative)", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x7F - 0x80 = 0xFF: d7=0, r7=1, result7=1 → overflow
    cpu.set_reg(4, 0x7F);
    cpu.set_reg(5, 0x80);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_V) != 0);
}

// ---------------------------------------------------------------------------
// ORI  (0110 KKKK dddd KKKK)
//
// Operation : Rd ← Rd | K
// Operands  : Rd ∈ R16–R31, K ∈ 0–255
// Flags     : Z = (result == 0), N = result[7], V = 0, S = N XOR V = N
//             C and H are unaffected.
// ---------------------------------------------------------------------------

// Encode ORI Rd, K opcode.
static u16 encode_ori(u8 d, u8 k)
{
    u8 d_off = d - 16; // 4-bit offset into R16–R31
    return static_cast<u16>(0x6000
        | ((k & 0xF0) << 4)       // K[7:4] → bits [11:8]
        | ((d_off & 0x0F) << 4)   // d offset → bits [7:4]
        | (k & 0x0F));            // K[3:0] → bits [3:0]
}

// ---------------------------------------------------------------------------
// Result and register tests
// ---------------------------------------------------------------------------

TEST_CASE("ORI - result is OR of Rd and K", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xA0);
    cpu.exec_ori(encode_ori(16, 0x0F)); // 0xA0 | 0x0F = 0xAF

    REQUIRE(cpu.reg(16) == 0xAF);
}

TEST_CASE("ORI - OR with 0x00 leaves Rd unchanged", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(20, 0x5A);
    cpu.exec_ori(encode_ori(20, 0x00));

    REQUIRE(cpu.reg(20) == 0x5A);
}

TEST_CASE("ORI - OR with 0xFF sets Rd to 0xFF", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(18, 0x00);
    cpu.exec_ori(encode_ori(18, 0xFF));

    REQUIRE(cpu.reg(18) == 0xFF);
}

TEST_CASE("ORI - individual bits are set correctly", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0b10100000);
    cpu.exec_ori(encode_ori(16, 0b01000101)); // 0b10100000 | 0b01000101 = 0b11100101

    REQUIRE(cpu.reg(16) == 0b11100101);
}

// ---------------------------------------------------------------------------
// Destination register selection
// ---------------------------------------------------------------------------

TEST_CASE("ORI - correct destination register selected (R16)", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x10);
    cpu.set_reg(17, 0x22); // decoy
    cpu.exec_ori(encode_ori(16, 0x01));

    REQUIRE(cpu.reg(16) == 0x11);
    REQUIRE(cpu.reg(17) == 0x22); // adjacent unchanged
}

TEST_CASE("ORI - correct destination register selected (R31)", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(31, 0x40);
    cpu.set_reg(30, 0xFF); // decoy
    cpu.exec_ori(encode_ori(31, 0x08));

    REQUIRE(cpu.reg(31) == 0x48);
    REQUIRE(cpu.reg(30) == 0xFF); // adjacent unchanged
}

TEST_CASE("ORI - correct destination register selected (R24, mid-range)", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(24, 0x01);
    cpu.exec_ori(encode_ori(24, 0x80));

    REQUIRE(cpu.reg(24) == 0x81);
}

// ---------------------------------------------------------------------------
// Immediate decoding
// ---------------------------------------------------------------------------

TEST_CASE("ORI - K high nibble decoded correctly", "[ori][alu]")
{
    // K = 0xA0: only high nibble set
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0xA0));

    REQUIRE(cpu.reg(16) == 0xA0);
}

TEST_CASE("ORI - K low nibble decoded correctly", "[ori][alu]")
{
    // K = 0x0B: only low nibble set
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x0B));

    REQUIRE(cpu.reg(16) == 0x0B);
}

// ---------------------------------------------------------------------------
// Z flag (Zero)
// ---------------------------------------------------------------------------

TEST_CASE("ORI - Z set when both Rd and K are zero", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x00));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("ORI - Z cleared when result is non-zero", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set Z
    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

// ---------------------------------------------------------------------------
// N flag (Negative)
// ---------------------------------------------------------------------------

TEST_CASE("ORI - N set when bit 7 of result is set", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x80)); // result = 0x80 → bit 7 set

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("ORI - N cleared when bit 7 of result is clear", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set N
    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x7F)); // result = 0x7F → bit 7 clear

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

// ---------------------------------------------------------------------------
// V flag (Overflow) — always cleared by ORI
// ---------------------------------------------------------------------------

TEST_CASE("ORI - V always cleared", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set V
    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x80));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

// ---------------------------------------------------------------------------
// S flag (Sign = N XOR V)
// Since V is always 0 after ORI, S == N.
// ---------------------------------------------------------------------------

TEST_CASE("ORI - S set when N=1 (result bit 7 set)", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x80)); // N=1, V=0 → S=1

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("ORI - S cleared when N=0 (result bit 7 clear)", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set S
    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x01)); // N=0, V=0 → S=0

    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

// ---------------------------------------------------------------------------
// C flag — must not be modified by ORI
// ---------------------------------------------------------------------------

TEST_CASE("ORI - C flag not affected", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // C set before ORI — must remain set
    cpu.set_sreg(SREG_C);
    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_C) != 0);

    // C clear before ORI — must remain clear
    cpu.set_sreg(0x00);
    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

// ---------------------------------------------------------------------------
// H flag — must not be modified by ORI
// ---------------------------------------------------------------------------

TEST_CASE("ORI - H flag not affected", "[ori][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // H set before ORI — must remain set
    cpu.set_sreg(SREG_H);
    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_H) != 0);

    // H clear before ORI — must remain clear
    cpu.set_sreg(0x00);
    cpu.set_reg(16, 0x00);
    cpu.exec_ori(encode_ori(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_H) == 0);
}

TEST_CASE("CPC - V set on signed overflow (negative minus positive overflows to positive)", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x80 - 0x01 = 0x7F: d7=1, r7=0, result7=0 → overflow
    cpu.set_reg(4, 0x80);
    cpu.set_reg(5, 0x01);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_V) != 0);
}

TEST_CASE("CPC - V cleared when no signed overflow", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set V
    // 0x40 - 0x01 = 0x3F: d7=0, r7=0 → no overflow
    cpu.set_reg(4, 0x40);
    cpu.set_reg(5, 0x01);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

// ---------------------------------------------------------------------------
// S flag (Sign = N XOR V)
// ---------------------------------------------------------------------------

TEST_CASE("CPC - S clear when N=0 and V=0", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x0F - 0x01 = 0x0E: N=0, V=0 → S=0
    cpu.set_reg(4, 0x0F);
    cpu.set_reg(5, 0x01);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

TEST_CASE("CPC - S set when N=1 and V=0", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x80 - 0x81 = 0xFF: d7=1, r7=1, result7=1 → V=0, N=1 → S=1
    cpu.set_reg(4, 0x80);
    cpu.set_reg(5, 0x81);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_V) == 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("CPC - S set when N=0 and V=1", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x80 - 0x01 = 0x7F: d7=1, r7=0, result7=0 → V=1, N=0 → S=1
    cpu.set_reg(4, 0x80);
    cpu.set_reg(5, 0x01);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_N) == 0);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("CPC - S clear when N=1 and V=1", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x7F - 0x80 = 0xFF: d7=0, r7=1, result7=1 → V=1, N=1 → S=0
    cpu.set_reg(4, 0x7F);
    cpu.set_reg(5, 0x80);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

// ---------------------------------------------------------------------------
// H flag (Half Carry / Borrow from bit 3)
// ---------------------------------------------------------------------------

TEST_CASE("CPC - H set when borrow from bit 3", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x00 - 0x08 - 0: d3=0, r3=1 → borrow at bit 3 → H=1
    cpu.set_reg(4, 0x00);
    cpu.set_reg(5, 0x08);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_H) != 0);
}

TEST_CASE("CPC - H cleared when no borrow from bit 3", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // pre-set H
    // 0x08 - 0x01 - 0 = 0x07: d3=1, r3=0, result3=1 → no borrow at bit 3 → H=0
    cpu.set_reg(4, 0x08);
    cpu.set_reg(5, 0x01);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_H) == 0);
}

TEST_CASE("CPC - H set when carry-in causes borrow from bit 3", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x08 - 0x08 - C(1) = 0xFF: d3=1, r3=1, result3=1 → H=1 (borrow due to carry-in)
    cpu.set_sreg(SREG_C);
    cpu.set_reg(4, 0x08);
    cpu.set_reg(5, 0x08);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & SREG_H) != 0);
}

// ---------------------------------------------------------------------------
// T and I flags must not be modified
// ---------------------------------------------------------------------------

TEST_CASE("CPC - T and I flags preserved", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T (bit 6) and I (bit 7) set
    cpu.set_reg(4, 0x42);
    cpu.set_reg(5, 0x10);
    cpu.exec_cpc(encode_cpc(4, 5));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
}

// ---------------------------------------------------------------------------
// Register range — CPC operates on R0–R31
// ---------------------------------------------------------------------------

TEST_CASE("CPC - works with R0 and R1", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0x10);
    cpu.set_reg(1, 0x10);
    cpu.set_sreg(SREG_Z); // Z set going in
    cpu.exec_cpc(encode_cpc(0, 1)); // 0x10 - 0x10 - 0 = 0 → Z preserved

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
    REQUIRE(cpu.reg(0) == 0x10);
    REQUIRE(cpu.reg(1) == 0x10);
}

TEST_CASE("CPC - works with R30 and R31", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(30, 0x05);
    cpu.set_reg(31, 0x10);
    cpu.exec_cpc(encode_cpc(30, 31)); // 5 < 16 → borrow → C=1

    REQUIRE((cpu.sreg() & SREG_C) != 0);
    REQUIRE(cpu.reg(30) == 0x05);
    REQUIRE(cpu.reg(31) == 0x10);
}

TEST_CASE("CPC - Rd == Rr (same register, no carry-in)", "[cpc][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // Comparing a register with itself: result always 0, Z must be preserved if set
    cpu.set_sreg(SREG_Z);
    cpu.set_reg(7, 0xBE);
    cpu.exec_cpc(encode_cpc(7, 7));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

// ---------------------------------------------------------------------------
// SUBI (0101 KKKK dddd KKKK)
//
// Operation : Rd ← Rd - K
// Operands  : Rd ∈ R16–R31, K ∈ 0–255
// Flags     : C, Z, N, V, S (N XOR V), H
// ---------------------------------------------------------------------------

// Encode SUBI Rd, K
static u16 encode_subi(u8 d, u8 k)
{
    u8 d_off = d - 16;
    return static_cast<u16>(0x5000
        | ((k & 0xF0) << 4)
        | ((d_off & 0x0F) << 4)
        | (k & 0x0F));
}

TEST_CASE("SUBI - basic subtraction stores result and preserves other regs", "[subi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(16, 0x20);
    cpu.set_reg(17, 0x99); // decoy

    cpu.exec_subi(encode_subi(16, 0x05)); // 0x20 - 0x05 = 0x1B

    REQUIRE(cpu.reg(16) == 0x1B);
    REQUIRE(cpu.reg(17) == 0x99);
}

TEST_CASE("SUBI - zero result sets Z flag", "[subi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(18, 0x05);
    cpu.set_sreg(0x00);

    cpu.exec_subi(encode_subi(18, 0x05));

    REQUIRE(cpu.reg(18) == 0x00);
    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("SUBI - borrow sets C and negative sets N", "[subi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(19, 0x00);
    cpu.set_sreg(0x00);

    cpu.exec_subi(encode_subi(19, 0x01)); // 0x00 - 0x01 = 0xFF

    REQUIRE(cpu.reg(19) == 0xFF);
    REQUIRE((cpu.sreg() & SREG_C) != 0);
    REQUIRE((cpu.sreg() & SREG_N) != 0);
    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("SUBI - signed overflow sets V and S accordingly", "[subi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x80 - 0x01 = 0x7F -> signed overflow (negative - positive -> positive)
    cpu.set_reg(20, 0x80);
    cpu.set_sreg(0x00);

    cpu.exec_subi(encode_subi(20, 0x01));

    REQUIRE(cpu.reg(20) == 0x7F);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE((cpu.sreg() & SREG_N) == 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0); // S = N XOR V -> 1
}

// ---------------------------------------------------------------------------
// SBCI (0100 KKKK dddd KKKK)
//
// Operation : Rd ← Rd - K - C
// Operands  : Rd ∈ R16–R31, K ∈ 0–255
// Flags     : C, Z (cleared if result non-zero, otherwise preserved), N, V, S (N XOR V), H
// ---------------------------------------------------------------------------

// Encode SBCI Rd, K
static u16 encode_sbci(u8 d, u8 k)
{
    u8 d_off = d - 16;
    return static_cast<u16>(0x4000
        | ((k & 0xF0) << 4)
        | ((d_off & 0x0F) << 4)
        | (k & 0x0F));
}

TEST_CASE("SBCI - basic subtraction (carry-in = 0) stores result and preserves other regs", "[sbci][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(16, 0x20);
    cpu.set_reg(17, 0x99); // decoy
    cpu.set_sreg(0x00);   // carry-in = 0

    cpu.exec_sbci(encode_sbci(16, 0x05)); // 0x20 - 0x05 - 0 = 0x1B

    REQUIRE(cpu.reg(16) == 0x1B);
    REQUIRE(cpu.reg(17) == 0x99);
}

TEST_CASE("SBCI - Z preserved when result is zero", "[sbci][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // Case: result == 0 and Z was set -> Z should remain set
    cpu.set_reg(18, 0x05);
    cpu.set_sreg(SREG_Z);
    cpu.exec_sbci(encode_sbci(18, 0x05)); // 0x05 - 0x05 - 0 = 0
    REQUIRE((cpu.sreg() & SREG_Z) != 0);

    // Case: result == 0 and Z was clear -> Z should remain clear
    cpu.set_reg(19, 0x07);
    cpu.set_sreg(0x00);
    cpu.exec_sbci(encode_sbci(19, 0x07)); // 0x07 - 0x07 - 0 = 0
    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("SBCI - carry-in affects result and produces borrow (C) and negative (N)", "[sbci][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(20, 0x00);
    cpu.set_sreg(SREG_C); // carry-in = 1

    cpu.exec_sbci(encode_sbci(20, 0x00)); // 0x00 - 0x00 - 1 = 0xFF

    REQUIRE(cpu.reg(20) == 0xFF);
    REQUIRE((cpu.sreg() & SREG_C) != 0);
    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("SBCI - non-zero result clears Z", "[sbci][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(21, 0x10);
    cpu.set_sreg(SREG_Z); // pre-set Z

    cpu.exec_sbci(encode_sbci(21, 0x01)); // 0x10 - 0x01 - 0 = 0x0F (non-zero)

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("SBCI - signed overflow sets V and S accordingly", "[sbci][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x80 - 0x00 - 1 = 0x7F -> signed overflow
    cpu.set_reg(22, 0x80);
    cpu.set_sreg(SREG_C); // carry-in = 1

    cpu.exec_sbci(encode_sbci(22, 0x00));

    REQUIRE(cpu.reg(22) == 0x7F);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
    REQUIRE((cpu.sreg() & SREG_N) == 0);
    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

// ---------------------------------------------------------------------------
// CP  (0001 01rd dddd rrrr)
//
// Operation : Rd - Rr  (result discarded)
// Flags     : C, Z, N, V, S (N XOR V), H
// ---------------------------------------------------------------------------

static u16 encode_cp(u8 d, u8 r)
{
    return static_cast<u16>(0x1400
        | ((d & 0x1F) << 4)
        | ((r & 0x10) << 5)
        | (r & 0x0F));
}

TEST_CASE("CP - registers Rd and Rr are not modified", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(2, 0x10);
    cpu.set_reg(3, 0x05);
    cpu.exec_cp(encode_cp(2, 3));

    REQUIRE(cpu.reg(2) == 0x10); // Rd unchanged
    REQUIRE(cpu.reg(3) == 0x05); // Rr unchanged
}

TEST_CASE("CP - Z flag set when Rd equals Rr", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(4, 0xAB);
    cpu.set_reg(5, 0xAB);
    cpu.set_sreg(0x00);
    cpu.exec_cp(encode_cp(4, 5));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("CP - Z flag cleared when Rd differs from Rr", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(4, 0x10);
    cpu.set_reg(5, 0x01);
    cpu.set_sreg(SREG_Z); // pre-set Z
    cpu.exec_cp(encode_cp(4, 5));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("CP - C flag set when borrow (Rd < Rr unsigned)", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(6, 0x00);
    cpu.set_reg(7, 0x01); // 0x00 - 0x01 borrows
    cpu.set_sreg(0x00);
    cpu.exec_cp(encode_cp(6, 7));

    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("CP - C flag cleared when no borrow (Rd >= Rr unsigned)", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(6, 0x05);
    cpu.set_reg(7, 0x03);
    cpu.set_sreg(SREG_C); // pre-set C
    cpu.exec_cp(encode_cp(6, 7));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

TEST_CASE("CP - N flag set when result bit 7 is set", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x00 - 0x01 = 0xFF (bit 7 set)
    cpu.set_reg(8, 0x00);
    cpu.set_reg(9, 0x01);
    cpu.set_sreg(0x00);
    cpu.exec_cp(encode_cp(8, 9));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("CP - N flag cleared when result bit 7 is clear", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x05 - 0x03 = 0x02 (bit 7 clear)
    cpu.set_reg(8, 0x05);
    cpu.set_reg(9, 0x03);
    cpu.set_sreg(SREG_N); // pre-set N
    cpu.exec_cp(encode_cp(8, 9));

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

TEST_CASE("CP - V flag set on signed overflow (0x80 - 0x01)", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x80 - 0x01 = 0x7F: negative minus positive gives positive -> overflow
    cpu.set_reg(10, 0x80);
    cpu.set_reg(11, 0x01);
    cpu.set_sreg(0x00);
    cpu.exec_cp(encode_cp(10, 11));

    REQUIRE((cpu.sreg() & SREG_V) != 0);
}

TEST_CASE("CP - V flag cleared when no signed overflow", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x05 - 0x03 = 0x02: no overflow
    cpu.set_reg(10, 0x05);
    cpu.set_reg(11, 0x03);
    cpu.set_sreg(SREG_V); // pre-set V
    cpu.exec_cp(encode_cp(10, 11));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

TEST_CASE("CP - S equals N XOR V", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x80 - 0x01 = 0x7F: N=0, V=1 -> S=1
    cpu.set_reg(12, 0x80);
    cpu.set_reg(13, 0x01);
    cpu.set_sreg(0x00);
    cpu.exec_cp(encode_cp(12, 13));

    u8 N = (cpu.sreg() & SREG_N) != 0 ? 1 : 0;
    u8 V = (cpu.sreg() & SREG_V) != 0 ? 1 : 0;
    u8 S = (cpu.sreg() & SREG_S) != 0 ? 1 : 0;
    REQUIRE(S == (N ^ V));
}

TEST_CASE("CP - H flag set when borrow from bit 3", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x00 - 0x08 = 0xF8: borrow from bit 3 into bit 4
    cpu.set_reg(14, 0x00);
    cpu.set_reg(15, 0x08);
    cpu.set_sreg(0x00);
    cpu.exec_cp(encode_cp(14, 15));

    REQUIRE((cpu.sreg() & SREG_H) != 0);
}

TEST_CASE("CP - H flag cleared when no borrow from bit 3", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    // 0x08 - 0x01 = 0x07: rd3=1, rr3=0, r3=0 -> H formula evaluates to 0
    cpu.set_reg(14, 0x08);
    cpu.set_reg(15, 0x01);
    cpu.set_sreg(SREG_H); // pre-set H
    cpu.exec_cp(encode_cp(14, 15));

    REQUIRE((cpu.sreg() & SREG_H) == 0);
}

TEST_CASE("CP - self compare (Rd == Rr) sets only Z", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_reg(16, 0x42);
    cpu.set_sreg(0x00);
    cpu.exec_cp(encode_cp(16, 16));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
    REQUIRE((cpu.sreg() & SREG_C) == 0);
    REQUIRE((cpu.sreg() & SREG_N) == 0);
    REQUIRE((cpu.sreg() & SREG_V) == 0);
    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

TEST_CASE("CP - T and I flags preserved", "[cp][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T and I set
    cpu.set_reg(18, 0x10);
    cpu.set_reg(19, 0x01);
    cpu.exec_cp(encode_cp(18, 19));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
}

// ---------------------------------------------------------------------------
// COM  (1001 010d dddd 0000)
//
// Operation : Rd ← 0xFF - Rd  (one's complement)
// Flags     : C always set, Z = (result == 0), N = result[7], V cleared,
//             S = N XOR V = N
// ---------------------------------------------------------------------------

static u16 encode_com(u8 d)
{
    // 1001 010d dddd 0000
    return static_cast<u16>(0x9400 | ((d & 0x1F) << 4));
}

TEST_CASE("COM - result is one's complement of Rd", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(3, 0xAA);
    cpu.exec_com(encode_com(3));

    REQUIRE(cpu.reg(3) == 0x55u);
}

TEST_CASE("COM - 0x00 complements to 0xFF", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(5, 0x00);
    cpu.exec_com(encode_com(5));

    REQUIRE(cpu.reg(5) == 0xFFu);
}

TEST_CASE("COM - 0xFF complements to 0x00", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(7, 0xFF);
    cpu.exec_com(encode_com(7));

    REQUIRE(cpu.reg(7) == 0x00u);
}

TEST_CASE("COM - adjacent registers unchanged", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(9,  0x11); // decoy before
    cpu.set_reg(10, 0x3C);
    cpu.set_reg(11, 0x22); // decoy after
    cpu.exec_com(encode_com(10));

    REQUIRE(cpu.reg(9)  == 0x11u);
    REQUIRE(cpu.reg(11) == 0x22u);
}

TEST_CASE("COM - C always set", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // C clear before
    cpu.set_reg(2, 0x55);
    cpu.exec_com(encode_com(2));

    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("COM - C set even when it was already set", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_C);
    cpu.set_reg(2, 0x55);
    cpu.exec_com(encode_com(2));

    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("COM - Z set when result is 0x00 (Rd was 0xFF)", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(4, 0xFF);
    cpu.set_sreg(0x00);
    cpu.exec_com(encode_com(4));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("COM - Z cleared when result is non-zero", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(4, 0xAA); // result = 0x55, non-zero
    cpu.set_sreg(SREG_Z); // pre-set Z
    cpu.exec_com(encode_com(4));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("COM - N set when result bit 7 is set", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(6, 0x01); // result = 0xFE, bit 7 set
    cpu.exec_com(encode_com(6));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("COM - N cleared when result bit 7 is clear", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(6, 0x80); // result = 0x7F, bit 7 clear
    cpu.set_sreg(SREG_N); // pre-set N
    cpu.exec_com(encode_com(6));

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

TEST_CASE("COM - V always cleared", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_V); // pre-set V
    cpu.set_reg(8, 0x55);
    cpu.exec_com(encode_com(8));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

TEST_CASE("COM - S equals N (since V is always 0)", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // result = 0xFE (N=1) → S should be 1
    cpu.set_reg(10, 0x01);
    cpu.set_sreg(0x00);
    cpu.exec_com(encode_com(10));

    REQUIRE((cpu.sreg() & SREG_S) != 0);

    // result = 0x7F (N=0) → S should be 0
    cpu.set_reg(10, 0x80);
    cpu.set_sreg(0x00);
    cpu.exec_com(encode_com(10));

    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

TEST_CASE("COM - T and I flags preserved", "[com][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T and I set
    cpu.set_reg(12, 0x55);
    cpu.exec_com(encode_com(12));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
}

// ---------------------------------------------------------------------------
// SBIW  (1001 0111 KKdd KKKK)
//
// Operation : Rd+1:Rd ← Rd+1:Rd - K  (K = 0..63, d ∈ {24,26,28,30})
// Cycles    : 2
// Flags     : C, Z, N, V, S (N XOR V)
//             H, T, I unaffected
// ---------------------------------------------------------------------------

// Encode SBIW Rd, K
//   dd bits [5:4]: (d-24)/2  →  24→0, 26→1, 28→2, 30→3
//   KK bits [7:6] and K bits [3:0]
static u16 encode_sbiw(u8 d, u8 k)
{
    // 1001 0111 KKdd KKKK
    u8 dd = (d - 24) >> 1;
    return static_cast<u16>(0x9700
        | ((k & 0x30) << 2)   // KK → bits [7:6]
        | ((dd & 0x03) << 4)  // dd → bits [5:4]
        | (k & 0x0F));        // K  → bits [3:0]
}

// Helper: set a 16-bit word into a register pair (lo, hi).
static void set_word(AvrCpu& cpu, u8 lo_reg, u16 val)
{
    cpu.set_reg(lo_reg,     static_cast<u8>(val & 0xFF));
    cpu.set_reg(lo_reg + 1, static_cast<u8>(val >> 8));
}

// Helper: read a 16-bit word from a register pair.
static u16 get_word(AvrCpu& cpu, u8 lo_reg)
{
    return static_cast<u16>(cpu.reg(lo_reg) | (cpu.reg(lo_reg + 1) << 8));
}

TEST_CASE("SBIW - basic subtraction (R25:R24)", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0010);
    cpu.exec_sbiw(encode_sbiw(24, 5));

    REQUIRE(get_word(cpu, 24) == 0x000Bu);
}

TEST_CASE("SBIW - basic subtraction (R27:R26)", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 26, 0x0100);
    cpu.exec_sbiw(encode_sbiw(26, 1));

    REQUIRE(get_word(cpu, 26) == 0x00FFu);
}

TEST_CASE("SBIW - basic subtraction (R29:R28)", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 28, 0x0040);
    cpu.exec_sbiw(encode_sbiw(28, 0x3F)); // subtract 63

    REQUIRE(get_word(cpu, 28) == 0x0001u);
}

TEST_CASE("SBIW - basic subtraction (R31:R30)", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 30, 0xABCD);
    cpu.exec_sbiw(encode_sbiw(30, 0x0D));

    REQUIRE(get_word(cpu, 30) == 0xABC0u);
}

TEST_CASE("SBIW - subtract 0 leaves word unchanged", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x1234);
    cpu.exec_sbiw(encode_sbiw(24, 0));

    REQUIRE(get_word(cpu, 24) == 0x1234u);
}

TEST_CASE("SBIW - adjacent register pairs unchanged", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0010);
    set_word(cpu, 26, 0xBEEF); // decoy
    cpu.exec_sbiw(encode_sbiw(24, 4));

    REQUIRE(get_word(cpu, 26) == 0xBEEFu); // untouched
}

TEST_CASE("SBIW - Z set when result is zero", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0007);
    cpu.set_sreg(0x00);
    cpu.exec_sbiw(encode_sbiw(24, 7));

    REQUIRE(get_word(cpu, 24) == 0x0000u);
    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("SBIW - Z cleared when result is non-zero", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0008);
    cpu.set_sreg(SREG_Z); // pre-set Z
    cpu.exec_sbiw(encode_sbiw(24, 7));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

TEST_CASE("SBIW - C (borrow) set when result wraps past zero", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x0000 - 1 wraps to 0xFFFF
    set_word(cpu, 24, 0x0000);
    cpu.set_sreg(0x00);
    cpu.exec_sbiw(encode_sbiw(24, 1));

    REQUIRE(get_word(cpu, 24) == 0xFFFFu);
    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("SBIW - C cleared when no borrow", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0010);
    cpu.set_sreg(SREG_C); // pre-set C
    cpu.exec_sbiw(encode_sbiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

TEST_CASE("SBIW - N set when result bit 15 is set", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x0000 - 1 = 0xFFFF → bit 15 set
    set_word(cpu, 24, 0x0000);
    cpu.set_sreg(0x00);
    cpu.exec_sbiw(encode_sbiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("SBIW - N cleared when result bit 15 is clear", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x00FF);
    cpu.set_sreg(SREG_N); // pre-set N
    cpu.exec_sbiw(encode_sbiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

TEST_CASE("SBIW - V set on signed overflow (0x8000 - 1 = 0x7FFF)", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x8000 (most-negative signed 16-bit) - 1 = 0x7FFF → overflow
    set_word(cpu, 24, 0x8000);
    cpu.set_sreg(0x00);
    cpu.exec_sbiw(encode_sbiw(24, 1));

    REQUIRE(get_word(cpu, 24) == 0x7FFFu);
    REQUIRE((cpu.sreg() & SREG_V) != 0);
}

TEST_CASE("SBIW - V cleared when no signed overflow", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0010);
    cpu.set_sreg(SREG_V); // pre-set V
    cpu.exec_sbiw(encode_sbiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

TEST_CASE("SBIW - S equals N XOR V", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // 0x8000 - 1: N=0, V=1 → S=1
    set_word(cpu, 24, 0x8000);
    cpu.set_sreg(0x00);
    cpu.exec_sbiw(encode_sbiw(24, 1));

    u8 N = (cpu.sreg() & SREG_N) ? 1 : 0;
    u8 V = (cpu.sreg() & SREG_V) ? 1 : 0;
    u8 S = (cpu.sreg() & SREG_S) ? 1 : 0;
    REQUIRE(S == (N ^ V));
}

TEST_CASE("SBIW - T and I flags preserved", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T and I set
    set_word(cpu, 24, 0x0010);
    cpu.exec_sbiw(encode_sbiw(24, 1));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
}

TEST_CASE("SBIW - returns 2 cycles", "[sbiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0010);
    u8 cycles = cpu.exec_sbiw(encode_sbiw(24, 1));

    REQUIRE(cycles == 2u);
}

// ---------------------------------------------------------------------------
// ADIW  (1001 0110 KKdd KKKK)
//
// Operation : Rd+1:Rd ← Rd+1:Rd + K  (K = 0..63, d in {24,26,28,30})
// Flags     : C = ~R15 & Rdh7
//             Z = (result == 0)
//             N = R15
//             V = R15 & ~Rdh7
//             S = N XOR V
//             H, T, I unaffected
// ---------------------------------------------------------------------------

// Encode ADIW Rd, K
//   dd bits: (d-24)/2  → 24→0, 26→1, 28→2, 30→3
static u16 encode_adiw(u8 d, u8 k)
{
    // 1001 0110 KKdd KKKK
    u8 dd = (d - 24) >> 1;
    return static_cast<u16>(0x9600
        | ((k & 0x30) << 2)   // KK → bits [7:6]
        | ((dd & 0x03) << 4)  // dd → bits [5:4]
        | (k & 0x0F));        // K  → bits [3:0]
}

TEST_CASE("ADIW - basic addition (R25:R24)", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0010);
    cpu.exec_adiw(encode_adiw(24, 5));

    REQUIRE(get_word(cpu, 24) == 0x0015u);
}

TEST_CASE("ADIW - uses R27:R26 (X register)", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 26, 0x0100);
    cpu.exec_adiw(encode_adiw(26, 1));

    REQUIRE(get_word(cpu, 26) == 0x0101u);
}

TEST_CASE("ADIW - uses R29:R28 (Y register)", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 28, 0x0040);
    cpu.exec_adiw(encode_adiw(28, 0x3F)); // add 63

    REQUIRE(get_word(cpu, 28) == 0x007Fu);
}

TEST_CASE("ADIW - uses R31:R30 (Z register)", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 30, 0xABCD);
    cpu.exec_adiw(encode_adiw(30, 0x0D));

    REQUIRE(get_word(cpu, 30) == 0xABDAu);
}

TEST_CASE("ADIW - max K value (63) applied correctly", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x1234);
    cpu.exec_adiw(encode_adiw(24, 63));

    REQUIRE(get_word(cpu, 24) == 0x1273u);
}

TEST_CASE("ADIW - K=0 leaves word unchanged", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0xBEEF);
    cpu.exec_adiw(encode_adiw(24, 0));

    REQUIRE(get_word(cpu, 24) == 0xBEEFu);
}

TEST_CASE("ADIW - carry into high byte", "[adiw][alu]")
{
    // 0x00FF + 1 = 0x0100 (low byte wraps, carry into high byte)
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x00FF);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE(get_word(cpu, 24) == 0x0100u);
}

TEST_CASE("ADIW - 16-bit overflow wraps to zero, Z flag set", "[adiw][alu]")
{
    // 0xFFFF + 1 = 0x0000 (with wrap)
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0xFFFF);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE(get_word(cpu, 24) == 0x0000u);
    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

// ---------------------------------------------------------------------------
// ADIW — Z flag
// ---------------------------------------------------------------------------

TEST_CASE("ADIW - Z set when result is zero", "[adiw][alu]")
{
    // 0xFFFF + 1 wraps to 0
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0xFFFF);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("ADIW - Z cleared when result is non-zero", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_Z); // pre-set Z
    set_word(cpu, 24, 0x0001);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

// ---------------------------------------------------------------------------
// ADIW — N flag
// ---------------------------------------------------------------------------

TEST_CASE("ADIW - N set when result bit 15 is set", "[adiw][alu]")
{
    // 0x7FFF + 1 = 0x8000 → N=1
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x7FFF);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("ADIW - N cleared when result bit 15 is clear", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_N); // pre-set N
    set_word(cpu, 24, 0x0001);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

// ---------------------------------------------------------------------------
// ADIW — V flag (overflow: unsigned 16-bit → signed overflow into bit 15)
// V = result[15] & ~Rdh[7]  (high bit of result set, but was clear before)
// ---------------------------------------------------------------------------

TEST_CASE("ADIW - V set on signed overflow (0x7FFF + 1)", "[adiw][alu]")
{
    // 0x7FFF + 1 = 0x8000: Rdh was 0x7F (bit7=0), result high byte 0x80 (bit15=1) → V=1
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x7FFF);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_V) != 0);
}

TEST_CASE("ADIW - V cleared for normal positive addition", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_V); // pre-set V
    set_word(cpu, 24, 0x0010);
    cpu.exec_adiw(encode_adiw(24, 5));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

TEST_CASE("ADIW - V cleared when high byte remains negative (0x8000 + 1)", "[adiw][alu]")
{
    // Rdh bit7=1, result bit15=1 → V = result[15] & ~Rdh[7] = 1 & 0 = 0
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x8000);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

// ---------------------------------------------------------------------------
// ADIW — C flag (carry out of bit 15)
// C = ~result[15] & Rdh[7]  (result went from negative to non-negative: wraparound)
// ---------------------------------------------------------------------------

TEST_CASE("ADIW - C set on unsigned overflow (0xFFFF + 1 wraps)", "[adiw][alu]")
{
    // Rdh = 0xFF (bit7=1), result = 0x0000 (bit15=0) → C = ~0 & 1 = 1
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0xFFFF);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("ADIW - C cleared for normal addition", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_C); // pre-set C
    set_word(cpu, 24, 0x0010);
    cpu.exec_adiw(encode_adiw(24, 5));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

// ---------------------------------------------------------------------------
// ADIW — S flag (S = N XOR V)
// ---------------------------------------------------------------------------

TEST_CASE("ADIW - S = N XOR V: both clear → S clear", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0001);
    cpu.exec_adiw(encode_adiw(24, 1)); // result=0x0002, N=0, V=0

    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

TEST_CASE("ADIW - S = N XOR V: N=1, V=0 → S=1", "[adiw][alu]")
{
    // 0x8000 + 1 = 0x8001: N=1, V=0 (Rdh bit7 was 1) → S=1
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x8000);
    cpu.exec_adiw(encode_adiw(24, 1));

    u8 N = (cpu.sreg() & SREG_N) ? 1 : 0;
    u8 V = (cpu.sreg() & SREG_V) ? 1 : 0;
    u8 S = (cpu.sreg() & SREG_S) ? 1 : 0;
    REQUIRE(S == (N ^ V));
}

TEST_CASE("ADIW - S = N XOR V: N=1, V=1 → S=0 (0x7FFF+1)", "[adiw][alu]")
{
    // 0x7FFF + 1 = 0x8000: N=1, V=1 → S=0
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x7FFF);
    cpu.exec_adiw(encode_adiw(24, 1));

    u8 N = (cpu.sreg() & SREG_N) ? 1 : 0;
    u8 V = (cpu.sreg() & SREG_V) ? 1 : 0;
    u8 S = (cpu.sreg() & SREG_S) ? 1 : 0;
    REQUIRE(S == (N ^ V));
    REQUIRE(S == 0);
}

// ---------------------------------------------------------------------------
// ADIW — unaffected bits
// ---------------------------------------------------------------------------

TEST_CASE("ADIW - H, T, I flags are unaffected", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xC0); // T=1, I=1
    set_word(cpu, 24, 0x0010);
    cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE((cpu.sreg() & 0xC0) == 0xC0);
}

TEST_CASE("ADIW - returns 2 cycles", "[adiw][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    set_word(cpu, 24, 0x0010);
    u8 cycles = cpu.exec_adiw(encode_adiw(24, 1));

    REQUIRE(cycles == 2u);
}

// ---------------------------------------------------------------------------
// ANDI  (0111 KKKK dddd KKKK)
//
// Operation : Rd ← Rd AND K    where d ∈ [16..31], K ∈ [0..255]
// Flags     : Z = (result == 0), N = result[7], V = 0, S = N XOR V = N
//             C and H are unaffected.
// ---------------------------------------------------------------------------

// Encode ANDI Rd, K opcode.
static u16 encode_andi(u8 d, u8 k)
{
    u8 d_off = d - 16; // 4-bit offset into R16–R31
    return static_cast<u16>(0x7000
        | ((k & 0xF0) << 4)       // K[7:4] → bits [11:8]
        | ((d_off & 0x0F) << 4)   // d offset → bits [7:4]
        | (k & 0x0F));            // K[3:0] → bits [3:0]
}

// ---------------------------------------------------------------------------
// Result and register tests
// ---------------------------------------------------------------------------

TEST_CASE("ANDI - AND of two non-zero values", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x0F));

    REQUIRE(cpu.reg(16) == 0x0F);
}

TEST_CASE("ANDI - immediate 0x00 clears register", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(20, 0xAB);
    cpu.exec_andi(encode_andi(20, 0x00));

    REQUIRE(cpu.reg(20) == 0x00);
}

TEST_CASE("ANDI - immediate 0xFF is identity (register unchanged)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(18, 0x5A);
    cpu.exec_andi(encode_andi(18, 0xFF));

    REQUIRE(cpu.reg(18) == 0x5A);
}

TEST_CASE("ANDI - masks out high nibble", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xCD);
    cpu.exec_andi(encode_andi(16, 0x0F)); // 0xCD & 0x0F = 0x0D

    REQUIRE(cpu.reg(16) == 0x0D);
}

TEST_CASE("ANDI - masks out low nibble", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xCD);
    cpu.exec_andi(encode_andi(16, 0xF0)); // 0xCD & 0xF0 = 0xC0

    REQUIRE(cpu.reg(16) == 0xC0);
}

TEST_CASE("ANDI - correct destination register selected (R16)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xFF);
    cpu.set_reg(17, 0xFF); // decoy
    cpu.exec_andi(encode_andi(16, 0x00));

    REQUIRE(cpu.reg(16) == 0x00);
    REQUIRE(cpu.reg(17) == 0xFF); // decoy unchanged
}

TEST_CASE("ANDI - correct destination register selected (R31)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(31, 0xFF);
    cpu.set_reg(30, 0xFF); // decoy
    cpu.exec_andi(encode_andi(31, 0x55));

    REQUIRE(cpu.reg(31) == 0x55);
    REQUIRE(cpu.reg(30) == 0xFF); // decoy unchanged
}

TEST_CASE("ANDI - correct destination register selected (R24, mid-range)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(24, 0b10110011);
    cpu.exec_andi(encode_andi(24, 0b11110000));

    REQUIRE(cpu.reg(24) == 0b10110000);
}

// ---------------------------------------------------------------------------
// Immediate decode tests
// ---------------------------------------------------------------------------

TEST_CASE("ANDI - K high nibble decoded correctly", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0xA0)); // K = 0xA0: only high nibble set

    REQUIRE(cpu.reg(16) == 0xA0);
}

TEST_CASE("ANDI - K low nibble decoded correctly", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x0B)); // K = 0x0B: only low nibble set

    REQUIRE(cpu.reg(16) == 0x0B);
}

// ---------------------------------------------------------------------------
// Z flag (Zero)
// ---------------------------------------------------------------------------

TEST_CASE("ANDI - Z set when result is zero", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xF0);
    cpu.exec_andi(encode_andi(16, 0x0F)); // 0xF0 & 0x0F = 0x00

    REQUIRE((cpu.sreg() & SREG_Z) != 0);
}

TEST_CASE("ANDI - Z cleared when result is non-zero", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_Z); // pre-set Z
    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_Z) == 0);
}

// ---------------------------------------------------------------------------
// N flag (Negative)
// ---------------------------------------------------------------------------

TEST_CASE("ANDI - N set when result bit 7 is set", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x80)); // result = 0x80 → bit 7 set

    REQUIRE((cpu.sreg() & SREG_N) != 0);
}

TEST_CASE("ANDI - N cleared when result bit 7 is clear", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_N); // pre-set N
    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x7F)); // result = 0x7F → bit 7 clear

    REQUIRE((cpu.sreg() & SREG_N) == 0);
}

// ---------------------------------------------------------------------------
// V flag (Overflow) — always cleared
// ---------------------------------------------------------------------------

TEST_CASE("ANDI - V always cleared (result negative)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_V); // pre-set V
    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x80)); // result = 0x80

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

TEST_CASE("ANDI - V always cleared (result zero)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_V); // pre-set V
    cpu.set_reg(16, 0x00);
    cpu.exec_andi(encode_andi(16, 0xFF));

    REQUIRE((cpu.sreg() & SREG_V) == 0);
}

// ---------------------------------------------------------------------------
// S flag (Sign = N XOR V = N since V=0)
// ---------------------------------------------------------------------------

TEST_CASE("ANDI - S set when N=1 (V=0)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x80)); // N=1, V=0 → S=1

    REQUIRE((cpu.sreg() & SREG_S) != 0);
}

TEST_CASE("ANDI - S cleared when N=0 (V=0)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_S); // pre-set S
    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x01)); // N=0, V=0 → S=0

    REQUIRE((cpu.sreg() & SREG_S) == 0);
}

// ---------------------------------------------------------------------------
// C and H flags — unaffected
// ---------------------------------------------------------------------------

TEST_CASE("ANDI - C flag unaffected (was set)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_C);
    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_C) != 0);
}

TEST_CASE("ANDI - C flag unaffected (was clear)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00);
    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_C) == 0);
}

TEST_CASE("ANDI - H flag unaffected (was set)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(SREG_H);
    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_H) != 0);
}

TEST_CASE("ANDI - H flag unaffected (was clear)", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00);
    cpu.set_reg(16, 0xFF);
    cpu.exec_andi(encode_andi(16, 0x01));

    REQUIRE((cpu.sreg() & SREG_H) == 0);
}

// ---------------------------------------------------------------------------
// Cycle count
// ---------------------------------------------------------------------------

TEST_CASE("ANDI - returns 1 cycle", "[andi][alu]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(16, 0xFF);
    u8 cycles = cpu.exec_andi(encode_andi(16, 0x01));

    REQUIRE(cycles == 1u);
}
