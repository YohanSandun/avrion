#include "cpu/instruction_desc.h"
#include "cpu/avr_cpu.h"
#include <array>

namespace avrion {

static const std::array<InstructionDesc, 27> kInstructionTable = {{
    // mask  pattern name
    // misc
    {0xFFFF, 0x0000, "NOP",    &AvrCpu::exec_nop},
    {0xFFFF, 0x9478, "SEI",    &AvrCpu::exec_sei},

    // data transfer
    {0xF800, 0xB800, "OUT",    &AvrCpu::exec_out},
    {0xF800, 0xB000, "IN",     &AvrCpu::exec_in},
    {0xF000, 0xE000, "LDI",    &AvrCpu::exec_ldi},
    {0xFE0F, 0x920C, "ST X",   &AvrCpu::exec_st_x},
    {0xFE0F, 0x920D, "ST X+",  &AvrCpu::exec_st_x_post_inc},
    {0xFE0F, 0x920E, "ST -X",  &AvrCpu::exec_st_x_pre_dec},
    {0xFE0F, 0x9000, "LDS",    &AvrCpu::exec_lds},
    {0xFE0F, 0x9200, "STS",    &AvrCpu::exec_sts},
    {0xFFFF, 0x95C8, "LPM",    &AvrCpu::exec_lpm},
    {0xFE0F, 0x9004, "LPM Z",  &AvrCpu::exec_lpm_z},
    {0xFE0F, 0x9005, "LPM Z+", &AvrCpu::exec_lpm_z_post_inc},
    {0xFF00, 0x0100, "MOVW",   &AvrCpu::exec_movw},

    // alu
    {0xFC00, 0x2400, "EOR",    &AvrCpu::exec_eor},
    {0xF000, 0x3000, "CPI",    &AvrCpu::exec_cpi},
    {0xFC00, 0x0400, "CPC",    &AvrCpu::exec_cpc},
    {0xF000, 0x6000, "ORI",    &AvrCpu::exec_ori},
    {0xFC00, 0x2000, "AND",    &AvrCpu::exec_and},
    {0xFC00, 0x0C00, "ADD",    &AvrCpu::exec_add},
    {0xFC00, 0x1C00, "ADC",    &AvrCpu::exec_adc},
    {0xF000, 0x5000, "SUBI",   &AvrCpu::exec_subi},

    // branch
    {0xFE0E, 0x940C, "JMP",    &AvrCpu::exec_jmp},
    {0xF000, 0xC000, "RJMP",   &AvrCpu::exec_rjmp},
    {0xFC07, 0xF401, "BRNE",   &AvrCpu::exec_brne},
    {0xFE0E, 0x940E, "CALL",   &AvrCpu::exec_call},
    {0xFC07, 0xF001, "BREQ",   &AvrCpu::exec_breq}
}};

const InstructionDesc* instruction_table_begin() {
    return kInstructionTable.data();
}

const InstructionDesc* instruction_table_end() {
    return kInstructionTable.data() + kInstructionTable.size();
}

} // namespace avrion