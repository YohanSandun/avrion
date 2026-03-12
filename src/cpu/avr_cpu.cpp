#include "cpu/avr_cpu.h"
#include "core/interrupt_controller.h"
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

        // Check for pending interrupts before executing the next instruction
        u32 irq_cycles = service_interrupts();
        if (irq_cycles > 0)
            return irq_cycles;

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

        return (this->*(desc->exec))(opcode);
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

    u16 AvrCpu::z() const
    {
        return (static_cast<u16>(reg(cfg_.z_high_reg)) << 8) | reg(cfg_.z_low_reg);
    }

    void AvrCpu::set_z(u16 v)
    {
        set_reg(cfg_.z_low_reg, v & 0xFF);
        set_reg(cfg_.z_high_reg, (v >> 8) & 0xFF);
    }

    u32 AvrCpu::service_interrupts()
    {
        if (!irq_ || !(st_.sreg & 0x80) || !irq_->any_pending())
            return 0;

        u8 vec = irq_->highest_pending();
        if (vec == 0)
            return 0;

        irq_->clear(vec);

        // Disable global interrupts
        st_.sreg &= static_cast<u8>(~0x80);

        // Push return address (current PC) onto stack
        u32 ret = st_.pc;
        if (cfg_.has_22_bit_pc) {
            mem_.write8(--st_.sp, (ret >> 16) & 0xFF);
        }
        mem_.write8(--st_.sp, (ret >> 8) & 0xFF);
        mem_.write8(--st_.sp, ret & 0xFF);

        // Jump to interrupt vector address.
        // Each vector is 2 bytes (1 instruction word) apart on ATmega328P.
        // Vector byte address = vector_number * 2 (for 16-bit PC devices)
        // or vector_number * 4 (for 22-bit PC devices with 4-byte vectors).
        u32 vector_byte_addr;
        if (cfg_.has_22_bit_pc) {
            vector_byte_addr = static_cast<u32>(vec) * 4;
        } else {
            vector_byte_addr = static_cast<u32>(vec) * 2;
        }
        st_.pc = vector_byte_addr;

        return cfg_.has_22_bit_pc ? 5 : 4;
    }

} // namespace avrion