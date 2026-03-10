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

// ---------------------------------------------------------------------------
// BRNE  (1111 01kk kkkk k001)
//
// Relative branch: PC = PC + k*2, where k is a signed 7-bit word offset.
// The exec model advances PC past the opcode before calling exec_brne,
// so PC on entry is already (instruction_address + 2).
// The Z-flag guard is handled at a higher level; exec_brne always applies
// the offset unconditionally.
// Flags: none affected.
// ---------------------------------------------------------------------------

// Encode BRNE with a signed 7-bit word offset k.
static u16 encode_brne(int8_t k)
{
    // 1111 01kk kkkk k001  — k occupies bits 9:3
    return static_cast<u16>(0xF401 | ((static_cast<u16>(k) & 0x7F) << 3));
}

TEST_CASE("BRNE - forward branch (positive offset)", "[brne]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // PC already past opcode; k=+3 words → +6 bytes
    cpu.set_pc(20);
    cpu.exec_brne(encode_brne(3));

    REQUIRE(cpu.pc() == 26u);
}

TEST_CASE("BRNE - backward branch (negative offset)", "[brne]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // PC=100, k=-5 → target = 100 + (-5)*2 = 90
    cpu.set_pc(100);
    cpu.exec_brne(encode_brne(-5));

    REQUIRE(cpu.pc() == 90u);
}

TEST_CASE("BRNE - zero offset stays at current PC", "[brne]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(50);
    cpu.exec_brne(encode_brne(0));

    REQUIRE(cpu.pc() == 50u);
}

TEST_CASE("BRNE - self-loop (k=-1 jumps back to own address)", "[brne]")
{
    // Instruction at byte 0; PC is already 2 when exec fires.
    // k=-1 → target = 2 + (-1)*2 = 0
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(2);
    cpu.exec_brne(encode_brne(-1));

    REQUIRE(cpu.pc() == 0u);
}

TEST_CASE("BRNE - max positive offset (k=+63)", "[brne]")
{
    // k=63 → offset_bytes = +126
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(100);
    cpu.exec_brne(encode_brne(63));

    REQUIRE(cpu.pc() == 226u);
}

TEST_CASE("BRNE - max negative offset (k=-64)", "[brne]")
{
    // k=-64 → offset_bytes = -128; start at 228 so target = 100
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(228);
    cpu.exec_brne(encode_brne(-64));

    REQUIRE(cpu.pc() == 100u);
}

TEST_CASE("BRNE - real blink.hex encoding (0xF7E1, k=-4 words, target = PC-8)", "[brne]")
{
    // At byte 0xD2: opcode 0xF7E1, PC advanced to 0xD4 before exec.
    // k = (0xF7E1 & 0x03F8) >> 3 = 0x7C; sign-extended = -4 words → -8 bytes.
    // Target = 0xD4 - 8 = 0xCC.
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_pc(0xD4);
    cpu.exec_brne(0xF7E1);

    REQUIRE(cpu.pc() == 0xCCu);
}

