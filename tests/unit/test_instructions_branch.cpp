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

    // ---------------------------------------------------------------------------
    // BRCS  (1111 00kk kkkk k000)
    //
    // Branch if C set. Relative branch with signed 7-bit word offset k.
    // ---------------------------------------------------------------------------

    // Encode BRCS with a signed 7-bit word offset k.
    static u16 encode_brcs(int8_t k)
    {
        return static_cast<u16>(0xF000 | ((static_cast<u16>(k) & 0x7F) << 3));
    }

    TEST_CASE("BRCS - forward branch (positive offset)", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        // PC already past opcode; k=+3 words → +6 bytes
        cpu.set_pc(20);
        cpu.set_sreg(0x01); // C=1 so branch is taken
        cpu.exec_brcs(encode_brcs(3));

        REQUIRE(cpu.pc() == 26u);
    }

    TEST_CASE("BRCS - backward branch (negative offset)", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_pc(100);
        cpu.set_sreg(0x01); // C=1
        cpu.exec_brcs(encode_brcs(-5));

        REQUIRE(cpu.pc() == 90u);
    }

    TEST_CASE("BRCS - zero offset stays at current PC", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_pc(50);
        cpu.exec_brcs(encode_brcs(0));

        REQUIRE(cpu.pc() == 50u);
    }

    TEST_CASE("BRCS - self-loop (k=-1 jumps back to own address)", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_pc(2);
        cpu.set_sreg(0x01); // C=1
        cpu.exec_brcs(encode_brcs(-1));

        REQUIRE(cpu.pc() == 0u);
    }

    TEST_CASE("BRCS - max positive offset (k=+63)", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_pc(100);
        cpu.set_sreg(0x01); // C=1
        cpu.exec_brcs(encode_brcs(63));

        REQUIRE(cpu.pc() == 226u);
    }

    TEST_CASE("BRCS - max negative offset (k=-64)", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_pc(228);
        cpu.set_sreg(0x01); // C=1
        cpu.exec_brcs(encode_brcs(-64));

        REQUIRE(cpu.pc() == 100u);
    }

    TEST_CASE("BRCS - flags unaffected", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        constexpr u8 sentinel = 0b01010101;
        cpu.set_sreg(sentinel);
        cpu.set_pc(20);
        cpu.exec_brcs(encode_brcs(3));

        REQUIRE(cpu.sreg() == sentinel);
    }

    // ---------------------------------------------------------------------------
    // BRCS — C-flag guard tests
    // ---------------------------------------------------------------------------

    TEST_CASE("BRCS - C=1: branch is taken", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_sreg(0x01); // C=1 — branch taken
        cpu.set_pc(20);
        cpu.exec_brcs(encode_brcs(3)); // +6 bytes

        REQUIRE(cpu.pc() == 26u);
    }

    TEST_CASE("BRCS - C=0: branch is not taken, PC unchanged", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_sreg(0x00); // C=0 — branch not taken
        cpu.set_pc(20);
        cpu.exec_brcs(encode_brcs(3));

        REQUIRE(cpu.pc() == 20u);
    }

    TEST_CASE("BRCS - C=0: negative offset also not taken", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_sreg(0x00); // C=0
        cpu.set_pc(100);
        cpu.exec_brcs(encode_brcs(-5));

        REQUIRE(cpu.pc() == 100u);
    }

    TEST_CASE("BRCS - C=0 with other SREG bits set: branch still not taken", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_sreg(0xFE); // all flags set except C
        cpu.set_pc(50);
        cpu.exec_brcs(encode_brcs(10));

        REQUIRE(cpu.pc() == 50u);
    }

    TEST_CASE("BRCS - only C bit matters: other flags set but C=1 still branches", "[brcs]")
    {
        auto cfg = make_test_config();
        MemoryMap mem{cfg};
        AvrCpu    cpu{mem, cfg};

        cpu.set_sreg(0xFF); // all flags set, including C
        cpu.set_pc(20);
        cpu.exec_brcs(encode_brcs(3)); // +6 bytes

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
// Pushes ret = pc() + 2 onto the stack (low byte first, then high byte),
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

// Stack layout after a 16-bit-PC CALL push (little-endian, lo at lowest address):
//   [INITIAL_SP - 2] = ret_lo   ← SP points here after CALL
//   [INITIAL_SP - 1] = ret_hi
//   SP_after = INITIAL_SP - 2
//
// Stack layout after a 22-bit-PC CALL push:
//   [INITIAL_SP - 3] = ret_lo   ← SP points here
//   [INITIAL_SP - 2] = ret_hi
//   [INITIAL_SP - 1] = ret upper byte
//   SP_after = INITIAL_SP - 3

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

    // PC = 0x0100 → ret = pc() + 2 = 0x0102.
    // Low byte = 0x02 lands at INITIAL_SP - 2 (lo is at the lowest stack address).
    cpu.set_pc(0x0100);
    flash_write16(mem.flash(), 0x0100, 0x0001); // target word — value doesn't matter here

    cpu.exec_call(encode_call(0));

    REQUIRE(mem.read8(INITIAL_SP - 2) == 0x02u);
}

