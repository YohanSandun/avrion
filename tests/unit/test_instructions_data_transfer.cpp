#include <catch2/catch_test_macros.hpp>

#include "cpu/avr_cpu.h"
#include "device/device_config.h"
#include "memory/memory_map.h"

using namespace avrion;

// ---------------------------------------------------------------------------
// Config helpers
// ---------------------------------------------------------------------------

// Generic IO test config: maps io_base into SRAM so writes can be read back
// without needing any special-register wiring.
static DeviceConfig make_io_in_sram_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 32 * 1024;
    c.io_base          = 0x0100;  // IO sits at the start of SRAM
    c.sram_base        = 0x0100;
    c.sram_size_bytes  = 256;
    c.x_low_reg        = 26;      // standard AVR: R26 = XL, R27 = XH
    c.x_high_reg       = 27;
    return c;
}

// SREG test config: standard ATmega-style layout.
// SREG I/O address = 0x3F  →  data address = io_base (0x20) + 0x3F = 0x5F
static DeviceConfig make_sreg_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 32 * 1024;
    c.io_base          = 0x0020;
    c.sram_base        = 0x0100;
    c.sram_size_bytes  = 2 * 1024;
    c.sreg_data_addr   = 0x005F;  // 0x20 + 0x3F
    return c;
}

// ---------------------------------------------------------------------------
// Opcode encoder
//
// OUT  1011 1AAr rrrr AAAA
//   A[5:4] → bits [10:9]
//   r[4:0] → bits [8:4]
//   A[3:0] → bits [3:0]
// ---------------------------------------------------------------------------
static u16 encode_out(u8 A, u8 r)
{
    return static_cast<u16>(0xB800
        | ((A & 0x30) << 5)   // A[5:4] → bits [10:9]
        | ((r & 0x1F) << 4)   // r[4:0] → bits [8:4]
        | (A & 0x0F));        // A[3:0] → bits [3:0]
}

// ---------------------------------------------------------------------------
// OUT — result / register tests
// ---------------------------------------------------------------------------

TEST_CASE("OUT - value from Rr is written to the IO address", "[out][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(5, 0xAB);
    cpu.exec_out(encode_out(0x01, 5)); // OUT 0x01, R5 → write8(io_base+1, 0xAB)

    REQUIRE(mem.read8(cfg.io_base + 0x01) == 0xAB);
}

TEST_CASE("OUT - source register is not modified", "[out][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(7, 0x55);
    cpu.exec_out(encode_out(0x02, 7));

    REQUIRE(cpu.reg(7) == 0x55);
}

TEST_CASE("OUT - correct register is selected (R0)", "[out][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(0, 0x11);
    cpu.set_reg(1, 0x22); // decoy
    cpu.exec_out(encode_out(0x00, 0));

    REQUIRE(mem.read8(cfg.io_base + 0x00) == 0x11);
}

TEST_CASE("OUT - correct register is selected (R31)", "[out][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(31, 0xCC);
    cpu.set_reg(30, 0xFF); // decoy
    cpu.exec_out(encode_out(0x00, 31));

    REQUIRE(mem.read8(cfg.io_base + 0x00) == 0xCC);
}

TEST_CASE("OUT - correct register is selected (R16, upper half)", "[out][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(16, 0x9D);
    cpu.exec_out(encode_out(0x05, 16));

    REQUIRE(mem.read8(cfg.io_base + 0x05) == 0x9D);
}

// ---------------------------------------------------------------------------
// OUT — I/O address decoding tests
// ---------------------------------------------------------------------------

TEST_CASE("OUT - address field A decoded correctly (low nibble only)", "[out][data_transfer]")
{
    // A = 0x0A: only bits [3:0] used, bits [5:4] are 0
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(2, 0xBB);
    cpu.exec_out(encode_out(0x0A, 2)); // → data addr io_base + 0x0A

    REQUIRE(mem.read8(cfg.io_base + 0x0A) == 0xBB);
    // adjacent address must be untouched
    REQUIRE(mem.read8(cfg.io_base + 0x0B) == 0x00);
}

TEST_CASE("OUT - address field A decoded correctly (bits [5:4] set)", "[out][data_transfer]")
{
    // A = 0x3F: maximum 6-bit address (all bits set)
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(3, 0x42);
    cpu.exec_out(encode_out(0x3F, 3));

    REQUIRE(mem.read8(cfg.io_base + 0x3F) == 0x42);
}

// ---------------------------------------------------------------------------
// OUT — SREG special-register test
// ---------------------------------------------------------------------------

TEST_CASE("OUT - writing to SREG IO address updates cpu sreg", "[out][data_transfer]")
{
    // AVR convention: SREG I/O address = 0x3F  →  data addr = 0x5F
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);     // required: write8 calls cpu_->set_sreg()

    cpu.set_sreg(0x00);
    cpu.set_reg(10, 0xA5);

    cpu.exec_out(encode_out(0x3F, 10)); // OUT SREG, R10

    REQUIRE(cpu.sreg() == 0xA5);
}

TEST_CASE("OUT - SREG write reflects exact bit pattern", "[out][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(20, 0b10110101); // arbitrary pattern
    cpu.exec_out(encode_out(0x3F, 20));

    REQUIRE(cpu.sreg() == 0b10110101);
}

TEST_CASE("OUT - SREG cleared when writing zero", "[out][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sreg(0xFF); // pre-fill
    cpu.set_reg(0, 0x00);
    cpu.exec_out(encode_out(0x3F, 0)); // OUT SREG, R0

    REQUIRE(cpu.sreg() == 0x00);
}

// ---------------------------------------------------------------------------
// LDI  (1110 KKKK dddd KKKK)
//
// Loads an 8-bit immediate K into Rd, where d ∈ [16..31].
// Flags: none affected.
// ---------------------------------------------------------------------------

// Encode LDI Rd, K
static u16 encode_ldi(u8 d, u8 K)
{
    // d must be in [16..31]; offset = d - 16
    return static_cast<u16>(0xE000
        | ((K  & 0xF0) << 4)          // K[7:4] → bits [11:8]
        | (((d - 16) & 0x0F) << 4)    // d offset → bits [7:4]
        | (K  & 0x0F));               // K[3:0] → bits [3:0]
}

TEST_CASE("LDI - loads immediate into Rd", "[ldi][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.exec_ldi(encode_ldi(16, 0xAB));

    REQUIRE(cpu.reg(16) == 0xAB);
}

TEST_CASE("LDI - full byte immediate 0xFF", "[ldi][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.exec_ldi(encode_ldi(20, 0xFF));

    REQUIRE(cpu.reg(20) == 0xFF);
}

TEST_CASE("LDI - zero immediate clears register", "[ldi][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(24, 0xCD); // pre-fill
    cpu.exec_ldi(encode_ldi(24, 0x00));

    REQUIRE(cpu.reg(24) == 0x00);
}

