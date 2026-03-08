#include "cpu/avr_cpu.h"
#include "device/device_config.h"
#include "memory/memory_map.h"
#include "cpu/decoder.h"
#include <iomanip>
#include <sstream>
#include <stdexcept>

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
        if (st_.halted)
        {
            return 0;
        }

        u16 opcode = mem_.fetch16(st_.pc);

        return dispatch_and_exec(opcode);
    }

    u32 AvrCpu::dispatch_and_exec(u16 opcode)
    {
        const InstructionDesc *desc = lookup_instruction(opcode);
        if (!desc || !desc->exec)
        {
            std::ostringstream msg;
            msg << "Unknown or unimplemented opcode: 0x"
                << std::uppercase << std::hex << std::setw(4) << std::setfill('0') << opcode;
            throw std::runtime_error(msg.str());
        }

        (this->*(desc->exec))(opcode);
        return desc->cycles;
    }

} // namespace avrion