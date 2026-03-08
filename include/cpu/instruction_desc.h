#pragma once
#include "core/types.h"
#include "cpu/avr_cpu.h"

namespace avrion {

using InstrExecFn = void (AvrCpu::*)(u16 opcode);

struct InstructionDesc {
    u16 mask;
    u16 pattern;
    const char* name;
    u8 cycles;
    InstrExecFn exec;
};

} // namespace avrion