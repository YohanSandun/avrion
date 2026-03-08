#include "cpu/instruction_desc.h"
#include "cpu/avr_cpu.h"
#include <array>

namespace avrion {

static const std::array<InstructionDesc, 6> kInstructionTable = {{
    // misc
    {0xFFFF, 0x0000, "NOP",  1, &AvrCpu::exec_nop},

    // data transfer
    // {0xF000, 0xE000, "LDI",  1, &AvrCpu::exec_ldi},
    // {0xFC00, 0x2C00, "MOV",  1, &AvrCpu::exec_mov},

    // alu
    {0xFC00, 0x2400, "EOR",  1, &AvrCpu::exec_eor},

    // branch
    {0xFE0E, 0x940C, "JMP", 3, &AvrCpu::exec_jmp},
}};

const InstructionDesc* instruction_table_begin() {
    return kInstructionTable.data();
}

const InstructionDesc* instruction_table_end() {
    return kInstructionTable.data() + kInstructionTable.size();
}

} // namespace avrion