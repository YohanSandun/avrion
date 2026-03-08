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
    // {0xFC00, 0x0C00, "ADD",  1, &AvrCpu::exec_add},
    // {0xFC00, 0x1800, "SUB",  1, &AvrCpu::exec_sub},

    // branch
    // {0xF000, 0xC000, "RJMP", 2, &AvrCpu::exec_rjmp},
}};

const InstructionDesc* instruction_table_begin() {
    return kInstructionTable.data();
}

const InstructionDesc* instruction_table_end() {
    return kInstructionTable.data() + kInstructionTable.size();
}

} // namespace avrion