#include "cpu/avr_cpu.h"
#include "memory/memory_map.h"

namespace avrion {

void AvrCpu::exec_jmp(u16 opcode) {
    // JMP
    // 1001 010k kkkk 110k
    // kkkk kkkk kkkk kkkk
    u32 k = ((opcode & 0x01F0) << 13) | ((opcode & 0x0001) << 16) | mem_.fetch16(pc());
    set_pc(k << 1);
}

void AvrCpu::exec_rjmp(u16 opcode) {
    // RJMP
    // 1100 kkkk kkkk kkkk
    u16 k = opcode & 0x0FFF;
    if (k & 0x0800) {
        k |= 0xF000;
    }
    set_pc(pc() + static_cast<u32>(static_cast<int16_t>(k) << 1));
}

void AvrCpu::exec_brne(u16 opcode) {
    // BRNE
    // 1111 01kk kkkk k001
    if (sreg() & 0x02) { // Z flag set → not taken
        return;
    }
    
    u16 k = (opcode & 0x03F8) >> 3;
    if (k & 0x0040) {
        k |= 0xFF80;
    }
    set_pc(pc() + static_cast<u32>(static_cast<int16_t>(k) << 1));
}

void AvrCpu::exec_call(u16 opcode) {
    // CALL
    // 1001 010k kkkk 111k
    // kkkk kkkk kkkk kkkk
    u32 k = ((opcode & 0x01F0) << 13) | ((opcode & 0x0001) << 16) | mem_.fetch16(pc());
    u32 ret = pc() + 4;
    mem_.write8(st_.sp, ret & 0xFF);
    mem_.write8(--st_.sp, (ret >> 8) & 0xFF);
    if (cfg_.has_22_bit_pc) {
        mem_.write8(--st_.sp, (ret >> 16) & 0xFF);
    }
    set_pc(k << 1);
}

} // namespace avrion