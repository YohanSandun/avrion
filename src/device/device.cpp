#include <stdlib.h>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <cstdio>
#include "device/device.h"
#include "device/device_config.h"
#include "periph/peripheral.h"
#include "memory/memory_map.h"

namespace avrion
{
    Device::Device(DeviceConfig cfg) : cfg_(std::move(cfg)), mem_(cfg_), cpu_(mem_, cfg_)
    {
        mem_.attach_cpu(&cpu_);
        cpu_.set_irq_controller(&irq_);
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
            total_cycles_ += cycles_this_instr;
            if (cycles_this_instr <= cycles)
                cycles -= cycles_this_instr;
            else
                cycles = 0;
        }
    }

    void Device::run_realtime(const std::atomic<bool>& stop)
    {
        if (cfg_.clock_hz == 0)
            throw std::runtime_error("run_realtime: clock_hz not set in DeviceConfig");

        using clock     = std::chrono::steady_clock;
        using ns_t      = std::chrono::nanoseconds;

        // ~1600 cycles ≈ 100 µs at 16 MHz — small enough for responsive stop,
        // large enough to keep loop overhead low.
        constexpr u64 CHUNK = 1600;

        auto wall_start          = clock::now();
        u64  start_cycles         = total_cycles_;
        u64  next_heartbeat_cycle = start_cycles + cfg_.clock_hz; // every 1s emulated

        while (!stop.load(std::memory_order_relaxed))
        {
            // Run one chunk
            u64 chunk_remaining = CHUNK;
            while (chunk_remaining > 0)
            {
                u32 c = cpu_.step_instruction();
                tick_peripherals(c);
                total_cycles_ += c;
                chunk_remaining -= (c < chunk_remaining) ? c : chunk_remaining;
            }

            // Throttle: sleep until wall time catches up with emulated time.
            u64 emulated = total_cycles_ - start_cycles;
            auto expected = ns_t(emulated * 1'000'000'000ULL / cfg_.clock_hz);
            auto elapsed  = clock::now() - wall_start;
            if (expected > elapsed)
                std::this_thread::sleep_for(expected - elapsed);

            // Heartbeat: print timing stats every 1s of emulated time.
            // if (total_cycles_ >= next_heartbeat_cycle)
            // {
            //     next_heartbeat_cycle += cfg_.clock_hz;
            //     auto wall_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            //                        clock::now() - wall_start).count();
            //     double emu_s = static_cast<double>(emulated) / cfg_.clock_hz;
            //     std::fprintf(stderr, "[timing] %.3fs emulated | %lldms wall | %llu total cycles\n",
            //                  emu_s, (long long)wall_ms, (unsigned long long)total_cycles_);
            //     std::fflush(stderr);
            // }
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

        p->set_irq_controller(&irq_);

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
            mem_.map_peripheral(region.data_base, region.length, it->second.get(), region.periph_offset);
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