TEST_CASE("BRNE - flags unaffected", "[brne]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    constexpr u8 sentinel = 0b11001010;
    cpu.set_sreg(sentinel);
    cpu.set_pc(20);
    cpu.exec_brne(encode_brne(3));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// BRNE — Z-flag guard tests
//
// BRNE branches only when Z==0.  When Z==1 (equal), PC must not change.
// ---------------------------------------------------------------------------

TEST_CASE("BRNE - Z=0: branch is taken", "[brne]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // Z=0 — branch taken
    cpu.set_pc(20);
    cpu.exec_brne(encode_brne(3)); // +6 bytes

    REQUIRE(cpu.pc() == 26u);
}

TEST_CASE("BRNE - Z=1: branch is not taken, PC unchanged", "[brne]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x02); // Z=1 (bit 1) — branch not taken
    cpu.set_pc(20);
    cpu.exec_brne(encode_brne(3));

    REQUIRE(cpu.pc() == 20u);
}

TEST_CASE("BRNE - Z=1: negative offset also not taken", "[brne]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x02); // Z=1
    cpu.set_pc(100);
    cpu.exec_brne(encode_brne(-5));

    REQUIRE(cpu.pc() == 100u);
}

TEST_CASE("BRNE - Z=1 with other SREG bits set: branch still not taken", "[brne]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // all flags set, including Z
    cpu.set_pc(50);
    cpu.exec_brne(encode_brne(10));

    REQUIRE(cpu.pc() == 50u);
}

TEST_CASE("BRNE - only Z bit matters: other flags set but Z=0 still branches", "[brne]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFD); // all flags set except Z (bit 1 = 0)
    cpu.set_pc(20);
    cpu.exec_brne(encode_brne(3)); // +6 bytes

    REQUIRE(cpu.pc() == 26u);
}

// ---------------------------------------------------------------------------
// CALL  (1001 010k kkkk 111k  +  kkkk kkkk kkkk kkkk)
//
// Pushes ret = pc() + 4 onto the stack (low byte first, then high byte),
// then jumps to byte address k << 1.
// For 22-bit PC devices an extra upper byte is also pushed.
// Stack layout after a 16-bit-PC push:
//   [initial_SP]     = ret_lo
//   [initial_SP - 1] = ret_hi
//   SP_after = initial_SP - 1
// Flags: none affected.
// ---------------------------------------------------------------------------

// Encode the first word of CALL: k[21:17] → bits [8:4], k[16] → bit [0].
// Write the second word (k[15:0]) separately with flash_write16.
static u16 encode_call(u32 k)
{
    return static_cast<u16>(0x940E
        | ((k >> 13) & 0x01F0)   // k[21:17] → bits [8:4]
        | ((k >> 16) & 0x0001)); // k[16]    → bit  [0]
}

// Config with 256 KB flash for tests whose target address exceeds 32 KB.
static DeviceConfig make_large_flash_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 256 * 1024;
    c.sram_size_bytes  = 2 * 1024;
    c.sram_base        = 0x0100;
    return c;
}

static DeviceConfig make_22bit_pc_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 256 * 1024;
    c.sram_size_bytes  = 2 * 1024;
    c.sram_base        = 0x0100;
    c.has_22_bit_pc    = true;
    return c;
}

// Initial SP for both make_test_config() and make_large_flash_config():
//   st_.sp = sram_size_bytes - 1 = 2047 = 0x07FF
static constexpr u16 INITIAL_SP = 0x07FFu;

TEST_CASE("CALL - PC jumps to decoded target (k in second word)", "[call]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // PC already past first word (mimics step_instruction advancing by 2).
    // Second word = 0x0050 → k = 0x0050 → target byte addr = 0x00A0.
    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x0050);

    cpu.exec_call(encode_call(0));

    REQUIRE(cpu.pc() == 0x00A0u);
}

TEST_CASE("CALL - return address low byte pushed to stack", "[call]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // PC = 0x0100 → ret = pc() + 4 = 0x0104.
    // Low byte = 0x04 should land at initial SP (0x07FF).
    cpu.set_pc(0x0100);
    flash_write16(mem.flash(), 0x0100, 0x0001); // target word — value doesn't matter here

    cpu.exec_call(encode_call(0));

    REQUIRE(mem.read8(INITIAL_SP) == 0x04u);
}

TEST_CASE("CALL - return address high byte pushed to stack", "[call]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // PC = 0x0100 → ret = 0x0104. High byte = 0x01 should land at initial SP - 1.
    cpu.set_pc(0x0100);
    flash_write16(mem.flash(), 0x0100, 0x0001);

    cpu.exec_call(encode_call(0));

    REQUIRE(mem.read8(INITIAL_SP - 1) == 0x01u);
}

TEST_CASE("CALL - SP decremented by 1 after 16-bit-PC push", "[call]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x0001);

    cpu.exec_call(encode_call(0));

    REQUIRE(cpu.sp() == INITIAL_SP - 1);
}

TEST_CASE("CALL - return address uses pc() at call time, not original instruction address", "[call]")
{
    // With PC = 0x0200 at call time, ret = 0x0204 (pc() + 4).
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(0x0200);
    flash_write16(mem.flash(), 0x0200, 0x0001);

    cpu.exec_call(encode_call(0));

    // Low = 0x04, high = 0x02
    REQUIRE(mem.read8(INITIAL_SP)     == 0x04u);
    REQUIRE(mem.read8(INITIAL_SP - 1) == 0x02u);
}

