#include <stdlib.h>
#include <stdexcept>
#include "device/device.h"
#include "device/device_config.h"
#include "periph/peripheral.h"
#include "memory/memory_map.h"

namespace avrion
{
    Device::Device(DeviceConfig cfg) : cfg_(std::move(cfg)), mem_(cfg_), cpu_(mem_, cfg_)
    {
        mem_.attach_cpu(&cpu_);
    }

    void Device::reset()
    {
        cpu_.reset();
        tick_peripherals(0);
    }

    void Device::run_cycles(u64 cycles)
    {
        while (cycles > 0)
        {
            u32 cycles_this_instr = cpu_.step_instruction();
            tick_peripherals(cycles_this_instr);
            cycles -= cycles_this_instr;
        }
    }

    void Device::load_flash(u32 flash_byte_addr, const u8 *data, usize len)
    {
        if (!data || len == 0)
            return;

        auto &flash = mem_.flash();
        if (flash_byte_addr >= flash.size())
            return;

        const usize max_copy = std::min<usize>(len, flash.size() - static_cast<usize>(flash_byte_addr));
        std::memcpy(flash.data() + flash_byte_addr, data, max_copy);
    }

    void Device::add_peripheral(std::string_view id, std::unique_ptr<Peripheral> p)
    {
        if (!p)
            throw std::invalid_argument("add_peripheral: null peripheral");
        if (id.empty())
            throw std::invalid_argument("add_peripheral: empty id");

        auto [it, inserted] = periphs_.emplace(id, std::move(p));
        if (!inserted)
        {
            throw std::runtime_error("add_peripheral: duplicate peripheral id");
        }

        tick_list_.push_back(it->second.get());
    }

    void Device::wire_from_config()
    {
        for (const auto &region : cfg_.io_regions)
        {
            auto it = periphs_.find(region.id);
            if (it == periphs_.end())
            {
                throw std::runtime_error("wire_from_config: missing peripheral for region id");
            }
            mem_.map_peripheral(region.data_base, region.length, it->second.get());
        }
    }

    void Device::tick_peripherals(u32 cycles)
    {
        for (auto *p : tick_list_)
        {
            p->tick(cycles);
        }
    }
} // namespace avrion