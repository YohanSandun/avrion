#pragma once
#include "core/types.h"
#include "device/device_config.h"
#include <vector>

namespace avrion {

class Peripheral;
class AvrCpu;

class MemoryMap {
public:
  explicit MemoryMap(const DeviceConfig& cfg);

  void attach_cpu(AvrCpu* cpu);

  // FLASH
  u16 fetch16(u32 flash_byte_addr) const;
  std::vector<u8>&       flash() { return flash_; }
  const std::vector<u8>& flash() const { return flash_; }

  // DATA space
  u8  read8(u16 data_addr);
  void write8(u16 data_addr, u8 value);

  bool is_sram(u16 data_addr) const;
  u8*  sram_ptr(u16 data_addr);

  void map_peripheral(u16 data_base, u16 length, Peripheral* p);

  const DeviceConfig& cfg() const { return cfg_; }

private:
  struct Mapping { u16 base; u16 len; Peripheral* p; };

  const DeviceConfig& cfg_;
  std::vector<u8> sram_;
  std::vector<u8> flash_;
  std::vector<Mapping> mappings_;
  AvrCpu* cpu_ = nullptr;

  Peripheral* find_peripheral(u16 data_addr, u16& out_off);
};

} // namespace avrion