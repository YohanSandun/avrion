#include "cpu/instruction_desc.h"
#include "cpu/avr_cpu.h"
#include <array>

namespace avrion {

static const std::array<InstructionDesc, 12> kInstructionTable = {{
    // misc
    {0xFFFF, 0x0000, "NOP",  1, &AvrCpu::exec_nop},

    // data transfer
    {0xF800, 0xB800, "OUT",  1, &AvrCpu::exec_out},
    {0xF000, 0xE000, "LDI",  1, &AvrCpu::exec_ldi},
    {0xFE0F, 0x920C, "ST X",  1, &AvrCpu::exec_st_x},
    {0xFE0F, 0x920D, "ST X+",  1, &AvrCpu::exec_st_x_post_inc},
    {0xFE0F, 0x920E, "ST -X",  1, &AvrCpu::exec_st_x_pre_dec},

    // alu
    {0xFC00, 0x2400, "EOR",  1, &AvrCpu::exec_eor},
    {0xF000, 0x3000, "CPI",  1, &AvrCpu::exec_cpi},
    {0xFC00, 0x0400, "CPC",  1, &AvrCpu::exec_cpc},

    // branch
    {0xFE0E, 0x940C, "JMP", 3, &AvrCpu::exec_jmp},
    {0xF000, 0xC000, "RJMP", 2, &AvrCpu::exec_rjmp},
    {0xFC07, 0xF401, "BRNE", 2, &AvrCpu::exec_brne}
}};

const InstructionDesc* instruction_table_begin() {
    return kInstructionTable.data();
}

const InstructionDesc* instruction_table_end() {
    return kInstructionTable.data() + kInstructionTable.size();
}

} // namespace avrion