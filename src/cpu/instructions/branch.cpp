#include "cpu/avr_cpu.h"
#include "cpu/decoder.h"
#include "memory/memory_map.h"
#include <iostream>

namespace avrion {

u8 AvrCpu::exec_jmp(u16 opcode) {
    // JMP
    // 1001 010k kkkk 110k
    // kkkk kkkk kkkk kkkk
    u32 k = ((opcode & 0x01F0) << 13) | ((opcode & 0x0001) << 16) | mem_.fetch16(pc());
    set_pc(k << 1);
    return cfg_.has_22_bit_pc ? 4 : 3;
}

u8 AvrCpu::exec_rjmp(u16 opcode) {
    // RJMP
    // 1100 kkkk kkkk kkkk
    u16 k = opcode & 0x0FFF;
    if (k & 0x0800) {
        k |= 0xF000;
    }
    set_pc(pc() + static_cast<u32>(static_cast<int16_t>(k) << 1));
    return 2;
}

u8 AvrCpu::exec_brne(u16 opcode) {
    // BRNE
    // 1111 01kk kkkk k001
    if (sreg() & 0x02) { // Z flag set → not taken
        return 1;
    }
    
    u16 k = (opcode & 0x03F8) >> 3;
    if (k & 0x0040) {
        k |= 0xFF80;
    }
    set_pc(pc() + static_cast<u32>(static_cast<int16_t>(k) << 1));
    return 2;
}

u8 AvrCpu::exec_call(u16 opcode) {
    // CALL
    // 1001 010k kkkk 111k
    // kkkk kkkk kkkk kkkk
    u32 k = ((opcode & 0x01F0) << 13) | ((opcode & 0x0001) << 16) | mem_.fetch16(pc());
    u32 ret = pc() + 2;
    if (cfg_.has_22_bit_pc) {
        mem_.write8(--st_.sp, (ret >> 16) & 0xFF); // upper byte pushed first (highest address)
    }
    mem_.write8(--st_.sp, (ret >> 8) & 0xFF); // hi byte
    mem_.write8(--st_.sp, ret & 0xFF);          // lo byte — SP now points here
    set_pc(k << 1);
    return cfg_.has_22_bit_pc ? 4 : 3;
}

u8 AvrCpu::exec_breq(u16 opcode) {
    // BREQ
    // 1111 00kk kkkk k001
    if (!(sreg() & 0x02)) { // Z flag not set → not taken
        return 1;
    }
    
    u16 k = (opcode & 0x03F8) >> 3;
    if (k & 0x0040) {
        k |= 0xFF80;
    }
    set_pc(pc() + static_cast<u32>(static_cast<int16_t>(k) << 1));
    return 2;
}

u8 AvrCpu::exec_cpse(u16 opcode) {
    // CPSE
    // 0001 00rd dddd rrrr
    u8 d = (opcode >> 4) & 0x1F;
    u8 r = (opcode & 0x000F) | ((opcode >> 5) & 0x10);

    if (reg(d) != reg(r)) {
        return 1;
    }

    u16 next_opcode = mem_.fetch16(pc());
    const InstructionDesc* desc = lookup_instruction(next_opcode);
    bool next_is_two_word = desc && desc->is_two_word;

    set_pc(pc() + (next_is_two_word ? 4 : 2));
    return next_is_two_word ? 3 : 2;
}

u8 AvrCpu::exec_ret(u16 opcode) {
    // RET
    // 1001 0101 0000 1000
    u32 ret = mem_.read8(st_.sp++);
    ret |= static_cast<u32>(mem_.read8(st_.sp++)) << 8;
    if (cfg_.has_22_bit_pc) {
        ret |= static_cast<u32>(mem_.read8(st_.sp++)) << 16;
    }
    std::cout << "returning to: " << ret << std::endl;
    set_pc(ret);
    return cfg_.has_22_bit_pc ? 5 : 4;
}

u8 AvrCpu::exec_sbis(u16 opcode) {
    // SBIS
    // 1001 1011 AAAA Abbb
    u8 A = (opcode >> 3) & 0x1F;
    u8 b = opcode & 0x07;

    if (!(sreg() & (1 << b))) { 
        return 1;
    }

    u16 next_opcode = mem_.fetch16(pc());
    const InstructionDesc* desc = lookup_instruction(next_opcode);
    bool next_is_two_word = desc && desc->is_two_word;

    set_pc(pc() + (next_is_two_word ? 4 : 2));
    return next_is_two_word ? 3 : 2;
}

} // namespace avrion