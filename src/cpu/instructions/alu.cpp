#include "cpu/avr_cpu.h"
#include "memory/memory_map.h"

namespace avrion {

u8 AvrCpu::exec_eor(u16 opcode) {
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
    return 1;
}

u8 AvrCpu::exec_cpi(u16 opcode) {
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
    return 1;
}

u8 AvrCpu::exec_cpc(u16 opcode) {
    // CPC
    // 0000 01rd dddd rrrr
    u8 d = reg((opcode >> 4) & 0x1F);
    u8 r = reg((opcode & 0x000F) | ((opcode >> 5) & 0x10));
    u8 result = d - r - ((sreg() & 0x01) ? 1 : 0); // C flag is borrow from previous operation

    u8 rd7 = (d >> 7) & 1;
    u8 rr7 = (r >> 7) & 1;
    u8 r7 = (result >> 7) & 1;

    u8 _sreg = sreg();
    _sreg = (_sreg & ~1u) | (((~rd7 & rr7) | (rr7 & r7) | (r7 & ~rd7)) & 1); // C flag
    _sreg = result == 0
        ? _sreg : (_sreg & ~0x02u); // Z cleared if result is non-zero; otherwise unchanged
    _sreg = (result & 0x80) != 0
        ? (_sreg | 0x04) : (_sreg & ~0x04); // N set if bit 7 of result is set
    _sreg = (_sreg & ~0x08u) | ((((rd7 & ~rr7 & ~r7) | (~rd7 & rr7 & r7)) & 1) << 3) ; // V flag
    _sreg |= (((_sreg & 0x04) >> 2) ^ ((_sreg & 0x08) >> 3)) << 4; // S = N XOR V
    _sreg = (_sreg & ~0x20u) | ((((~(d & 0x8) & (r & 0x8)) | ((r & 0x8) & (result & 0x8)) | ((result & 0x8) & ~(d & 0x8))) & 0x8) << 2); // H flag
    set_sreg(_sreg);
    return 1;
}

u8 AvrCpu::exec_ori(u16 opcode) {
    // ORI
    // 0110 KKKK dddd KKKK
    u8 d = 16 + ((opcode >> 4) & 0x0F);
    u8 k = (opcode & 0x000F) | ((opcode >> 4) & 0xF0);
    u8 result = reg(d) | k;
    set_reg(d, result);

    u8 _sreg = sreg();
    _sreg = result == 0
        ? (_sreg | 0x02) : (_sreg & ~0x02u); // Z set if result is zero
    _sreg = (result & 0x80) != 0
        ? (_sreg | 0x04) : (_sreg & ~0x04); // N set if bit 7 of result is set
    _sreg = (_sreg & ~0x08u); // V cleared
    // S = N XOR V; since V is always 0 for ORI, S = N
    _sreg = (_sreg & 0x04)
        ? (_sreg | 0x10) : (_sreg & ~0x10);
    set_sreg(_sreg);
    return 1;
}

u8 AvrCpu::exec_and(u16 opcode) {
    // AND
    // 0010 00rd dddd rrrr
    u8 d = (opcode >> 4) & 0x1F;
    u8 r = (opcode & 0x000F) | ((opcode >> 5) & 0x10);
    u8 result = reg(d) & reg(r);
    set_reg(d, result);

    u8 _sreg = sreg();
    _sreg = result == 0
        ? (_sreg | 0x02) : (_sreg & ~0x02u); // Z set if result is zero
    _sreg = (result & 0x80) != 0
        ? (_sreg | 0x04) : (_sreg & ~0x04); // N set if bit 7 of result is set
    _sreg = (_sreg & ~0x08u); // V cleared
    // S = N XOR V; since V is always 0 for AND, S = N
    _sreg = (_sreg & 0x04)
        ? (_sreg | 0x10) : (_sreg & ~0x10);
    set_sreg(_sreg);
    return 1;
}

u8 AvrCpu::exec_add(u16 opcode) {
    // ADD
    // 0000 11rd dddd rrrr
    u8 d = (opcode >> 4) & 0x1F;
    u8 r = (opcode & 0x000F) | ((opcode >> 5) & 0x10);
    u8 rr = reg(r);
    u8 rd = reg(d);
    u8 result = rd + rr;
    set_reg(d, result);

    u8 rd7 = (rd >> 7) & 1;
    u8 rr7 = (rr >> 7) & 1;
    u8 r7 = (result >> 7) & 1;

    u8 _sreg = sreg();
    _sreg = (_sreg & ~1u) | (((rd7 & rr7) | (rr7 & ~r7) | (~r7 & rd7)) & 1); // C flag
    _sreg = result == 0
        ? (_sreg | 0x02) : (_sreg & ~0x02u); // Z set if result is zero
    _sreg = (result & 0x80) != 0
        ? (_sreg | 0x04) : (_sreg & ~0x04); // N set if bit 7 of result is set
    _sreg = (_sreg & ~0x08u) | ((((rd7 & rr7 & ~r7) | (~rd7 & ~rr7 & r7)) & 1) << 3) ; // V flag
    _sreg |= (((_sreg & 0x04) >> 2) ^ ((_sreg & 0x08) >> 3)) << 4; // S = N XOR V
    _sreg = (_sreg & ~0x20u) | (((((rd & 0x8) & (rr & 0x8)) | ((rr & 0x8) & ~(result & 0x8)) | (~(result & 0x8) & (rd & 0x8))) & 0x8) << 2); // H flag
    set_sreg(_sreg);
    return 1;
}

} // namespace avrion