TEST_CASE("LDI - K high nibble decoded correctly", "[ldi][data_transfer]")
{
    // K = 0xA0: only high nibble set; low nibble is 0
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.exec_ldi(encode_ldi(18, 0xA0));

    REQUIRE(cpu.reg(18) == 0xA0);
}

TEST_CASE("LDI - K low nibble decoded correctly", "[ldi][data_transfer]")
{
    // K = 0x0B: only low nibble set; high nibble is 0
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.exec_ldi(encode_ldi(18, 0x0B));

    REQUIRE(cpu.reg(18) == 0x0B);
}

TEST_CASE("LDI - correct destination register selected (R16)", "[ldi][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.exec_ldi(encode_ldi(16, 0x11));

    REQUIRE(cpu.reg(16) == 0x11);
    REQUIRE(cpu.reg(17) == 0x00); // adjacent unchanged
}

TEST_CASE("LDI - correct destination register selected (R31)", "[ldi][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.exec_ldi(encode_ldi(31, 0x77));

    REQUIRE(cpu.reg(31) == 0x77);
    REQUIRE(cpu.reg(30) == 0x00); // adjacent unchanged
}

TEST_CASE("LDI - correct destination register selected (R24, mid-range)", "[ldi][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.exec_ldi(encode_ldi(24, 0x55));

    REQUIRE(cpu.reg(24) == 0x55);
}

TEST_CASE("LDI - flags not affected", "[ldi][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    constexpr u8 sentinel = 0b10110101;
    cpu.set_sreg(sentinel);
    cpu.exec_ldi(encode_ldi(16, 0xFF));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// ST X  (1001 001r rrrr 1100)
//
// Stores Rr to the data address held in the X pointer register (r27:r26).
// Neither the source register nor X is modified.
// ---------------------------------------------------------------------------

// Encode ST X, Rr
static u16 encode_st_x(u8 r)
{
    return static_cast<u16>(0x920C | ((r & 0x1F) << 4));
}

// Config with io_base inside SRAM so that any SRAM write is readable.
// Reuses make_io_in_sram_config(): sram_base = io_base = 0x0100, size = 256.
// X pointer values in range [0x0100, 0x01FF] are valid.

TEST_CASE("ST X - writes Rr value to memory at X address", "[st_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // XL — X = 0x0100
    cpu.set_reg(27, 0x01); // XH
    cpu.set_reg(5, 0xAB);
    cpu.exec_st_x(encode_st_x(5));

    REQUIRE(mem.read8(0x0100) == 0xAB);
}

TEST_CASE("ST X - writes zero value correctly", "[st_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x05); // X = 0x0105
    cpu.set_reg(27, 0x01);
    cpu.set_reg(2, 0x00);
    cpu.exec_st_x(encode_st_x(2));

    REQUIRE(mem.read8(0x0105) == 0x00);
}

TEST_CASE("ST X - writes 0xFF correctly", "[st_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x0A); // X = 0x010A
    cpu.set_reg(27, 0x01);
    cpu.set_reg(3, 0xFF);
    cpu.exec_st_x(encode_st_x(3));

    REQUIRE(mem.read8(0x010A) == 0xFF);
}

TEST_CASE("ST X - correct source register selected (R0)", "[st_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(0,  0x11);
    cpu.set_reg(1,  0x22); // decoy
    cpu.exec_st_x(encode_st_x(0));

    REQUIRE(mem.read8(0x0100) == 0x11);
}

TEST_CASE("ST X - correct source register selected (R31)", "[st_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(31, 0xCC);
    cpu.set_reg(30, 0xFF); // decoy
    cpu.exec_st_x(encode_st_x(31));

    REQUIRE(mem.read8(0x0100) == 0xCC);
}

TEST_CASE("ST X - correct source register selected (R10, mid-range)", "[st_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(10, 0x7E);
    cpu.set_reg(11, 0x3F); // decoy
    cpu.exec_st_x(encode_st_x(10));

    REQUIRE(mem.read8(0x0100) == 0x7E);
}

TEST_CASE("ST X - X low byte forms correct target address", "[st_x][data_transfer]")
{
    // Vary XL to confirm the low byte steers the write to different addresses.
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x15); // X = 0x0115
    cpu.set_reg(27, 0x01);
    cpu.set_reg(4, 0xBB);
    cpu.exec_st_x(encode_st_x(4));

    REQUIRE(mem.read8(0x0115) == 0xBB);
    REQUIRE(mem.read8(0x0100) == 0x00); // other address untouched
}

TEST_CASE("ST X - X high byte forms correct target address", "[st_x][data_transfer]")
{
    // Both bytes of X must be used: write to 0x0100 and verify 0x0000 is untouched.
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100  (not 0x0000)
    cpu.set_reg(27, 0x01);
    cpu.set_reg(6, 0xD3);
    cpu.exec_st_x(encode_st_x(6));

    REQUIRE(mem.read8(0x0100) == 0xD3);
}

TEST_CASE("ST X - source register not modified", "[st_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(8, 0x55);
    cpu.exec_st_x(encode_st_x(8));

    REQUIRE(cpu.reg(8) == 0x55);
}

TEST_CASE("ST X - X pointer registers not modified", "[st_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x0F); // X = 0x010F
    cpu.set_reg(27, 0x01);
    cpu.set_reg(9, 0x44);
    cpu.exec_st_x(encode_st_x(9));

    REQUIRE(cpu.reg(26) == 0x0F);
    REQUIRE(cpu.reg(27) == 0x01);
}

