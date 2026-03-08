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
