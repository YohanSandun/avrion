#include "memory/memory_map.h"
#include "periph/peripheral.h"
#include "cpu/avr_cpu.h"

namespace avrion
{
    MemoryMap::MemoryMap(const DeviceConfig &cfg) : cfg_(cfg)
    {
        sram_.resize(cfg_.sram_size_bytes);
        flash_.resize(cfg_.flash_size_bytes);
    }

    void MemoryMap::attach_cpu(AvrCpu *cpu)
    {
        cpu_ = cpu;
    }

    u16 MemoryMap::fetch16(u32 flash_byte_addr) const
    {
        if (flash_byte_addr + 1 >= flash_.size())
        {
            return 0;
        }
        return flash_[flash_byte_addr] | (flash_[flash_byte_addr + 1] << 8);
    }

    u8 MemoryMap::read8(u16 data_addr)
    {
        // SRAM
        if (is_sram(data_addr))
        {
            return sram_[data_addr - cfg_.sram_base];
        }

        // GPR window
        if (data_addr >= cfg_.gpr_base && data_addr < cfg_.gpr_base + cfg_.gpr_count)
        {
            const u8 idx = static_cast<u8>(data_addr - cfg_.gpr_base);
            return cpu_->reg(idx);
        }

        // CPU-mapped special regs (common ones)
        if (data_addr == cfg_.sreg_data_addr)
            return cpu_->sreg();

        if (data_addr == cfg_.spl_data_addr)
        {
            return static_cast<u8>(cpu_->sp() & 0x00FF);
        }
        if (cfg_.sph_data_addr != 0 && data_addr == cfg_.sph_data_addr)
        {
            return static_cast<u8>((cpu_->sp() >> 8) & 0x00FF);
        }

        // Peripherals (IO and extended IO) via mapping table
        u16 off = 0;
        if (auto *p = find_peripheral(data_addr, off))
        {
            return p->read(off);
        }

        // Unmapped
        return 0xFF;
    }

    void MemoryMap::write8(u16 data_addr, u8 value)
    {
        // SRAM
        if (is_sram(data_addr))
        {
            sram_[data_addr - cfg_.sram_base] = value;
            return;
        }

        // GPR window
        if (data_addr >= cfg_.gpr_base && data_addr < cfg_.gpr_base + cfg_.gpr_count)
        {
            const u8 idx = static_cast<u8>(data_addr - cfg_.gpr_base);
            cpu_->set_reg(idx, value);
            return;
        }

        // CPU-mapped special regs
        if (data_addr == cfg_.sreg_data_addr)
        {
            cpu_->set_sreg(value);
            return;
        }

        if (data_addr == cfg_.spl_data_addr)
        {
            u16 sp = cpu_->sp();
            sp = static_cast<u16>((sp & 0xFF00) | value);
            cpu_->set_sp(sp);
            return;
        }
        if (cfg_.sph_data_addr != 0 && data_addr == cfg_.sph_data_addr)
        {
            u16 sp = cpu_->sp();
            sp = static_cast<u16>((sp & 0x00FF) | (static_cast<u16>(value) << 8));
            cpu_->set_sp(sp);
            return;
        }

        // Peripherals
        u16 off = 0;
        if (auto *p = find_peripheral(data_addr, off))
        {
            p->write(off, value);
            return;
        }
    }

    bool MemoryMap::is_sram(u16 data_addr) const
    {
        return data_addr >= cfg_.sram_base && data_addr < cfg_.sram_base + cfg_.sram_size_bytes;
    }

    u8 *MemoryMap::sram_ptr(u16 data_addr)
    {
        if (!is_sram(data_addr))
        {
            return nullptr;
        }
        return &sram_[data_addr - cfg_.sram_base];
    }

    void MemoryMap::map_peripheral(u16 data_base, u16 length, Peripheral *p)
    {
        mappings_.push_back({data_base, length, p});
    }

    Peripheral *MemoryMap::find_peripheral(u16 data_addr, u16 &out_off)
    {
        for (const auto &m : mappings_)
        {
            if (data_addr >= m.base && data_addr < m.base + m.len)
            {
                out_off = data_addr - m.base;
                return m.p;
            }
        }
        return nullptr;
    }

} // namespace avrion