TEST_CASE("CALL - return address high byte pushed to stack", "[call]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // PC = 0x0100 → ret = 0x0102. High byte = 0x01 should land at initial SP - 1.
    cpu.set_pc(0x0100);
    flash_write16(mem.flash(), 0x0100, 0x0001);

    cpu.exec_call(encode_call(0));

    REQUIRE(mem.read8(INITIAL_SP - 1) == 0x01u);
}

TEST_CASE("CALL - SP decremented by 2 after 16-bit-PC push", "[call]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x0001);

    cpu.exec_call(encode_call(0));

    REQUIRE(cpu.sp() == INITIAL_SP - 2);
}

TEST_CASE("CALL - return address uses pc() at call time, not original instruction address", "[call]")
{
    // With PC = 0x0200 at call time, ret = 0x0202 (pc() + 2).
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(0x0200);
    flash_write16(mem.flash(), 0x0200, 0x0001);

    cpu.exec_call(encode_call(0));

    // lo = 0x02 at INITIAL_SP-2, hi = 0x02 at INITIAL_SP-1
    REQUIRE(mem.read8(INITIAL_SP - 2) == 0x02u);
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
    // PC = 0x0100 → ret = 0x0102. With 22-bit PC, all 3 bytes are pushed
    // little-endian (lo at lowest address):
    //   [INITIAL_SP - 3] = ret_lo  (0x02)  ← SP points here
    //   [INITIAL_SP - 2] = ret_hi  (0x01)
    //   [INITIAL_SP - 1] = ret upper (0x00)
    auto cfg = make_22bit_pc_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(0x0100);
    flash_write16(mem.flash(), 0x0100, 0x0001);

    cpu.exec_call(encode_call(0));

    REQUIRE(mem.read8(INITIAL_SP - 3) == 0x02u); // ret_lo
    REQUIRE(mem.read8(INITIAL_SP - 2) == 0x01u); // ret_hi
    REQUIRE(mem.read8(INITIAL_SP - 1) == 0x00u); // ret upper
}

