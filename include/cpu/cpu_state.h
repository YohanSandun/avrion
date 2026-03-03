#pragma once
#include "core/types.h"
#include <array>

namespace avrion {

struct CpuState {
  std::array<u8, 32> r{};
  u16 pc = 0;
  u8  sreg = 0;
  u16 sp = 0;

  bool halted = false;
};

struct CpuSnapshot {
  std::array<u8, 32> r{};
  u16 pc = 0;
  u8  sreg = 0;
  u16 sp = 0;
};

} // namespace avrion