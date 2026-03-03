#pragma once
#include "core/types.h"

namespace avrion {

class Peripheral {
public:
  virtual ~Peripheral() = default;

  virtual u8  read(u16 offset) = 0;
  virtual void write(u16 offset, u8 value) = 0;

  virtual void tick(u32 cycles) { (void)cycles; }
};

} // namespace avrion