TEST_CASE("ST X - flags not affected", "[st_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b11001010;
    cpu.set_sreg(sentinel);
    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(7, 0x12);
    cpu.exec_st_x(encode_st_x(7));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// ST X+  (1001 001r rrrr 1101)
//
// Stores Rr to the address in X, then increments X by 1.
// Source register and flags are not modified.
// ---------------------------------------------------------------------------

// Encode ST X+, Rr
static u16 encode_st_x_post_inc(u8 r)
{
    return static_cast<u16>(0x920D | ((r & 0x1F) << 4));
}

TEST_CASE("ST X+ - writes Rr value to memory at X address", "[st_x_post_inc][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(5, 0xAB);
    cpu.exec_st_x_post_inc(encode_st_x_post_inc(5));

    REQUIRE(mem.read8(0x0100) == 0xAB);
}

TEST_CASE("ST X+ - X is incremented by 1 after store", "[st_x_post_inc][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(3, 0x55);
    cpu.exec_st_x_post_inc(encode_st_x_post_inc(3));

    // X must now point to 0x0101
    REQUIRE(cpu.reg(26) == 0x01); // XL
    REQUIRE(cpu.reg(27) == 0x01); // XH unchanged
}

TEST_CASE("ST X+ - write happens before increment (store at original address)", "[st_x_post_inc][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x05); // X = 0x0105
    cpu.set_reg(27, 0x01);
    cpu.set_reg(4, 0x77);
    cpu.exec_st_x_post_inc(encode_st_x_post_inc(4));

    REQUIRE(mem.read8(0x0105) == 0x77);   // stored at original X
    REQUIRE(mem.read8(0x0106) == 0x00);   // incremented address untouched
}

TEST_CASE("ST X+ - XL wraps and carries into XH on 0xFF->0x00 boundary", "[st_x_post_inc][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // X = 0x01FF — store here, then X should become 0x0200
    cpu.set_reg(26, 0xFF); // XL
    cpu.set_reg(27, 0x01); // XH
    cpu.set_reg(2, 0x99);
    cpu.exec_st_x_post_inc(encode_st_x_post_inc(2));

    REQUIRE(mem.read8(0x01FF) == 0x99);
    REQUIRE(cpu.reg(26) == 0x00); // XL wrapped
    REQUIRE(cpu.reg(27) == 0x02); // XH carried
}

TEST_CASE("ST X+ - correct source register selected (R0)", "[st_x_post_inc][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(0, 0x11);
    cpu.set_reg(1, 0x22); // decoy
    cpu.exec_st_x_post_inc(encode_st_x_post_inc(0));

    REQUIRE(mem.read8(0x0100) == 0x11);
}

TEST_CASE("ST X+ - correct source register selected (R31)", "[st_x_post_inc][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(31, 0xCC);
    cpu.set_reg(30, 0xFF); // decoy
    cpu.exec_st_x_post_inc(encode_st_x_post_inc(31));

    REQUIRE(mem.read8(0x0100) == 0xCC);
}

TEST_CASE("ST X+ - source register not modified", "[st_x_post_inc][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(8, 0x55);
    cpu.exec_st_x_post_inc(encode_st_x_post_inc(8));

    REQUIRE(cpu.reg(8) == 0x55);
}

TEST_CASE("ST X+ - flags not affected", "[st_x_post_inc][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b10100101;
    cpu.set_sreg(sentinel);
    cpu.set_reg(26, 0x00); // X = 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(6, 0x34);
    cpu.exec_st_x_post_inc(encode_st_x_post_inc(6));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// ST -X  (1001 001r rrrr 1110)
//
// Decrements X by 1, then stores Rr to the new X address.
// Source register and flags are not modified.
// ---------------------------------------------------------------------------

// Encode ST -X, Rr
static u16 encode_st_x_pre_dec(u8 r)
{
    return static_cast<u16>(0x920E | ((r & 0x1F) << 4));
}

TEST_CASE("ST -X - writes Rr value to memory at decremented X address", "[st_x_pre_dec][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x01); // X = 0x0101  →  pre-dec → store at 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(5, 0xAB);
    cpu.exec_st_x_pre_dec(encode_st_x_pre_dec(5));

    REQUIRE(mem.read8(0x0100) == 0xAB);
}

TEST_CASE("ST -X - X is decremented by 1 before store", "[st_x_pre_dec][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x05); // X = 0x0105  →  pre-dec → X = 0x0104
    cpu.set_reg(27, 0x01);
    cpu.set_reg(3, 0x55);
    cpu.exec_st_x_pre_dec(encode_st_x_pre_dec(3));

    REQUIRE(cpu.reg(26) == 0x04); // XL decremented
    REQUIRE(cpu.reg(27) == 0x01); // XH unchanged
}

TEST_CASE("ST -X - decrement happens before write (store at X-1, not original X)", "[st_x_pre_dec][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x0A); // X = 0x010A  →  pre-dec → store at 0x0109
    cpu.set_reg(27, 0x01);
    cpu.set_reg(4, 0x77);
    cpu.exec_st_x_pre_dec(encode_st_x_pre_dec(4));

    REQUIRE(mem.read8(0x0109) == 0x77);  // stored at decremented address
    REQUIRE(mem.read8(0x010A) == 0x00);  // original address untouched
}

TEST_CASE("ST -X - XL borrows from XH on 0x00->0xFF boundary", "[st_x_pre_dec][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // X = 0x0200  →  pre-dec → store at 0x01FF
    cpu.set_reg(26, 0x00); // XL
    cpu.set_reg(27, 0x02); // XH
    cpu.set_reg(2, 0x99);
    cpu.exec_st_x_pre_dec(encode_st_x_pre_dec(2));

    REQUIRE(mem.read8(0x01FF) == 0x99);
    REQUIRE(cpu.reg(26) == 0xFF); // XL borrowed
    REQUIRE(cpu.reg(27) == 0x01); // XH decremented
}

TEST_CASE("ST -X - correct source register selected (R0)", "[st_x_pre_dec][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x01); // X = 0x0101 → store at 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(0, 0x11);
    cpu.set_reg(1, 0x22); // decoy
    cpu.exec_st_x_pre_dec(encode_st_x_pre_dec(0));

    REQUIRE(mem.read8(0x0100) == 0x11);
}

TEST_CASE("ST -X - correct source register selected (R31)", "[st_x_pre_dec][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x01); // X = 0x0101 → store at 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(31, 0xCC);
    cpu.set_reg(30, 0xFF); // decoy
    cpu.exec_st_x_pre_dec(encode_st_x_pre_dec(31));

    REQUIRE(mem.read8(0x0100) == 0xCC);
}

TEST_CASE("ST -X - source register not modified", "[st_x_pre_dec][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(26, 0x01); // X = 0x0101 → store at 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(8, 0x55);
    cpu.exec_st_x_pre_dec(encode_st_x_pre_dec(8));

    REQUIRE(cpu.reg(8) == 0x55);
}

TEST_CASE("ST -X - flags not affected", "[st_x_pre_dec][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b01011010;
    cpu.set_sreg(sentinel);
    cpu.set_reg(26, 0x01); // X = 0x0101 → store at 0x0100
    cpu.set_reg(27, 0x01);
    cpu.set_reg(7, 0x12);
    cpu.exec_st_x_pre_dec(encode_st_x_pre_dec(7));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// IN  (1011 0AAr rrrr AAAA)
//
// Reads from I/O address A into Rd.
// I/O address mapping: data_addr = io_base + A.
// Flags: none affected.
// ---------------------------------------------------------------------------

// IN  1011 0AAr rrrr AAAA
//   A[5:4] → bits [10:9]
//   r[4:0] → bits [8:4]
//   A[3:0] → bits [3:0]
static u16 encode_in(u8 A, u8 r)
{
    return static_cast<u16>(0xB000
        | ((A & 0x30) << 5)   // A[5:4] → bits [10:9]
        | ((r & 0x1F) << 4)   // r[4:0] → bits [8:4]
        | (A & 0x0F));        // A[3:0] → bits [3:0]
}

// ---------------------------------------------------------------------------
// IN — result / register tests
// ---------------------------------------------------------------------------

TEST_CASE("IN - value at IO address is loaded into Rd", "[in][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(cfg.io_base + 0x01, 0xAB);
    cpu.exec_in(encode_in(0x01, 5)); // IN R5, 0x01

    REQUIRE(cpu.reg(5) == 0xAB);
}

TEST_CASE("IN - IO address is not modified after read", "[in][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(cfg.io_base + 0x02, 0x55);
    cpu.exec_in(encode_in(0x02, 7));

    REQUIRE(mem.read8(cfg.io_base + 0x02) == 0x55);
}

TEST_CASE("IN - zero value at IO address loads zero into Rd", "[in][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(3, 0xFF); // pre-fill to confirm zero is actually written
    // io_base+0x03 is zero-initialised
    cpu.exec_in(encode_in(0x03, 3));

    REQUIRE(cpu.reg(3) == 0x00);
}

TEST_CASE("IN - 0xFF value is loaded correctly", "[in][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(cfg.io_base + 0x04, 0xFF);
    cpu.exec_in(encode_in(0x04, 10));

    REQUIRE(cpu.reg(10) == 0xFF);
}

// ---------------------------------------------------------------------------
// IN — destination register selection
// ---------------------------------------------------------------------------

TEST_CASE("IN - correct destination register selected (R0)", "[in][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(cfg.io_base + 0x00, 0x11);
    cpu.set_reg(1, 0x22); // decoy
    cpu.exec_in(encode_in(0x00, 0));

    REQUIRE(cpu.reg(0) == 0x11);
    REQUIRE(cpu.reg(1) == 0x22); // decoy unchanged
}

TEST_CASE("IN - correct destination register selected (R31)", "[in][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(cfg.io_base + 0x00, 0xCC);
    cpu.set_reg(30, 0xFF); // decoy
    cpu.exec_in(encode_in(0x00, 31));

    REQUIRE(cpu.reg(31) == 0xCC);
    REQUIRE(cpu.reg(30) == 0xFF); // decoy unchanged
}

TEST_CASE("IN - correct destination register selected (R16, upper half)", "[in][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(cfg.io_base + 0x05, 0x9D);
    cpu.exec_in(encode_in(0x05, 16));

    REQUIRE(cpu.reg(16) == 0x9D);
}

// ---------------------------------------------------------------------------
// IN — I/O address decoding
// ---------------------------------------------------------------------------

TEST_CASE("IN - address field A decoded correctly (low nibble only)", "[in][data_transfer]")
{
    // A = 0x0A: only bits [3:0] used, bits [5:4] are 0
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(cfg.io_base + 0x0A, 0xBB);
    cpu.exec_in(encode_in(0x0A, 2));

    REQUIRE(cpu.reg(2) == 0xBB);
}

TEST_CASE("IN - address field A decoded correctly (bits [5:4] set)", "[in][data_transfer]")
{
    // A = 0x3F: maximum 6-bit address (all bits set)
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(cfg.io_base + 0x3F, 0x42);
    cpu.exec_in(encode_in(0x3F, 3));

    REQUIRE(cpu.reg(3) == 0x42);
}

TEST_CASE("IN - only the target register is written, adjacent registers unchanged", "[in][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(cfg.io_base + 0x01, 0xDE);
    cpu.set_reg(8,  0x00);
    cpu.set_reg(9,  0x77); // decoy above
    cpu.set_reg(7,  0x88); // decoy below
    cpu.exec_in(encode_in(0x01, 8));

    REQUIRE(cpu.reg(8) == 0xDE);
    REQUIRE(cpu.reg(9) == 0x77);
    REQUIRE(cpu.reg(7) == 0x88);
}

// ---------------------------------------------------------------------------
// IN — SREG special-register test
// ---------------------------------------------------------------------------

TEST_CASE("IN - reading from SREG IO address loads cpu sreg value into Rd", "[in][data_transfer]")
{
    // AVR convention: SREG I/O address = 0x3F  →  data addr = 0x5F
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sreg(0xA5);
    cpu.exec_in(encode_in(0x3F, 10)); // IN R10, SREG

    REQUIRE(cpu.reg(10) == 0xA5);
}

TEST_CASE("IN - SREG read reflects exact bit pattern", "[in][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sreg(0b10110101);
    cpu.exec_in(encode_in(0x3F, 20));

    REQUIRE(cpu.reg(20) == 0b10110101);
}

// ---------------------------------------------------------------------------
// IN — flags not affected
// ---------------------------------------------------------------------------

TEST_CASE("IN - flags not affected", "[in][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b11001010;
    cpu.set_sreg(sentinel);
    mem.write8(cfg.io_base + 0x01, 0xFF);
    cpu.exec_in(encode_in(0x01, 4));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// LDS  (1001 000d dddd 0000 | kkkk kkkk kkkk kkkk)
//
// Two-word instruction. The second word in flash (at pc()) holds the 16-bit
// data address k. Reads mem[k] into Rd.
// Flags: none affected. PC: not modified by exec_lds (caller manages advance).
// ---------------------------------------------------------------------------

// LDS  1001 000d dddd 0000
//   d[4:0] → bits [8:4]
static u16 encode_lds(u8 d)
{
    return static_cast<u16>(0x9000 | ((d & 0x1F) << 4));
}

// Helper: write a 16-bit little-endian word into flash at a byte address.
static void lds_flash_write16(std::vector<u8>& flash, u32 byte_addr, u16 value)
{
    flash[byte_addr]     = static_cast<u8>(value & 0xFF);
    flash[byte_addr + 1] = static_cast<u8>(value >> 8);
}

// Config with a simple SRAM region starting at 0x0100 (256 bytes).
// flash_size_bytes provides room for test code at low byte addresses.
static DeviceConfig make_lds_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 32 * 1024;
    c.sram_base        = 0x0100;
    c.sram_size_bytes  = 256;
    return c;
}

// ---------------------------------------------------------------------------
// LDS — value / result tests
// ---------------------------------------------------------------------------

TEST_CASE("LDS - value at data address is loaded into Rd", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0110, 0xAB);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0110); // second word = address 0x0110
    cpu.exec_lds(encode_lds(5));

    REQUIRE(cpu.reg(5) == 0xAB);
}

TEST_CASE("LDS - zero value at data address loads zero into Rd", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(3, 0xFF); // pre-fill to confirm zero is actually written
    // 0x0100 is zero-initialised
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_lds(encode_lds(3));

    REQUIRE(cpu.reg(3) == 0x00);
}

TEST_CASE("LDS - 0xFF value at data address is loaded correctly", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0105, 0xFF);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0105);
    cpu.exec_lds(encode_lds(10));

    REQUIRE(cpu.reg(10) == 0xFF);
}

