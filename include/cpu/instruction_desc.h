#pragma once
#include "core/types.h"
#include "cpu/avr_cpu.h"

namespace avrion {

using InstrExecFn = u8 (AvrCpu::*)(u16 opcode);

struct InstructionDesc {
    u16 mask;
    u16 pattern;
    const char* name;
    InstrExecFn exec;
    bool is_two_word = false;
};

} // namespace avrion