TEST_CASE("CALL - 22-bit PC: SP decremented by 3", "[call][22bit]")
{
    auto cfg = make_22bit_pc_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_pc(2);
    flash_write16(mem.flash(), 2, 0x0001);

    cpu.exec_call(encode_call(0));

    REQUIRE(cpu.sp() == INITIAL_SP - 3);
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

// ---------------------------------------------------------------------------
// SBIS  (1001 1011 AAAA Abbb)
//
// Skip next instruction if bit b in IO register A is set. Tests map IO into
// SRAM by setting `cfg.io_base = cfg.sram_base` so we can write/read bytes
// directly with MemoryMap::write8/read8.
// ---------------------------------------------------------------------------

static u16 encode_sbis(u8 A, u8 b)
{
    return static_cast<u16>(0x9B00 | ((A & 0x1F) << 3) | (b & 0x07));
}

TEST_CASE("SBIS - IO bit clear: no skip, 1 cycle", "[sbis]")
{
    auto cfg = make_test_config();
    cfg.io_base = cfg.sram_base; // map IO into SRAM for tests
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // IO A=0, bit b=2 cleared
    mem.write8(cfg.io_base + 0, 0x00);
    cpu.set_pc(50);
    flash_write16(mem.flash(), 50, 0x0000); // single-word next

    u8 cycles = cpu.exec_sbis(encode_sbis(0, 2));

    REQUIRE(cpu.pc() == 50u);
    REQUIRE(cycles == 1);
}

TEST_CASE("SBIS - IO bit set: skip single-word next, PC += 2, 2 cycles", "[sbis]")
{
    auto cfg = make_test_config();
    cfg.io_base = cfg.sram_base;
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // IO A=1, set bit 2
    mem.write8(cfg.io_base + 1, 0x04);
    cpu.set_pc(60);
    flash_write16(mem.flash(), 60, 0x0000); // NOP — single-word

    u8 cycles = cpu.exec_sbis(encode_sbis(1, 2));

    REQUIRE(cpu.pc() == 62u);
    REQUIRE(cycles == 2);
}

TEST_CASE("SBIS - IO bit set: skip two-word next, PC += 4, 3 cycles", "[sbis]")
{
    auto cfg = make_test_config();
    cfg.io_base = cfg.sram_base;
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // IO A=2, set bit 0
    mem.write8(cfg.io_base + 2, 0x01);
    cpu.set_pc(70);
    flash_write16(mem.flash(), 70, 0x940C); // JMP — two-word

    u8 cycles = cpu.exec_sbis(encode_sbis(2, 0));

    REQUIRE(cpu.pc() == 74u);
    REQUIRE(cycles == 3);
}

TEST_CASE("SBIS - flags unaffected (IO-based check)", "[sbis]")
{
    auto cfg = make_test_config();
    cfg.io_base = cfg.sram_base;
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b10101010;
    cpu.set_sreg(sentinel);
    // IO A=0, set bit 1
    mem.write8(cfg.io_base + 0, 0x02);
    cpu.set_pc(80);
    flash_write16(mem.flash(), 80, 0x0000);

    cpu.exec_sbis(encode_sbis(0, 1));

    REQUIRE(cpu.sreg() == sentinel);
}

// ---------------------------------------------------------------------------
// RET  (1001 0101 0000 1000)
//
// Pops the return address from the stack back into PC.
// Stack layout expected by RET (little-endian, low byte at lower address):
//   mem[SP]    = return_addr[7:0]   (lo byte)
//   mem[SP+1]  = return_addr[15:8]  (hi byte)
//   (mem[SP+2] = return_addr[23:16] for 22-bit PC devices)
// After:
//   PC  = (hi << 8) | lo
//   SP += 2  (16-bit PC)  /  3  (22-bit PC)
// Cycles: 4 (16-bit PC) / 5 (22-bit PC)
// Flags:  none affected
// ---------------------------------------------------------------------------

TEST_CASE("RET - restores PC from stack (16-bit address)", "[ret]")
{
    // Return address 0x1234: lo=0x34 at SP, hi=0x12 at SP+1.
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sp(0x07FE);
    mem.write8(0x07FE, 0x34); // lo
    mem.write8(0x07FF, 0x12); // hi

    cpu.exec_ret(0x9508);

    REQUIRE(cpu.pc() == 0x1234u);
}

TEST_CASE("RET - SP incremented by 2 after 16-bit pop", "[ret]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sp(0x07FE);
    mem.write8(0x07FE, 0x00);
    mem.write8(0x07FF, 0x00);

    cpu.exec_ret(0x9508);

    REQUIRE(cpu.sp() == 0x0800u);
}

TEST_CASE("RET - restores full 16-bit PC correctly", "[ret]")
{
    // Address 0x7FFE: lo=0xFE, hi=0x7F
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sp(0x0200);
    mem.write8(0x0200, 0xFE); // lo
    mem.write8(0x0201, 0x7F); // hi

    cpu.exec_ret(0x9508);

    REQUIRE(cpu.pc() == 0x7FFEu);
}

TEST_CASE("RET - returns 4 cycles for 16-bit PC device", "[ret]")
{
    auto cfg = make_test_config(); // has_22_bit_pc = false
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sp(0x07FE);
    mem.write8(0x07FE, 0x00);
    mem.write8(0x07FF, 0x01);

    u8 cycles = cpu.exec_ret(0x9508);

    REQUIRE(cycles == 4);
}

TEST_CASE("RET - returns 5 cycles for 22-bit PC device", "[ret]")
{
    auto cfg = make_22bit_pc_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sp(0x07FD);
    mem.write8(0x07FD, 0x00);
    mem.write8(0x07FE, 0x00);
    mem.write8(0x07FF, 0x00);

    u8 cycles = cpu.exec_ret(0x9508);

    REQUIRE(cycles == 5);
}

TEST_CASE("RET - restores 22-bit PC from stack", "[ret]")
{
    // Return address 0x123456: lo=0x56, hi=0x34, upper=0x12
    auto cfg = make_22bit_pc_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sp(0x07FD);
    mem.write8(0x07FD, 0x56); // lo
    mem.write8(0x07FE, 0x34); // hi
    mem.write8(0x07FF, 0x12); // upper

    cpu.exec_ret(0x9508);

    REQUIRE(cpu.pc() == 0x123456u);
}

TEST_CASE("RET - SP incremented by 3 after 22-bit pop", "[ret]")
{
    auto cfg = make_22bit_pc_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    cpu.set_sp(0x07FD);
    mem.write8(0x07FD, 0x00);
    mem.write8(0x07FE, 0x00);
    mem.write8(0x07FF, 0x00);

    cpu.exec_ret(0x9508);

    REQUIRE(cpu.sp() == 0x0800u);
}

TEST_CASE("RET - flags unaffected", "[ret]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    constexpr u8 sentinel = 0b10110101;
    cpu.set_sreg(sentinel);
    cpu.set_sp(0x07FE);
    mem.write8(0x07FE, 0x00);
    mem.write8(0x07FF, 0x00);

    cpu.exec_ret(0x9508);

    REQUIRE(cpu.sreg() == sentinel);
}

TEST_CASE("RET - PC not affected by old register values", "[ret]")
{
    // Ensures PC comes entirely from the stack, not from any register.
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    for (u8 i = 0; i < 32; ++i) cpu.set_reg(i, 0xFF);
    cpu.set_sp(0x07FE);
    mem.write8(0x07FE, 0xAB); // lo
    mem.write8(0x07FF, 0xCD); // hi

    cpu.exec_ret(0x9508);

    REQUIRE(cpu.pc() == 0xCDABu);
}

// ---------------------------------------------------------------------------
// CALL + RET round-trip
//
// After CALL, exec_ret must restore PC to the instruction following CALL
// (return address = pc_before_call + 2, since PC is advanced past only the
// first CALL word before exec fires; the second word is consumed internally).
// SP must be fully restored to its value before CALL.
// ---------------------------------------------------------------------------

TEST_CASE("CALL+RET round-trip: PC restored to return address", "[call][ret]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    // Place CALL at byte 0x0200; second word (target) can be anything.
    cpu.set_pc(0x0200);
    flash_write16(mem.flash(), 0x0200, 0x0000); // target word = 0

    cpu.exec_call(encode_call(0));
    // Return address = 0x0200 + 2 = 0x0202

    cpu.exec_ret(0x9508);

    REQUIRE(cpu.pc() == 0x0202u);
}

TEST_CASE("CALL+RET round-trip: SP restored to initial value", "[call][ret]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    const u16 sp_before = cpu.sp();

    cpu.set_pc(0x0100);
    flash_write16(mem.flash(), 0x0100, 0x0001);

    cpu.exec_call(encode_call(0));
    cpu.exec_ret(0x9508);

    REQUIRE(cpu.sp() == sp_before);
}

TEST_CASE("CALL+RET round-trip: nested calls restore SP correctly", "[call][ret]")
{
    // Two nested CALLs followed by two RETs should leave SP at its original value.
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};
    mem.attach_cpu(&cpu);

    const u16 sp_initial = cpu.sp();

    // First CALL from 0x0100 → second word at 0x0100, target = 0
    cpu.set_pc(0x0100);
    flash_write16(mem.flash(), 0x0100, 0x0001);
    cpu.exec_call(encode_call(0));

    // Second CALL from wherever PC landed; write second word near target
    u32 mid_pc = cpu.pc();
    flash_write16(mem.flash(), mid_pc, 0x0002);
    cpu.exec_call(encode_call(0));

    // Unwind
    cpu.exec_ret(0x9508);
    cpu.exec_ret(0x9508);

    REQUIRE(cpu.sp() == sp_initial);
}