TEST_CASE("LDS - source memory not modified after read", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0108, 0x42);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0108);
    cpu.exec_lds(encode_lds(7));

    REQUIRE(mem.read8(0x0108) == 0x42);
}

// ---------------------------------------------------------------------------
// LDS — destination register selection
// ---------------------------------------------------------------------------

TEST_CASE("LDS - correct destination register selected (R0)", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0100, 0x11);
    cpu.set_reg(1, 0x22); // decoy
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_lds(encode_lds(0));

    REQUIRE(cpu.reg(0) == 0x11);
    REQUIRE(cpu.reg(1) == 0x22); // decoy unchanged
}

TEST_CASE("LDS - correct destination register selected (R31)", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0100, 0xCC);
    cpu.set_reg(30, 0xFF); // decoy
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_lds(encode_lds(31));

    REQUIRE(cpu.reg(31) == 0xCC);
    REQUIRE(cpu.reg(30) == 0xFF); // decoy unchanged
}

TEST_CASE("LDS - correct destination register selected (R16, upper half)", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0112, 0x9D);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0112);
    cpu.exec_lds(encode_lds(16));

    REQUIRE(cpu.reg(16) == 0x9D);
}

TEST_CASE("LDS - only the target register is written, adjacent unchanged", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0100, 0xDE);
    cpu.set_reg(8, 0x00);
    cpu.set_reg(9, 0x77); // decoy above
    cpu.set_reg(7, 0x88); // decoy below
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_lds(encode_lds(8));

    REQUIRE(cpu.reg(8) == 0xDE);
    REQUIRE(cpu.reg(9) == 0x77);
    REQUIRE(cpu.reg(7) == 0x88);
}

