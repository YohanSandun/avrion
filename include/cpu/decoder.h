#pragma once
#include "core/types.h"
#include "cpu/instruction_desc.h"

namespace avrion
{

void initialize_decoder_lut();

// O(1) lookup
const InstructionDesc* lookup_instruction(u16 opcode);

// Access raw instruction table if needed for debugging/tests
const InstructionDesc* instruction_table_begin();
const InstructionDesc* instruction_table_end();

} // namespace avrion