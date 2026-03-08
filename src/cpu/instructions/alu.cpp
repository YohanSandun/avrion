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

} // namespace avrion