TEST_CASE("CALL - k[16] bit decoded from opcode bit 0", "[call]")
{
    // k = 0x10000, second word = 0x0000 → target byte = 0x20000.
    auto cfg = make_large_flash_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x0000); // k[15:0] = 0

    cpu.exec_call(encode_call(0x10000));   // k[16] = 1 → opcode bit[0] = 1

    REQUIRE(cpu.pc() == 0x20000u);
}

TEST_CASE("CALL - k[21:17] bits decoded from opcode bits [8:4]", "[call]")
{
    // k = 0x1E0000 (bits [20:17] all set), second word = 0x0000 → target byte = 0x3C0000.
    auto cfg = make_large_flash_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x0000);

    cpu.exec_call(encode_call(0x1E0000)); // k[20:17] = 0b1111 → opcode bits [8:5] = 0b1111

    REQUIRE(cpu.pc() == 0x3C0000u);
}

TEST_CASE("CALL - second word and first-word k bits combined into target", "[call]")
{
    // k = 0x10050 (bit[16]=1, low word=0x0050) → target = 0x200A0.
    auto cfg = make_large_flash_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x0050);   // k[15:0]

    cpu.exec_call(encode_call(0x10050));      // k[16] = 1

    REQUIRE(cpu.pc() == 0x200A0u);
}

TEST_CASE("CALL - second word is read relative to PC at call time", "[call]")
{
    // Second word must be fetched from pc = 6, NOT from 2.
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(6);
    flash_write16(mem.flash(), 6, 0x0050); // correct slot
    flash_write16(mem.flash(), 2, 0xFFFF); // wrong slot — must not be used

    cpu.exec_call(encode_call(0));

    REQUIRE(cpu.pc() == 0x00A0u);
}

TEST_CASE("CALL - 22-bit PC: upper ret byte pushed to stack", "[call][22bit]")
{
    // PC = 0x0100 → ret = 0x0104. With 22-bit PC, byte 2 of ret (0x00) is
    // also pushed: stack = [..., 0x00, 0x01, 0x04] at SP, SP-1, SP-2.
    auto cfg = make_22bit_pc_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(0x0100);
    flash_write16(mem.flash(), 0x0100, 0x0001);

    cpu.exec_call(encode_call(0));

    REQUIRE(mem.read8(INITIAL_SP)     == 0x04u); // ret_lo
    REQUIRE(mem.read8(INITIAL_SP - 1) == 0x01u); // ret_hi
    REQUIRE(mem.read8(INITIAL_SP - 2) == 0x00u); // ret byte 2
}

TEST_CASE("CALL - 22-bit PC: SP decremented by 2", "[call][22bit]")
{
    auto cfg = make_22bit_pc_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x0001);

    cpu.exec_call(encode_call(0));

    REQUIRE(cpu.sp() == INITIAL_SP - 2);
}

TEST_CASE("CALL - flags not affected", "[call]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b10110101;
    cpu.set_sreg(sentinel);
    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x0001);

    cpu.exec_call(encode_call(0));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// CPSE  (0001 00rd dddd rrrr)
//
// Compare Rd and Rr; if equal, skip the next instruction.
//   Rd != Rr: PC unchanged,   return 1 cycle.
//   Rd == Rr, next is single-word: PC += 2, return 2 cycles.
//   Rd == Rr, next is two-word:    PC += 4, return 3 cycles.
// Flags: none affected.
// ---------------------------------------------------------------------------

static u16 encode_cpse(u8 d, u8 r)
{
    // 0001 00rd dddd rrrr
    return static_cast<u16>(0x1000
        | ((r & 0x10) << 5)   // r[4] → bit 9
        | ((d & 0x1F) << 4)   // d[4:0] → bits [8:4]
        | (r & 0x0F));        // r[3:0] → bits [3:0]
}

TEST_CASE("CPSE - not equal: PC unchanged, 1 cycle", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(2, 0x10);
    cpu.set_reg(3, 0x20);
    cpu.set_pc(100);

    u8 cycles = cpu.exec_cpse(encode_cpse(2, 3));

    REQUIRE(cpu.pc() == 100u);
    REQUIRE(cycles == 1);
}