// ---------------------------------------------------------------------------
// BRCC  (1111 01kk kkkk k000)
//
// Branch if Carry Clear. Relative branch with signed 7-bit word offset k.
// Branches when C==0; falls through when C==1.
// PC on entry is already advanced past the opcode (instruction_address + 2).
// Flags: none affected.
// ---------------------------------------------------------------------------

static u16 encode_brcc(int8_t k)
{
    // 1111 01kk kkkk k000 — k occupies bits 9:3
    return static_cast<u16>(0xF400 | ((static_cast<u16>(k) & 0x7F) << 3));
}

TEST_CASE("BRCC - C=0: branch taken, forward offset", "[brcc]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // C=0 — branch taken
    cpu.set_pc(20);
    cpu.exec_brcc(encode_brcc(3)); // k=+3 words → +6 bytes

    REQUIRE(cpu.pc() == 26u);
}

TEST_CASE("BRCC - C=1: branch not taken, PC unchanged", "[brcc]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x01); // C=1 — branch not taken
    cpu.set_pc(20);
    cpu.exec_brcc(encode_brcc(3));

    REQUIRE(cpu.pc() == 20u);
}

TEST_CASE("BRCC - C=0: backward branch (negative offset)", "[brcc]")
{
    // PC=100, k=-5 → target = 100 + (-5)*2 = 90
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // C=0
    cpu.set_pc(100);
    cpu.exec_brcc(encode_brcc(-5));

    REQUIRE(cpu.pc() == 90u);
}

