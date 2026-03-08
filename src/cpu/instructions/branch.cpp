#include "cpu/avr_cpu.h"
#include "memory/memory_map.h"

namespace avrion {

void AvrCpu::exec_jmp(u16 opcode) {
    // JMP
    // 1001 010k kkkk 110k
    // kkkk kkkk kkkk kkkk
    u32 k = ((opcode & 0x01F0) << 13) | ((opcode & 0x0001) << 16) | mem_.fetch16(st_.pc);
    set_pc(k << 1);
}

} // namespace avrion