TEST_CASE("CPSE - equal, unknown next opcode (single-word): PC += 2, 2 cycles", "[cpse]")
{
    // An unrecognised opcode is treated as single-word (is_two_word == false).
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(4, 0xAB);
    cpu.set_reg(5, 0xAB);
    cpu.set_pc(100);
    flash_write16(mem.flash(), 100, 0x0000); // NOP — single-word

    u8 cycles = cpu.exec_cpse(encode_cpse(4, 5));

    REQUIRE(cpu.pc() == 102u);
    REQUIRE(cycles == 2);
}

TEST_CASE("CPSE - equal, JMP next (two-word): PC += 4, 3 cycles", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(0, 0x55);
    cpu.set_reg(1, 0x55);
    cpu.set_pc(100);
    flash_write16(mem.flash(), 100, 0x940C); // JMP — two-word

    u8 cycles = cpu.exec_cpse(encode_cpse(0, 1));

    REQUIRE(cpu.pc() == 104u);
    REQUIRE(cycles == 3);
}

TEST_CASE("CPSE - equal, CALL next (two-word): PC += 4, 3 cycles", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(10, 0xFF);
    cpu.set_reg(11, 0xFF);
    cpu.set_pc(50);
    flash_write16(mem.flash(), 50, 0x940E); // CALL — two-word

    u8 cycles = cpu.exec_cpse(encode_cpse(10, 11));

    REQUIRE(cpu.pc() == 54u);
    REQUIRE(cycles == 3);
}

TEST_CASE("CPSE - equal, LDS next (two-word): PC += 4, 3 cycles", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(6, 0x01);
    cpu.set_reg(7, 0x01);
    cpu.set_pc(200);
    flash_write16(mem.flash(), 200, 0x9000); // LDS — two-word

    u8 cycles = cpu.exec_cpse(encode_cpse(6, 7));

    REQUIRE(cpu.pc() == 204u);
    REQUIRE(cycles == 3);
}

TEST_CASE("CPSE - both registers zero: skip taken (single-word next)", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // r0 and r1 default to 0 after reset
    cpu.set_pc(20);
    flash_write16(mem.flash(), 20, 0x0000); // NOP

    u8 cycles = cpu.exec_cpse(encode_cpse(0, 1));

    REQUIRE(cpu.pc() == 22u);
    REQUIRE(cycles == 2);
}

TEST_CASE("CPSE - same register (d == r): always equal, skip taken", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(5, 0x42); // any value
    cpu.set_pc(60);
    flash_write16(mem.flash(), 60, 0x0000); // NOP

    u8 cycles = cpu.exec_cpse(encode_cpse(5, 5));

    REQUIRE(cpu.pc() == 62u);
    REQUIRE(cycles == 2);
}

TEST_CASE("CPSE - high register pair r28/r29 not equal: no skip", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(28, 0xAA);
    cpu.set_reg(29, 0xBB);
    cpu.set_pc(80);

    u8 cycles = cpu.exec_cpse(encode_cpse(28, 29));

    REQUIRE(cpu.pc() == 80u);
    REQUIRE(cycles == 1);
}

TEST_CASE("CPSE - high register pair r30/r31 equal: skip taken", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(30, 0x7F);
    cpu.set_reg(31, 0x7F);
    cpu.set_pc(40);
    flash_write16(mem.flash(), 40, 0x0000); // NOP

    u8 cycles = cpu.exec_cpse(encode_cpse(30, 31));

    REQUIRE(cpu.pc() == 42u);
    REQUIRE(cycles == 2);
}

TEST_CASE("CPSE - flags unaffected when equal", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    constexpr u8 sentinel = 0b10110101;
    cpu.set_sreg(sentinel);
    cpu.set_reg(8, 0x33);
    cpu.set_reg(9, 0x33);
    cpu.set_pc(100);
    flash_write16(mem.flash(), 100, 0x0000);

    cpu.exec_cpse(encode_cpse(8, 9));

    REQUIRE(cpu.sreg() == sentinel);
}

TEST_CASE("CPSE - flags unaffected when not equal", "[cpse]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    constexpr u8 sentinel = 0b11001010;
    cpu.set_sreg(sentinel);
    cpu.set_reg(8, 0x11);
    cpu.set_reg(9, 0x22);
    cpu.set_pc(100);

    cpu.exec_cpse(encode_cpse(8, 9));

    REQUIRE(cpu.sreg() == sentinel);
}
