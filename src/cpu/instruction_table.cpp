#include "cpu/instruction_desc.h"
#include "cpu/avr_cpu.h"
#include <array>

namespace avrion {

static const std::array<InstructionDesc, 22> kInstructionTable = {{
    // mask  pat     name       cyc16  cyc22
    // misc                
    {0xFFFF, 0x0000, "NOP",     1, 1, &AvrCpu::exec_nop},
    {0xFFFF, 0x9478, "SEI",     1, 1, &AvrCpu::exec_sei},

    // data transfer
    {0xF800, 0xB800, "OUT",     1, 1, &AvrCpu::exec_out},
    {0xF800, 0xB000, "IN",      1, 1, &AvrCpu::exec_in},
    {0xF000, 0xE000, "LDI",     1, 1, &AvrCpu::exec_ldi},
    {0xFE0F, 0x920C, "ST X",    1, 1, &AvrCpu::exec_st_x},
    {0xFE0F, 0x920D, "ST X+",   1, 1, &AvrCpu::exec_st_x_post_inc},
    {0xFE0F, 0x920E, "ST -X",   1, 1, &AvrCpu::exec_st_x_pre_dec},
    {0xFE0F, 0x9000, "LDS",     2, 2, &AvrCpu::exec_lds},
    {0xFE0F, 0x9200, "STS",     2, 2, &AvrCpu::exec_sts},
    {0xFFFF, 0x95C8, "LPM",     3, 3, &AvrCpu::exec_lpm},
    {0xFE0F, 0x9004, "LPM Z",   3, 3, &AvrCpu::exec_lpm_z},
    {0xFE0F, 0x9005, "LPM Z+",  3, 3, &AvrCpu::exec_lpm_z_post_inc},

    // alu
    {0xFC00, 0x2400, "EOR",     1, 1, &AvrCpu::exec_eor},
    {0xF000, 0x3000, "CPI",     1, 1, &AvrCpu::exec_cpi},
    {0xFC00, 0x0400, "CPC",     1, 1, &AvrCpu::exec_cpc},
    {0xF000, 0x6000, "ORI",     1, 1, &AvrCpu::exec_ori},
    {0xFC00, 0x2000, "AND",     1, 1, &AvrCpu::exec_and},

    // branch
    {0xFE0E, 0x940C, "JMP",     3, 4, &AvrCpu::exec_jmp},
    {0xF000, 0xC000, "RJMP",    2, 2, &AvrCpu::exec_rjmp},
    {0xFC07, 0xF401, "BRNE",    2, 2, &AvrCpu::exec_brne},
    {0xFE0E, 0x940E, "CALL",    3, 4, &AvrCpu::exec_call}
}};

const InstructionDesc* instruction_table_begin() {
    return kInstructionTable.data();
}

const InstructionDesc* instruction_table_end() {
    return kInstructionTable.data() + kInstructionTable.size();
}

} // namespace avrion