TEST_CASE("BRCC - C=0: zero offset stays at current PC", "[brcc]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // C=0
    cpu.set_pc(50);
    cpu.exec_brcc(encode_brcc(0));

    REQUIRE(cpu.pc() == 50u);
}

TEST_CASE("BRCC - C=0: self-loop (k=-1 jumps back to own address)", "[brcc]")
{
    // Instruction at byte 0; PC is already 2 when exec fires.
    // k=-1 → target = 2 + (-1)*2 = 0
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // C=0
    cpu.set_pc(2);
    cpu.exec_brcc(encode_brcc(-1));

    REQUIRE(cpu.pc() == 0u);
}

TEST_CASE("BRCC - C=0: max positive offset (k=+63)", "[brcc]")
{
    // k=63 → offset_bytes = +126
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // C=0
    cpu.set_pc(100);
    cpu.exec_brcc(encode_brcc(63));

    REQUIRE(cpu.pc() == 226u);
}

TEST_CASE("BRCC - C=0: max negative offset (k=-64)", "[brcc]")
{
    // k=-64 → offset_bytes = -128; start at 228 so target = 100
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // C=0
    cpu.set_pc(228);
    cpu.exec_brcc(encode_brcc(-64));

    REQUIRE(cpu.pc() == 100u);
}

TEST_CASE("BRCC - C=1: negative offset also not taken", "[brcc]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x01); // C=1
    cpu.set_pc(100);
    cpu.exec_brcc(encode_brcc(-5));

    REQUIRE(cpu.pc() == 100u);
}

TEST_CASE("BRCC - C=1 with other SREG bits set: branch still not taken", "[brcc]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFF); // all flags set, including C
    cpu.set_pc(50);
    cpu.exec_brcc(encode_brcc(10));

    REQUIRE(cpu.pc() == 50u);
}

TEST_CASE("BRCC - only C bit matters: other flags set but C=0 still branches", "[brcc]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0xFE); // all flags set except C (bit 0 = 0)
    cpu.set_pc(20);
    cpu.exec_brcc(encode_brcc(3)); // +6 bytes

    REQUIRE(cpu.pc() == 26u);
}

TEST_CASE("BRCC - flags unaffected", "[brcc]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    constexpr u8 sentinel = 0b10100100; // C=0, others set
    cpu.set_sreg(sentinel);
    cpu.set_pc(20);
    cpu.exec_brcc(encode_brcc(3));

    REQUIRE(cpu.sreg() == sentinel);
}

TEST_CASE("BRCC - cycle count taken: returns 2", "[brcc]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x00); // C=0 — branch taken
    cpu.set_pc(20);
    u8 cycles = cpu.exec_brcc(encode_brcc(3));

    REQUIRE(cycles == 2);
}

TEST_CASE("BRCC - cycle count not taken: returns 1", "[brcc]")
{
    auto cfg = make_test_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_sreg(0x01); // C=1 — branch not taken
    cpu.set_pc(20);
    u8 cycles = cpu.exec_brcc(encode_brcc(3));

    REQUIRE(cycles == 1);
}