// ---------------------------------------------------------------------------
// LDS — address word read from the correct flash position (pc())
// ---------------------------------------------------------------------------

TEST_CASE("LDS - address word is read from flash at pc()", "[lds][data_transfer]")
{
    // PC = 4: the address word must be read from byte offset 4, not offset 2.
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0110, 0xBB);
    cpu.set_pc(4);
    lds_flash_write16(mem.flash(), 4, 0x0110); // correct slot
    lds_flash_write16(mem.flash(), 2, 0xFFFF); // wrong slot — must not be used
    cpu.exec_lds(encode_lds(5));

    REQUIRE(cpu.reg(5) == 0xBB);
}

TEST_CASE("LDS - address low byte decoded correctly", "[lds][data_transfer]")
{
    // k = 0x0115: low byte = 0x15 drives address within the SRAM page
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0115, 0x77);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0115);
    cpu.exec_lds(encode_lds(4));

    REQUIRE(cpu.reg(4) == 0x77);
}

TEST_CASE("LDS - address high byte decoded correctly", "[lds][data_transfer]")
{
    // k = 0x0100: high byte = 0x01 selects the correct SRAM page.
    // If the high byte were ignored the read would target 0x0000 (not SRAM) and return 0.
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0100, 0xD3);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_lds(encode_lds(6));

    REQUIRE(cpu.reg(6) == 0xD3);
}

// ---------------------------------------------------------------------------
// LDS — PC advanced past the address word
// ---------------------------------------------------------------------------

TEST_CASE("LDS - PC is advanced by 2 after reading the address word", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_lds(encode_lds(5));

    REQUIRE(cpu.pc() == 4u);
}

// ---------------------------------------------------------------------------
// LDS — flags not affected
// ---------------------------------------------------------------------------

TEST_CASE("LDS - flags not affected", "[lds][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b10110101;
    cpu.set_sreg(sentinel);
    mem.write8(0x0100, 0xFF);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_lds(encode_lds(4));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// STS  (1001 001d dddd 0000 | kkkk kkkk kkkk kkkk)
//
// Two-word instruction. The second word in flash (at pc()) holds the 16-bit
// data address k. Writes reg(d) to mem[k] and advances PC by 2.
// Flags: none affected.
// ---------------------------------------------------------------------------

// STS  1001 001d dddd 0000
//   d[4:0] → bits [8:4]
static u16 encode_sts(u8 d)
{
    return static_cast<u16>(0x9200 | ((d & 0x1F) << 4));
}

// Reuses lds_flash_write16 and make_lds_config helpers from the LDS section.

// ---------------------------------------------------------------------------
// STS — value / result tests
// ---------------------------------------------------------------------------

TEST_CASE("STS - value from Rd is written to the data address", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(5, 0xAB);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0110);
    cpu.exec_sts(encode_sts(5));

    REQUIRE(mem.read8(0x0110) == 0xAB);
}

TEST_CASE("STS - zero value is written correctly", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    mem.write8(0x0100, 0xFF); // pre-fill to confirm zero overwrites
    cpu.set_reg(3, 0x00);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_sts(encode_sts(3));

    REQUIRE(mem.read8(0x0100) == 0x00);
}

TEST_CASE("STS - 0xFF value is written correctly", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(10, 0xFF);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0105);
    cpu.exec_sts(encode_sts(10));

    REQUIRE(mem.read8(0x0105) == 0xFF);
}

TEST_CASE("STS - source register is not modified", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(7, 0x42);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0108);
    cpu.exec_sts(encode_sts(7));

    REQUIRE(cpu.reg(7) == 0x42);
}

// ---------------------------------------------------------------------------
// STS — source register selection
// ---------------------------------------------------------------------------

TEST_CASE("STS - correct source register selected (R0)", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(0, 0x11);
    cpu.set_reg(1, 0x22); // decoy
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_sts(encode_sts(0));

    REQUIRE(mem.read8(0x0100) == 0x11);
}

TEST_CASE("STS - correct source register selected (R31)", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(31, 0xCC);
    cpu.set_reg(30, 0xFF); // decoy
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_sts(encode_sts(31));

    REQUIRE(mem.read8(0x0100) == 0xCC);
}

TEST_CASE("STS - correct source register selected (R16, upper half)", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(16, 0x9D);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0112);
    cpu.exec_sts(encode_sts(16));

    REQUIRE(mem.read8(0x0112) == 0x9D);
}

// ---------------------------------------------------------------------------
// STS — address word read from the correct flash position (pc())
// ---------------------------------------------------------------------------

TEST_CASE("STS - address word is read from flash at pc()", "[sts][data_transfer]")
{
    // PC = 4: address word must come from byte offset 4, not 2.
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(5, 0xBB);
    cpu.set_pc(4);
    lds_flash_write16(mem.flash(), 4, 0x0110); // correct slot
    lds_flash_write16(mem.flash(), 2, 0x0120); // wrong slot — must not be used
    cpu.exec_sts(encode_sts(5));

    REQUIRE(mem.read8(0x0110) == 0xBB);
    REQUIRE(mem.read8(0x0120) == 0x00); // wrong slot untouched
}

TEST_CASE("STS - address low byte decoded correctly", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(4, 0x77);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0115);
    cpu.exec_sts(encode_sts(4));

    REQUIRE(mem.read8(0x0115) == 0x77);
}

