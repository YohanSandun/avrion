#pragma once
#include "core/types.h"
#include "core/interrupt_controller.h"
#include "device/device_config.h"
#include "memory/memory_map.h"
#include "cpu/avr_cpu.h"
#include "periph/peripheral.h"
#include <atomic>
#include <memory>
#include <unordered_map>
#include <string_view>

namespace avrion {

class Device {
public:
  explicit Device(DeviceConfig cfg);
  virtual ~Device() = default;

  const DeviceConfig& config() const { return cfg_; }

  void reset();
  void run_cycles(u64 cycles);
  // Runs the CPU indefinitely at the clock speed set in DeviceConfig::clock_hz,
  // throttling with sleep so emulated time tracks wall-clock time.
  // Returns when stop becomes true (e.g. on SIGINT).
  void run_realtime(const std::atomic<bool>& stop);

  u64 total_cycles() const { return total_cycles_; }

  // CpuSnapshot cpu_snapshot() const { return cpu_.snapshot(); }

  // Unified DATA space access for debugger/UI/tools
  u8  read_data(u16 addr) { return mem_.read8(addr); }
  void write_data(u16 addr, u8 v) { mem_.write8(addr, v); }

  // Flash loading
  void load_flash(u32 flash_byte_addr, const u8* data, usize len);

  InterruptController& irq_controller() { return irq_; }
  const InterruptController& irq_controller() const { return irq_; }

  // Returns a pointer to the peripheral registered under id, cast to T*.
  // Returns nullptr if not found or wrong type.
  template<typename T>
  T* get_peripheral(std::string_view id)
  {
      auto it = periphs_.find(id);
      if (it == periphs_.end()) return nullptr;
      return dynamic_cast<T*>(it->second.get());
  }

protected:
  // For derived devices: register peripherals and wire regions.
  // Typical usage: add_peripheral("PORTB", std::make_unique<GpioPort>());
  void add_peripheral(std::string_view id, std::unique_ptr<Peripheral> p);

  // Map regions declared in cfg_.io_regions to peripherals by id.
  void wire_from_config();

  // Called every time CPU advances cycles (for timers/uart). Override as needed.
  virtual void tick_peripherals(u32 cycles);

protected:
  DeviceConfig cfg_;
  InterruptController irq_;
  MemoryMap mem_;
  AvrCpu cpu_;

  std::unordered_map<std::string_view, std::unique_ptr<Peripheral>> periphs_;

  // Optional: keep a stable ordered list for ticking
  std::vector<Peripheral*> tick_list_;

  u64 total_cycles_ = 0;
};

} // namespace avrion