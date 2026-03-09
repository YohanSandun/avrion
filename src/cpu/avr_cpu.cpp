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
        set_pc(st_.pc + 2);

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
        const u8 cycles = cfg_.has_22_bit_pc ? desc->cycles_22bit_pc : desc->cycles;
        return cycles;
    }

    u16 AvrCpu::x() const
    {
        return (static_cast<u16>(reg(cfg_.x_high_reg)) << 8) | reg(cfg_.x_low_reg);
    }

    void AvrCpu::set_x(u16 v)
    {
        set_reg(cfg_.x_low_reg, v & 0xFF);
        set_reg(cfg_.x_high_reg, (v >> 8) & 0xFF);
    }

} // namespace avrion