TEST_CASE("STS - address high byte decoded correctly", "[sts][data_transfer]")
{
    // k = 0x0100: if the high byte were ignored the write would target 0x0000 (not SRAM).
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(6, 0xD3);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_sts(encode_sts(6));

    REQUIRE(mem.read8(0x0100) == 0xD3);
}

TEST_CASE("STS - only the target address is written, adjacent memory unchanged", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(8, 0xDE);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0108);
    cpu.exec_sts(encode_sts(8));

    REQUIRE(mem.read8(0x0108) == 0xDE);
    REQUIRE(mem.read8(0x0107) == 0x00);
    REQUIRE(mem.read8(0x0109) == 0x00);
}

// ---------------------------------------------------------------------------
// STS — PC advanced past the address word
// ---------------------------------------------------------------------------

TEST_CASE("STS - PC is advanced by 2 after reading the address word", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_sts(encode_sts(5));

    REQUIRE(cpu.pc() == 4u);
}

// ---------------------------------------------------------------------------
// STS — flags not affected
// ---------------------------------------------------------------------------

TEST_CASE("STS - flags not affected", "[sts][data_transfer]")
{
    auto cfg = make_lds_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b10110101;
    cpu.set_sreg(sentinel);
    cpu.set_reg(4, 0xFF);
    cpu.set_pc(2);
    lds_flash_write16(mem.flash(), 2, 0x0100);
    cpu.exec_sts(encode_sts(4));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// LPM / LPM Z / LPM Z+ — shared infrastructure
//
// AVR docs:
//   LPM        : R0  ← flash[Z]  (Z unchanged)        opcode 0x95C8 (fixed)
//   LPM Z      : Rd  ← flash[Z]  (Z unchanged)        1001 000d dddd 0100
//   LPM Z+     : Rd  ← flash[Z], Z ← Z+1              1001 000d dddd 0101
//
// Z is the 16-bit pointer formed by ZH:ZL = R31:R30.
// The byte address in flash is the Z value directly.
// Flags: none affected by any LPM variant.
// ---------------------------------------------------------------------------

static DeviceConfig make_lpm_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 32 * 1024;
    c.sram_size_bytes  = 2  * 1024;
    c.sram_base        = 0x0100;
    c.z_low_reg        = 30;   // ZL = R30
    c.z_high_reg       = 31;   // ZH = R31
    return c;
}

// Write a byte into flash at a given byte address.
static void flash_write8(std::vector<u8>& flash, u32 byte_addr, u8 value)
{
    flash[byte_addr] = value;
}

// Encode LPM Z  (1001 000d dddd 0100)
static u16 encode_lpm_z(u8 d)
{
    return static_cast<u16>(0x9004 | ((d & 0x1F) << 4));
}

// Encode LPM Z+  (1001 000d dddd 0101)
static u16 encode_lpm_z_post_inc(u8 d)
{
    return static_cast<u16>(0x9005 | ((d & 0x1F) << 4));
}

// ---------------------------------------------------------------------------
// LPM  (1001 0101 1100 1000)
//
// Always loads into R0. Z is not modified.
// ---------------------------------------------------------------------------

TEST_CASE("LPM - byte at flash[Z] is loaded into R0", "[lpm][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0040, 0xAB);
    cpu.set_z(0x0040);
    cpu.exec_lpm(0x95C8);

    REQUIRE(cpu.reg(0) == 0xAB);
}

TEST_CASE("LPM - zero byte at flash[Z] loads zero into R0", "[lpm][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0xFF); // pre-fill
    flash_write8(mem.flash(), 0x0010, 0x00);
    cpu.set_z(0x0010);
    cpu.exec_lpm(0x95C8);

    REQUIRE(cpu.reg(0) == 0x00);
}

TEST_CASE("LPM - 0xFF byte at flash[Z] is loaded correctly", "[lpm][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0020, 0xFF);
    cpu.set_z(0x0020);
    cpu.exec_lpm(0x95C8);

    REQUIRE(cpu.reg(0) == 0xFF);
}

TEST_CASE("LPM - correct flash byte address used (Z low byte)", "[lpm][data_transfer]")
{
    // Two adjacent bytes — Z selects the correct one via its low byte.
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0030, 0x11);
    flash_write8(mem.flash(), 0x0031, 0x22);
    cpu.set_z(0x0031);
    cpu.exec_lpm(0x95C8);

    REQUIRE(cpu.reg(0) == 0x22u);
}

// ---------------------------------------------------------------------------
// PUSH  (1001 001d dddd 1111)
//
// Decrements SP by 1, then writes Rd to the new top of stack.
// SP_after = SP_before - 1
// mem[SP_after] = Rd
// Cycles: 1 (note: real AVR is 2, but this codebase returns 1; see impl)
// Flags: none affected
//
// Config: make_sreg_config() has sram_size=2*1024, sram_base=0x0100.
// Default SP = sram_size - 1 = 2047 = 0x07FF.
// ---------------------------------------------------------------------------

static constexpr u16 PUSH_INITIAL_SP = 0x07FFu; // sram_size_bytes(2048) - 1

static u16 encode_push(u8 d)
{
    // 1001 001d dddd 1111
    return static_cast<u16>(0x920F | ((d & 0x1F) << 4));
}

TEST_CASE("PUSH - register value written to stack", "[push][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(5, 0xAB);
    cpu.exec_push(encode_push(5));

    REQUIRE(mem.read8(PUSH_INITIAL_SP - 1) == 0xABu);
}

TEST_CASE("PUSH - SP decremented by 1", "[push][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(10, 0x00);
    cpu.exec_push(encode_push(10));

    REQUIRE(cpu.sp() == PUSH_INITIAL_SP - 1);
}

TEST_CASE("PUSH - zero value pushed correctly", "[push][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // R0 is already 0 after reset
    cpu.exec_push(encode_push(0));

    REQUIRE(mem.read8(PUSH_INITIAL_SP - 1) == 0x00u);
}

TEST_CASE("PUSH - 0xFF value pushed correctly", "[push][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(16, 0xFF);
    cpu.exec_push(encode_push(16));

    REQUIRE(mem.read8(PUSH_INITIAL_SP - 1) == 0xFFu);
}

TEST_CASE("PUSH - upper register (R31) pushed correctly", "[push][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(31, 0x77);
    cpu.exec_push(encode_push(31));

    REQUIRE(mem.read8(PUSH_INITIAL_SP - 1) == 0x77u);
}

TEST_CASE("PUSH - source register unchanged after push", "[push][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(8, 0x55);
    cpu.exec_push(encode_push(8));

    REQUIRE(cpu.reg(8) == 0x55u);
}

TEST_CASE("PUSH - two consecutive pushes stack in LIFO order", "[push][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_reg(0, 0x11);
    cpu.set_reg(1, 0x22);
    cpu.exec_push(encode_push(0)); // pushed first, ends up deeper
    cpu.exec_push(encode_push(1)); // pushed second, at top

    REQUIRE(mem.read8(PUSH_INITIAL_SP - 1) == 0x11u); // R0 deeper
    REQUIRE(mem.read8(PUSH_INITIAL_SP - 2) == 0x22u); // R1 on top
    REQUIRE(cpu.sp() == PUSH_INITIAL_SP - 2);
}

