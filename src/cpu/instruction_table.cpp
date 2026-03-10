#include "cpu/instruction_desc.h"
#include "cpu/avr_cpu.h"
#include <array>

namespace avrion {

static const std::array<InstructionDesc, 36> kInstructionTable = {{
    // mask  pattern name
    // misc
    {0xFFFF, 0x0000, "NOP",    &AvrCpu::exec_nop},
    {0xFFFF, 0x9478, "SEI",    &AvrCpu::exec_sei},
    {0xFFFF, 0x94F8, "CLI",    &AvrCpu::exec_cli},

    // data transfer
    {0xF800, 0xB800, "OUT",    &AvrCpu::exec_out},
    {0xF800, 0xB000, "IN",     &AvrCpu::exec_in},
    {0xF000, 0xE000, "LDI",    &AvrCpu::exec_ldi},
    {0xFE0F, 0x920C, "ST X",   &AvrCpu::exec_st_x},
    {0xFE0F, 0x920D, "ST X+",  &AvrCpu::exec_st_x_post_inc},
    {0xFE0F, 0x920E, "ST -X",  &AvrCpu::exec_st_x_pre_dec},
    {0xFE0F, 0x9000, "LDS",    &AvrCpu::exec_lds,  true},
    {0xFE0F, 0x9200, "STS",    &AvrCpu::exec_sts,  true},
    {0xFFFF, 0x95C8, "LPM",    &AvrCpu::exec_lpm},
    {0xFE0F, 0x9004, "LPM Z",  &AvrCpu::exec_lpm_z},
    {0xFE0F, 0x9005, "LPM Z+", &AvrCpu::exec_lpm_z_post_inc},
    {0xFF00, 0x0100, "MOVW",   &AvrCpu::exec_movw},
    {0xFE0F, 0x900C, "LD X",   &AvrCpu::exec_ld_x},
    {0xFE0F, 0x900D, "LD X+",  &AvrCpu::exec_ld_x_post_inc},
    {0xFE0F, 0x900E, "LD -X",  &AvrCpu::exec_ld_x_pre_dec},
    {0xFE0F, 0x920F, "PUSH",   &AvrCpu::exec_push},

    // alu
    {0xFC00, 0x2400, "EOR",    &AvrCpu::exec_eor},
    {0xF000, 0x3000, "CPI",    &AvrCpu::exec_cpi},
    {0xFC00, 0x0400, "CPC",    &AvrCpu::exec_cpc},
    {0xF000, 0x6000, "ORI",    &AvrCpu::exec_ori},
    {0xFC00, 0x2000, "AND",    &AvrCpu::exec_and},
    {0xFC00, 0x0C00, "ADD",    &AvrCpu::exec_add},
    {0xFC00, 0x1C00, "ADC",    &AvrCpu::exec_adc},
    {0xF000, 0x5000, "SUBI",   &AvrCpu::exec_subi},
    {0xF000, 0x4000, "SBCI",   &AvrCpu::exec_sbci},
    {0xFC00, 0x2800, "OR",     &AvrCpu::exec_or},

    // branch
    {0xFE0E, 0x940C, "JMP",    &AvrCpu::exec_jmp,  true},
    {0xF000, 0xC000, "RJMP",   &AvrCpu::exec_rjmp},
    {0xFC07, 0xF401, "BRNE",   &AvrCpu::exec_brne},
    {0xFE0E, 0x940E, "CALL",   &AvrCpu::exec_call, true},
    {0xFC07, 0xF001, "BREQ",   &AvrCpu::exec_breq},
    {0xFC00, 0x1000, "CPSE",   &AvrCpu::exec_cpse},
    {0xFFFF, 0x9508, "RET",    &AvrCpu::exec_ret}
}};

const InstructionDesc* instruction_table_begin() {
    return kInstructionTable.data();
}

const InstructionDesc* instruction_table_end() {
    return kInstructionTable.data() + kInstructionTable.size();
}

} // namespace avrion