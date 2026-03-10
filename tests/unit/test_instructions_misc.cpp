#include <catch2/catch_test_macros.hpp>

#include "cpu/avr_cpu.h"
#include "device/device_config.h"
#include "memory/memory_map.h"

using namespace avrion;

static constexpr u8 SREG_I = 0x80;

static DeviceConfig make_min_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 1024;
    c.io_base          = 0x0100;
    c.sram_base        = 0x0100;
    c.sram_size_bytes  = 256;
    return c;
}

// ---------------------------------------------------------------------------
// SEI (1001 0100 0111 1000)
//
// Operation : I ← 1  (sets the global interrupt enable bit in SREG)
// Returns   : 1 (clock cycle)
// Other bits: unaffected
// ---------------------------------------------------------------------------

TEST_CASE("SEI - sets I bit in SREG", "[sei][misc]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // I clear
    cpu.exec_sei(0x9478);

    REQUIRE((cpu.sreg() & SREG_I) != 0);
}

TEST_CASE("SEI - preserves other SREG bits", "[sei][misc]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x7F); // all bits except I set
    cpu.exec_sei(0x9478);

    REQUIRE(cpu.sreg() == 0xFF); // all bits now set
}

TEST_CASE("SEI - idempotent when I already set", "[sei][misc]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x80);
    cpu.exec_sei(0x9478);

    REQUIRE(cpu.sreg() == 0x80);
}

// ---------------------------------------------------------------------------
// CLI (1001 0100 1111 1000)
//
// Operation : I ← 0  (clears the global interrupt enable bit in SREG)
// Returns   : 1 (clock cycle)
// Other bits: unaffected
// ---------------------------------------------------------------------------

TEST_CASE("CLI - clears I bit in SREG", "[cli][misc]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x80); // I set
    cpu.exec_cli(0x94F8);

    REQUIRE((cpu.sreg() & SREG_I) == 0);
}

TEST_CASE("CLI - preserves other SREG bits", "[cli][misc]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // all bits set
    cpu.exec_cli(0x94F8);

    REQUIRE(cpu.sreg() == static_cast<u8>(0xFF & ~SREG_I));
}

TEST_CASE("CLI - idempotent when I already cleared", "[cli][misc]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00);
    cpu.exec_cli(0x94F8);

    REQUIRE(cpu.sreg() == 0x00);
}
