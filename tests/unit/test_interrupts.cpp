#include <catch2/catch_test_macros.hpp>

#include "cpu/avr_cpu.h"
#include "core/interrupt_controller.h"
#include "device/device_config.h"
#include "memory/memory_map.h"

using namespace avrion;

static constexpr u8 SREG_I = 0x80;

static DeviceConfig make_min_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 1024;
    c.sram_base = 0x0100;
    c.sram_size_bytes = 256;
    return c;
}

// ---------------------------------------------------------------------------
// RETI (1001 0101 0001 1000 = 0x9518)
// ---------------------------------------------------------------------------

TEST_CASE("RETI - pops PC and sets I flag", "[reti][misc]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    u16 sp_top = cfg.sram_base + cfg.sram_size_bytes - 1; // 0x01FF

    // Simulate stack after CALL pushed return address 0x0042:
    // CALL does: write(--sp, hi); write(--sp, lo);
    // So: addr sp_top-1 = hi byte (0x00), addr sp_top-2 = lo byte (0x42)
    // SP left pointing at sp_top-2
    u32 ret_addr = 0x0042;
    mem.write8(sp_top - 1, (ret_addr >> 8) & 0xFF); // hi = 0x00
    mem.write8(sp_top - 2, ret_addr & 0xFF);         // lo = 0x42
    cpu.set_sp(sp_top - 2);

    // RETI: lo = read(sp++); hi = read(sp++);
    // reads sp_top-2 (lo=0x42), then sp_top-1 (hi=0x00) → PC = 0x0042
    cpu.set_sreg(0x00); // I cleared (as if in ISR)

    u8 cycles = cpu.exec_reti(0x9518);

    REQUIRE(cpu.pc() == 0x0042);
    REQUIRE((cpu.sreg() & SREG_I) != 0); // I re-enabled
    REQUIRE(cycles == 4);
}

TEST_CASE("RETI - preserves other SREG bits", "[reti][misc]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    u16 sp_top = cfg.sram_base + cfg.sram_size_bytes - 1;
    mem.write8(sp_top - 1, 0x00);
    mem.write8(sp_top - 2, 0x00);
    cpu.set_sp(sp_top - 2);
    cpu.set_sreg(0x3F); // some flags set, I cleared

    cpu.exec_reti(0x9518);

    REQUIRE(cpu.sreg() == 0xBF); // I set, others preserved
}

// ---------------------------------------------------------------------------
// Interrupt servicing: CPU should jump to vector when IRQ pending + I set
// ---------------------------------------------------------------------------

TEST_CASE("CPU services interrupt when I flag set and IRQ pending", "[interrupt]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};
    mem.attach_cpu(&cpu);
    InterruptController irq;
    cpu.set_irq_controller(&irq);

    // Set SP to top of SRAM
    u16 sp_top = cfg.sram_base + cfg.sram_size_bytes - 1;
    cpu.set_sp(sp_top);

    // Put a NOP at the interrupt vector (vector 16 → byte addr 32 = 0x20)
    u16 vector_addr = 16 * 2; // 0x20
    auto& flash = mem.flash();
    flash[vector_addr] = 0x00;     // NOP low byte
    flash[vector_addr + 1] = 0x00; // NOP high byte

    // Also put a NOP at PC=0 so step doesn't crash if no IRQ
    flash[0] = 0x00;
    flash[1] = 0x00;

    cpu.set_pc(0x0010); // some arbitrary PC
    cpu.set_sreg(SREG_I); // enable interrupts

    irq.raise(16); // raise Timer0 overflow interrupt

    u32 cycles = cpu.step_instruction();

    // Should have jumped to vector
    REQUIRE(cpu.pc() == vector_addr);
    // I flag should be cleared
    REQUIRE((cpu.sreg() & SREG_I) == 0);
    // SP should have decreased by 2 (return address pushed)
    REQUIRE(cpu.sp() == sp_top - 2);
    // Cycles for interrupt entry
    REQUIRE(cycles == 4);
}

TEST_CASE("CPU does not service interrupt when I flag cleared", "[interrupt]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};
    mem.attach_cpu(&cpu);
    InterruptController irq;
    cpu.set_irq_controller(&irq);

    auto& flash = mem.flash();
    flash[0] = 0x00; flash[1] = 0x00; // NOP at 0

    cpu.set_pc(0x0000);
    cpu.set_sreg(0x00); // I cleared

    irq.raise(16);

    cpu.step_instruction(); // should execute NOP, not jump to vector

    REQUIRE(cpu.pc() == 0x0002); // PC advanced past NOP
    REQUIRE(irq.is_pending(16)); // IRQ still pending
}

TEST_CASE("CPU does not service interrupt without controller", "[interrupt]")
{
    auto cfg = make_min_config();
    MemoryMap mem{cfg};
    AvrCpu cpu{mem, cfg};
    mem.attach_cpu(&cpu);
    // No IRQ controller attached

    auto& flash = mem.flash();
    flash[0] = 0x00; flash[1] = 0x00; // NOP

    cpu.set_pc(0x0000);
    cpu.set_sreg(SREG_I);

    cpu.step_instruction();

    REQUIRE(cpu.pc() == 0x0002); // normal execution
}
