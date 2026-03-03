#include "cpu/avr_cpu.h"
#include "device/device_config.h"
#include "memory/memory_map.h"

namespace avrion
{
    AvrCpu::AvrCpu(MemoryMap &mem, const DeviceConfig &cfg)
        : mem_(mem), cfg_(cfg)
    {
        reset();
    }

    void AvrCpu::reset()
    {
        st_ = {};
        st_.pc = 0;
        st_.sreg = 0;
        st_.sp = cfg_.sram_size_bytes - 1;
    }

    u32 AvrCpu::step_instruction()
    {
        if (st_.halted) {
            return 0;
        }

        u16 opcode = mem_.fetch16(st_.pc);
        st_.pc += 2;

        return dispatch_and_exec(opcode);
    }

    u32 AvrCpu::dispatch_and_exec(u16 opcode)
    {
        return 0;
    }

} // namespace avrion