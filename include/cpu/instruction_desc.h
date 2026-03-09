#pragma once
#include "core/types.h"
#include "cpu/avr_cpu.h"

namespace avrion {

using InstrExecFn = void (AvrCpu::*)(u16 opcode);

struct InstructionDesc {
    u16 mask;
    u16 pattern;
    const char* name;
    u8 cycles;          // cycles for 16-bit PC devices
    u8 cycles_22bit_pc; // cycles for 22-bit PC devices (0 = same as cycles)
    InstrExecFn exec;
};

} // namespace avrion