TEST_CASE("PUSH - flags unaffected", "[push][data_transfer]")
{
    auto cfg = make_sreg_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b10110101;
    cpu.set_sreg(sentinel);
    cpu.set_reg(3, 0x42);
    cpu.exec_push(encode_push(3));

    REQUIRE(cpu.sreg() == sentinel);
}

TEST_CASE("LPM - Z is not modified", "[lpm][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_z(0x0050);
    cpu.exec_lpm(0x95C8);

    REQUIRE(cpu.z() == 0x0050);
}

TEST_CASE("LPM - destination is always R0, adjacent registers unchanged", "[lpm][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(1, 0x77); // decoy
    flash_write8(mem.flash(), 0x0010, 0xAA);
    cpu.set_z(0x0010);
    cpu.exec_lpm(0x95C8);

    REQUIRE(cpu.reg(0) == 0xAA);
    REQUIRE(cpu.reg(1) == 0x77); // unchanged
}

TEST_CASE("LPM - flags not affected", "[lpm][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    constexpr u8 sentinel = 0b10110101;
    cpu.set_sreg(sentinel);
    flash_write8(mem.flash(), 0x0010, 0xFF);
    cpu.set_z(0x0010);
    cpu.exec_lpm(0x95C8);

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// LPM Z  (1001 000d dddd 0100)
//
// Loads flash[Z] into Rd. Z is not modified.
// ---------------------------------------------------------------------------

TEST_CASE("LPM Z - byte at flash[Z] is loaded into Rd", "[lpm_z][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0040, 0xAB);
    cpu.set_z(0x0040);
    cpu.exec_lpm_z(encode_lpm_z(5));

    REQUIRE(cpu.reg(5) == 0xAB);
}

TEST_CASE("LPM Z - zero byte at flash[Z] loads zero into Rd", "[lpm_z][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(3, 0xFF); // pre-fill
    cpu.set_z(0x0010);
    cpu.exec_lpm_z(encode_lpm_z(3));

    REQUIRE(cpu.reg(3) == 0x00);
}

TEST_CASE("LPM Z - correct destination register selected (R0)", "[lpm_z][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0010, 0x11);
    cpu.set_reg(1, 0x22); // decoy
    cpu.set_z(0x0010);
    cpu.exec_lpm_z(encode_lpm_z(0));

    REQUIRE(cpu.reg(0) == 0x11);
    REQUIRE(cpu.reg(1) == 0x22);
}

TEST_CASE("LPM Z - correct destination register selected (R29)", "[lpm_z][data_transfer]")
{
    // R29 is adjacent to ZH (R31) and ZL (R30) — good boundary check.
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0010, 0xCC);
    cpu.set_reg(28, 0xFF); // decoy
    cpu.set_z(0x0010);
    cpu.exec_lpm_z(encode_lpm_z(29));

    REQUIRE(cpu.reg(29) == 0xCC);
    REQUIRE(cpu.reg(28) == 0xFF);
}

TEST_CASE("LPM Z - correct flash address used (Z selects the byte)", "[lpm_z][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0060, 0x55);
    flash_write8(mem.flash(), 0x0061, 0x66);
    cpu.set_z(0x0061);
    cpu.exec_lpm_z(encode_lpm_z(7));

    REQUIRE(cpu.reg(7) == 0x66);
}

TEST_CASE("LPM Z - Z is not modified", "[lpm_z][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_z(0x0050);
    cpu.exec_lpm_z(encode_lpm_z(5));

    REQUIRE(cpu.z() == 0x0050);
}

