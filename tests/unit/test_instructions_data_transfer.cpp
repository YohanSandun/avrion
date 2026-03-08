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