// ---------------------------------------------------------------------------
// IJMP  (1001 0100 0000 1001)
//
// PC <- Z  where Z (R31:R30) is a 16-bit word address.
// Because the emulator's PC is byte-addressed the jump target is Z << 1.
// No flags are affected; Z registers are not modified.
// ---------------------------------------------------------------------------

static DeviceConfig make_z_branch_config()
{
    DeviceConfig c{};
    c.flash_size_bytes = 32 * 1024;
    c.sram_size_bytes  = 2  * 1024;
    c.sram_base        = 0x0100;
    c.z_low_reg        = 30;  // ZL = R30
    c.z_high_reg       = 31;  // ZH = R31
    return c;
}

static constexpr u16 IJMP_OPCODE = 0x9409;

TEST_CASE("IJMP - PC set to Z word address converted to byte address", "[ijmp]")
{
    auto cfg = make_z_branch_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // Z = 0x005C (word address) -> expected PC = 0x00B8 (byte address)
    cpu.set_reg(30, 0x5C); // ZL
    cpu.set_reg(31, 0x00); // ZH

    cpu.exec_ijmp(IJMP_OPCODE);

    REQUIRE(cpu.pc() == 0x00B8u);
}

TEST_CASE("IJMP - Z = 0 jumps to address 0 (reset vector)", "[ijmp]")
{
    auto cfg = make_z_branch_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(30, 0x00);
    cpu.set_reg(31, 0x00);
    cpu.set_pc(0x1000); // start somewhere else

    cpu.exec_ijmp(IJMP_OPCODE);

    REQUIRE(cpu.pc() == 0x0000u);
}

TEST_CASE("IJMP - ZH:ZL both contribute to the target address", "[ijmp]")
{
    auto cfg = make_z_branch_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // Z = 0x0100 (word addr) -> byte addr = 0x0200
    cpu.set_reg(30, 0x00); // ZL
    cpu.set_reg(31, 0x01); // ZH

    cpu.exec_ijmp(IJMP_OPCODE);

    REQUIRE(cpu.pc() == 0x0200u);
}

TEST_CASE("IJMP - ZL alone (ZH = 0) forms correct byte address", "[ijmp]")
{
    auto cfg = make_z_branch_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // Z = 0x0010 -> byte addr = 0x0020
    cpu.set_reg(30, 0x10);
    cpu.set_reg(31, 0x00);

    cpu.exec_ijmp(IJMP_OPCODE);

    REQUIRE(cpu.pc() == 0x0020u);
}

TEST_CASE("IJMP - maximum Z value produces correct byte address", "[ijmp]")
{
    auto cfg = make_z_branch_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    // Z = 0x3FFF (word addr) -> byte addr = 0x7FFE (fits in 32KB flash)
    cpu.set_reg(30, 0xFF); // ZL
    cpu.set_reg(31, 0x3F); // ZH

    cpu.exec_ijmp(IJMP_OPCODE);

    REQUIRE(cpu.pc() == 0x7FFEu);
}

TEST_CASE("IJMP - Z registers not modified", "[ijmp]")
{
    auto cfg = make_z_branch_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(30, 0x2A);
    cpu.set_reg(31, 0x01);

    cpu.exec_ijmp(IJMP_OPCODE);

    REQUIRE(cpu.reg(30) == 0x2A);
    REQUIRE(cpu.reg(31) == 0x01);
}

TEST_CASE("IJMP - previous PC value has no influence on target", "[ijmp]")
{
    auto cfg = make_z_branch_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(30, 0x04);
    cpu.set_reg(31, 0x00); // Z = 0x0004 -> byte addr 0x0008

    cpu.set_pc(0x0000);
    cpu.exec_ijmp(IJMP_OPCODE);
    REQUIRE(cpu.pc() == 0x0008u);

    // jump again from a different PC — result must be the same
    cpu.set_pc(0x1234);
    cpu.exec_ijmp(IJMP_OPCODE);
    REQUIRE(cpu.pc() == 0x0008u);
}

TEST_CASE("IJMP - returns 2 cycles", "[ijmp]")
{
    auto cfg = make_z_branch_config();
    MemoryMap mem{cfg};
    AvrCpu    cpu{mem, cfg};

    cpu.set_reg(30, 0x01);
    cpu.set_reg(31, 0x00);

    u8 cycles = cpu.exec_ijmp(IJMP_OPCODE);

    REQUIRE(cycles == 2u);
}
