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

void AvrCpu::exec_in(u16 opcode) {
    // IN
    // 1011 0AAr rrrr AAAA
    u8 A = (opcode & 0x0F) | ((opcode & 0x0600) >> 5);
    u8 r = (opcode >> 4) & 0x1F;
    set_reg(r, mem_.read8(cfg_.io_base + A));
}

void AvrCpu::exec_ldi(u16 opcode) {
    // LDI
    // 1110 KKKK dddd KKKK
    u8 K = (opcode & 0x0F) | ((opcode & 0x0F00) >> 4);
    u8 d = 16 + ((opcode >> 4) & 0x0F);
    set_reg(d, K);
}

void AvrCpu::exec_st_x(u16 opcode) {
    // ST X
    // 1001 001r rrrr 1100
    u8 r = reg((opcode & 0x01F0) >> 4);
    mem_.write8(x(), r);
}

void AvrCpu::exec_st_x_post_inc(u16 opcode) {
    // ST X+
    // 1001 001r rrrr 1101
    u8 r = reg((opcode & 0x01F0) >> 4);
    u16 addr = x();
    mem_.write8(addr, r);
    set_x(addr + 1);
}

void AvrCpu::exec_st_x_pre_dec(u16 opcode) {
    // ST -X
    // 1001 001r rrrr 1110
    u8 r = reg((opcode & 0x01F0) >> 4);
    u16 addr = x() - 1;
    set_x(addr);
    mem_.write8(addr, r);
}

void AvrCpu::exec_lds(u16 opcode) {
    // LDS
    // 1001 000d dddd 0000
    // kkkk kkkk kkkk kkkk
    u8 d = (opcode & 0x01F0) >> 4;
    u16 k = mem_.fetch16(pc());
    set_reg(d, mem_.read8(k));
    set_pc(pc() + 2);
}

void AvrCpu::exec_sts(u16 opcode) {
    // STS
    // 1001 001d dddd 0000
    // kkkk kkkk kkkk kkkk
    u8 d = (opcode & 0x01F0) >> 4;
    u16 k = mem_.fetch16(pc());
    mem_.write8(k, reg(d));
    set_pc(pc() + 2);
}

} // namespace avrion