#include "cpu/avr_cpu.h"
#include "memory/memory_map.h"

namespace avrion {

void AvrCpu::exec_out(u16 opcode) {
    // OUT
    // 1011 1AAr rrrr AAAA
    u8 A = (opcode & 0x0F) | ((opcode & 0x0600) >> 5);
    u8 r = (opcode >> 4) & 0x1F;
    mem_.write8(cfg_.io_base + A, reg(r));
}

void AvrCpu::exec_ldi(u16 opcode) {
    // LDI
    // 1110 KKKK dddd KKKK
    u8 K = (opcode & 0x0F) | ((opcode & 0x0F00) >> 4);
    u8 d = 16 + ((opcode >> 4) & 0x0F);
    set_reg(d, K);
}

} // namespace avrion