#include "cpu/avr_cpu.h"
#include "memory/memory_map.h"

namespace avrion {

u8 AvrCpu::exec_out(u16 opcode) {
    // OUT
    // 1011 1AAr rrrr AAAA
    u8 A = (opcode & 0x0F) | ((opcode & 0x0600) >> 5);
    u8 r = (opcode >> 4) & 0x1F;
    mem_.write8(cfg_.io_base + A, reg(r));
    return 1;
}

u8 AvrCpu::exec_in(u16 opcode) {
    // IN
    // 1011 0AAr rrrr AAAA
    u8 A = (opcode & 0x0F) | ((opcode & 0x0600) >> 5);
    u8 r = (opcode >> 4) & 0x1F;
    set_reg(r, mem_.read8(cfg_.io_base + A));
    return 1;
}

u8 AvrCpu::exec_ldi(u16 opcode) {
    // LDI
    // 1110 KKKK dddd KKKK
    u8 K = (opcode & 0x0F) | ((opcode & 0x0F00) >> 4);
    u8 d = 16 + ((opcode >> 4) & 0x0F);
    set_reg(d, K);
    return 1;
}

u8 AvrCpu::exec_st_x(u16 opcode) {
    // ST X
    // 1001 001r rrrr 1100
    u8 r = reg((opcode & 0x01F0) >> 4);
    mem_.write8(x(), r);
    return 1;
}

u8 AvrCpu::exec_st_x_post_inc(u16 opcode) {
    // ST X+
    // 1001 001r rrrr 1101
    u8 r = reg((opcode & 0x01F0) >> 4);
    u16 addr = x();
    mem_.write8(addr, r);
    set_x(addr + 1);
    return 1;
}

u8 AvrCpu::exec_st_x_pre_dec(u16 opcode) {
    // ST -X
    // 1001 001r rrrr 1110
    u8 r = reg((opcode & 0x01F0) >> 4);
    u16 addr = x() - 1;
    set_x(addr);
    mem_.write8(addr, r);
    return 1;
}

u8 AvrCpu::exec_lds(u16 opcode) {
    // LDS
    // 1001 000d dddd 0000
    // kkkk kkkk kkkk kkkk
    u8 d = (opcode & 0x01F0) >> 4;
    u16 k = mem_.fetch16(pc());
    set_reg(d, mem_.read8(k));
    set_pc(pc() + 2);
    return 2;
}

u8 AvrCpu::exec_sts(u16 opcode) {
    // STS
    // 1001 001d dddd 0000
    // kkkk kkkk kkkk kkkk
    u8 d = (opcode & 0x01F0) >> 4;
    u16 k = mem_.fetch16(pc());
    mem_.write8(k, reg(d));
    set_pc(pc() + 2);
    return 2;
}

u8 AvrCpu::exec_lpm(u16 opcode) {
    // LPM
    // 1001 0101 1100 1000
    set_reg(0, mem_.fetch8(z()));
    return 3;
}

u8 AvrCpu::exec_lpm_z(u16 opcode) {
    // LPM Z
    // 1001 000d dddd 0100
    u8 d = (opcode & 0x01F0) >> 4;
    set_reg(d, mem_.fetch8(z()));
    return 3;
}

u8 AvrCpu::exec_lpm_z_post_inc(u16 opcode) {
    // LPM Z+
    // 1001 000d dddd 0101
    u8 d = (opcode & 0x01F0) >> 4;
    u16 addr = z();
    set_reg(d, mem_.fetch8(addr));
    set_z(addr + 1);
    return 3;
}

u8 AvrCpu::exec_movw(u16 opcode) {
    // MOVW
    // 0000 0001 dddd rrrr
    u8 d = ((opcode & 0x00F0) >> 4) << 1;
    u8 r = (opcode & 0x000F) << 1;
    set_reg(d, reg(r));
    set_reg(d + 1, reg(r + 1));
    return 1;
}

u8 AvrCpu::exec_ld_x(u16 opcode) {
    // LD X
    // 1001 000d dddd 1100
    u8 d = (opcode & 0x01F0) >> 4;
    set_reg(d, mem_.read8(x()));
    return 2;
}

u8 AvrCpu::exec_ld_x_post_inc(u16 opcode) {
    // LD X+
    // 1001 000d dddd 1101
    u8 d = (opcode & 0x01F0) >> 4;
    u16 addr = x();
    set_reg(d, mem_.read8(addr));
    set_x(addr + 1);
    return 2;
}

u8 AvrCpu::exec_ld_x_pre_dec(u16 opcode) {
    // LD -X
    // 1001 000d dddd 1110
    u8 d = (opcode & 0x01F0) >> 4;
    u16 addr = x() - 1;
    set_x(addr);
    set_reg(d, mem_.read8(addr));
    return 2;
}

u8 AvrCpu::exec_push(u16 opcode) {
    // PUSH
    // 1001 001d dddd 1111
    u8 d = (opcode & 0x01F0) >> 4;
    mem_.write8(--st_.sp, reg(d));
    return 1;
}

u8 AvrCpu::exec_mov(u16 opcode) {
    // MOV
    // 0010 11rd dddd rrrr
    u8 d = (opcode >> 4) & 0x1F;
    u8 r = (opcode & 0x000F) | ((opcode >> 5) & 0x10);

    set_reg(d, reg(r));
    return 1;
}

u8 AvrCpu::exec_pop(u16 opcode) {
    // POP
    // 1001 001d dddd 1111
    u8 d = (opcode & 0x01F0) >> 4;
    set_reg(d, mem_.read8(st_.sp++));
    return 2;
}

} // namespace avrion