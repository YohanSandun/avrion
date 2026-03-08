#include "cpu/avr_cpu.h"
#include "memory/memory_map.h"

namespace avrion {

void AvrCpu::exec_eor(u16 opcode) {
    // EOR
    // 0010 01rd dddd rrrr
    u8 d = (opcode >> 4) & 0x1F;
    u8 r = (opcode & 0x000F) | ((opcode >> 5) & 0x10);
    u8 result = reg(d) ^ reg(r);
    set_reg(d, result);

    u8 _sreg = sreg();
    _sreg = result == 0
        ? (_sreg | 0x02) : (_sreg & ~0x02); // Z set if result is zero
    _sreg = (result & 0x80) != 0
        ? (_sreg | 0x04) : (_sreg & ~0x04); // N set if bit 7 of result is set
    _sreg &= ~0x08; // V cleared
    // S = N XOR V; since V is always 0 for EOR, S = N
    _sreg = (_sreg & 0x04)
        ? (_sreg | 0x10) : (_sreg & ~0x10);
    set_sreg(_sreg);
}

void AvrCpu::exec_cpi(u16 opcode) {
    // CPI
    // 0011 KKKK dddd KKKK
    u8 d = reg(16 + ((opcode >> 4) & 0x0F));
    u8 k = (opcode & 0x000F) | ((opcode >> 4) & 0xF0);
    u8 result = d - k;

    u8 d7 = (d >> 7) & 1;
    u8 k7 = (k >> 7) & 1;
    u8 r7 = (result >> 7) & 1;

    u8 _sreg = sreg();
    _sreg = (_sreg & ~1u) | (((~d7 & k7) | (k7 & r7) | (r7 & ~d7)) & 1); // C flag
    _sreg = result == 0
        ? (_sreg | 0x02) : (_sreg & ~0x02u); // Z set if result is zero
    _sreg = (result & 0x80) != 0
        ? (_sreg | 0x04) : (_sreg & ~0x04); // N set if bit 7 of result is set
    _sreg = (_sreg & ~0x08u) | ((((d7 & ~k7 & ~r7) | (~d7 & k7 & r7)) & 1) << 3) ; // V flag
    _sreg |= (((_sreg & 0x04) >> 2) ^ ((_sreg & 0x08) >> 3)) << 4; // S = N XOR V
    _sreg = (_sreg & ~0x20u) | ((((~(d & 0x8) & (k & 0x8)) | ((k & 0x8) & (result & 0x8)) | ((result & 0x8) & ~(d & 0x8))) & 0x8) << 2); // H flag
    set_sreg(_sreg);
}

} // namespace avrion