TEST_CASE("LPM Z - flags not affected", "[lpm_z][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    constexpr u8 sentinel = 0b11001010;
    cpu.set_sreg(sentinel);
    flash_write8(mem.flash(), 0x0010, 0xFF);
    cpu.set_z(0x0010);
    cpu.exec_lpm_z(encode_lpm_z(4));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// LPM Z+  (1001 000d dddd 0101)
//
// Loads flash[Z] into Rd, then increments Z by 1.
// ---------------------------------------------------------------------------

TEST_CASE("LPM Z+ - byte at flash[Z] is loaded into Rd", "[lpm_z_post_inc][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0040, 0xAB);
    cpu.set_z(0x0040);
    cpu.exec_lpm_z_post_inc(encode_lpm_z_post_inc(5));

    REQUIRE(cpu.reg(5) == 0xAB);
}

TEST_CASE("LPM Z+ - Z is incremented by 1 after load", "[lpm_z_post_inc][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_z(0x0040);
    cpu.exec_lpm_z_post_inc(encode_lpm_z_post_inc(5));

    REQUIRE(cpu.z() == 0x0041);
}

TEST_CASE("LPM Z+ - load happens before increment (reads original Z address)", "[lpm_z_post_inc][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0040, 0xAA); // original Z
    flash_write8(mem.flash(), 0x0041, 0xBB); // post-increment address
    cpu.set_z(0x0040);
    cpu.exec_lpm_z_post_inc(encode_lpm_z_post_inc(5));

    REQUIRE(cpu.reg(5) == 0xAA); // loaded from original, not incremented
    REQUIRE(cpu.z()    == 0x0041);
}

TEST_CASE("LPM Z+ - Z wraps from 0xFFFF to 0x0000", "[lpm_z_post_inc][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_z(0xFFFF);
    cpu.exec_lpm_z_post_inc(encode_lpm_z_post_inc(5));

    REQUIRE(cpu.z() == 0x0000);
}

TEST_CASE("LPM Z+ - correct destination register selected (R0)", "[lpm_z_post_inc][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0010, 0x11);
    cpu.set_reg(1, 0x22); // decoy
    cpu.set_z(0x0010);
    cpu.exec_lpm_z_post_inc(encode_lpm_z_post_inc(0));

    REQUIRE(cpu.reg(0) == 0x11);
    REQUIRE(cpu.reg(1) == 0x22);
}

TEST_CASE("LPM Z+ - correct destination register selected (R29)", "[lpm_z_post_inc][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0010, 0xCC);
    cpu.set_reg(28, 0xFF); // decoy
    cpu.set_z(0x0010);
    cpu.exec_lpm_z_post_inc(encode_lpm_z_post_inc(29));

    REQUIRE(cpu.reg(29) == 0xCC);
    REQUIRE(cpu.reg(28) == 0xFF);
}

TEST_CASE("LPM Z+ - only target register written, adjacent unchanged", "[lpm_z_post_inc][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    flash_write8(mem.flash(), 0x0010, 0xDE);
    cpu.set_reg(8, 0x00);
    cpu.set_reg(9, 0x77); // decoy above
    cpu.set_reg(7, 0x88); // decoy below
    cpu.set_z(0x0010);
    cpu.exec_lpm_z_post_inc(encode_lpm_z_post_inc(8));

    REQUIRE(cpu.reg(8) == 0xDE);
    REQUIRE(cpu.reg(9) == 0x77);
    REQUIRE(cpu.reg(7) == 0x88);
}

TEST_CASE("LPM Z+ - flags not affected", "[lpm_z_post_inc][data_transfer]")
{
    auto cfg = make_lpm_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    constexpr u8 sentinel = 0b01011010;
    cpu.set_sreg(sentinel);
    flash_write8(mem.flash(), 0x0010, 0xFF);
    cpu.set_z(0x0010);
    cpu.exec_lpm_z_post_inc(encode_lpm_z_post_inc(4));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// LD X / LD X+ / LD -X
// 1001 000d dddd 1100  (LD X)
// 1001 000d dddd 1101  (LD X+)
// 1001 000d dddd 1110  (LD -X)
// ---------------------------------------------------------------------------

// Encode LD X variants
static u16 encode_ld_x(u8 d)
{
    return static_cast<u16>(0x900C | ((d & 0x1F) << 4));
}
static u16 encode_ld_x_post_inc(u8 d)
{
    return static_cast<u16>(0x900D | ((d & 0x1F) << 4));
}
static u16 encode_ld_x_pre_dec(u8 d)
{
    return static_cast<u16>(0x900E | ((d & 0x1F) << 4));
}

TEST_CASE("LD X - loads Rr from memory at X address", "[ld_x][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // X = 0x0100
    cpu.set_reg(26, 0x00);
    cpu.set_reg(27, 0x01);

    mem.write8(0x0100, 0xAB);
    cpu.exec_ld_x(encode_ld_x(5)); // LD R5, X

    REQUIRE(cpu.reg(5) == 0xAB);
}

TEST_CASE("LD X+ - loads then increments X", "[ld_x_post_inc][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // X = 0x0105
    cpu.set_reg(26, 0x05);
    cpu.set_reg(27, 0x01);

    mem.write8(0x0105, 0xFE);
    cpu.exec_ld_x_post_inc(encode_ld_x_post_inc(3)); // LD R3, X+

    REQUIRE(cpu.reg(3) == 0xFE);
    REQUIRE(cpu.x() == 0x0106);
}

TEST_CASE("LD -X - decrements X then loads", "[ld_x_pre_dec][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // X initially = 0x0105 -> pre-decrement to 0x0104
    cpu.set_reg(26, 0x05);
    cpu.set_reg(27, 0x01);

    mem.write8(0x0104, 0x11);
    cpu.exec_ld_x_pre_dec(encode_ld_x_pre_dec(7)); // LD R7, -X

    REQUIRE(cpu.reg(7) == 0x11);
    REQUIRE(cpu.x() == 0x0104);
}

// ---------------------------------------------------------------------------
// MOVW (0000 0001 dddd rrrr)
//
// Copies register pair Rr:Rr+1 into Rd:Rd+1 where d and r are even.
// Flags: none affected.
// ---------------------------------------------------------------------------

// Encode MOVW Rd, Rr
static u16 encode_movw(u8 d, u8 r)
{
   return static_cast<u16>(
        0x0100 | (((d >> 1) & 0x0F) << 4) | ((r >> 1) & 0x0F)
    );
}

TEST_CASE("MOVW - copies two registers (non-overlapping)", "[movw][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // Source pair R24:R25 = 0x12,0x34
    cpu.set_reg(24, 0x12);
    cpu.set_reg(25, 0x34);

    // Ensure destination pair R22:R23 are zero initially
    REQUIRE(cpu.reg(22) == 0x00);
    REQUIRE(cpu.reg(23) == 0x00);

    cpu.exec_movw(encode_movw(22, 24)); // MOVW R22, R24

    REQUIRE(cpu.reg(22) == 0x12);
    REQUIRE(cpu.reg(23) == 0x34);

    // Source registers must remain unchanged
    REQUIRE(cpu.reg(24) == 0x12);
    REQUIRE(cpu.reg(25) == 0x34);
}

TEST_CASE("MOVW - copies pair at lower registers (R0..R1 -> R8..R9)", "[movw][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0xAA);
    cpu.set_reg(1, 0xBB);

    cpu.exec_movw(encode_movw(8, 0)); // MOVW R8, R0

    REQUIRE(cpu.reg(8) == 0xAA);
    REQUIRE(cpu.reg(9) == 0xBB);

    // Verify adjacent registers not modified
    REQUIRE(cpu.reg(7) == 0x00);
    REQUIRE(cpu.reg(10) == 0x00);
}

// ---------------------------------------------------------------------------
// MOV  (0010 11rd dddd rrrr)
//
// Copies Rr into Rd. Flags not affected.
// ---------------------------------------------------------------------------

// Encode MOV Rd, Rr
static u16 encode_mov(u8 d, u8 r)
{
    return static_cast<u16>(0x2C00
        | ((r & 0x10) << 5)   // r[4] -> bit 9
        | ((d & 0x1F) << 4)   // d[4:0] -> bits [8:4]
        | (r & 0x0F));        // r[3:0] -> bits [3:0]
}

TEST_CASE("MOV - copies Rr to Rd (low registers)", "[mov][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(2, 0xAB);
    cpu.set_reg(3, 0xFF); // decoy
    cpu.exec_mov(encode_mov(1, 2)); // MOV R1, R2

    REQUIRE(cpu.reg(1) == 0xAB);
    REQUIRE(cpu.reg(2) == 0xAB); // source unchanged
    REQUIRE(cpu.reg(3) == 0xFF); // decoy unchanged
}

TEST_CASE("MOV - copies Rr to Rd (high registers)", "[mov][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(21, 0xCD);
    cpu.exec_mov(encode_mov(30, 21)); // MOV R30, R21

    REQUIRE(cpu.reg(30) == 0xCD);
    REQUIRE(cpu.reg(21) == 0xCD); // source unchanged
}

TEST_CASE("MOV - self-move leaves register unchanged", "[mov][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(5, 0x77);
    cpu.exec_mov(encode_mov(5, 5)); // MOV R5, R5

    REQUIRE(cpu.reg(5) == 0x77);
}

TEST_CASE("MOV - only destination modified, adjacent registers unchanged", "[mov][data_transfer]")
{
    auto cfg = make_io_in_sram_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(9, 0x11);  // source
    cpu.set_reg(10, 0x00); // destination initially zero
    cpu.set_reg(11, 0x22); // decoy above

    cpu.exec_mov(encode_mov(10, 9)); // MOV R10, R9

    REQUIRE(cpu.reg(10) == 0x11);
    REQUIRE(cpu.reg(9)  == 0x11); // source unchanged
    REQUIRE(cpu.reg(11) == 0x22); // adjacent unchanged
}
