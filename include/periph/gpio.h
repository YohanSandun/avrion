#pragma once
#include "core/types.h"
#include "periph/peripheral.h"
#include <array>

namespace avrion {

struct PinState {
  bool is_output = false;
  bool level = false;
  bool pullup = false;
};

class GpioPort : public Peripheral {
public:
  enum RegOffset : u16 { PIN = 0, DDR = 1, PORT = 2 };

  u8  read(u16 offset) override;
  void write(u16 offset, u8 value) override;

  void set_input_level(int bit, bool high);

  std::array<PinState, 8> snapshot() const;

private:
  // AVR-like internal regs
  u8 pin_latched_ = 0;  // reflects current pin levels (mix of output + ext)
  u8 ddr_ = 0;          // 1=output
  u8 port_ = 0;         // output latch / pull-up enable bits
  u8 ext_in_ = 0;       // external driven inputs

  void recompute_pins();
};

} // namespace avrion