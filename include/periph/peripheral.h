#pragma once
#include "core/types.h"

namespace avrion {

class InterruptController;

class Peripheral {
public:
  virtual ~Peripheral() = default;

  virtual u8  read(u16 offset) = 0;
  virtual void write(u16 offset, u8 value) = 0;

  virtual void tick(u32 cycles) { (void)cycles; }

  // Attach interrupt controller so peripheral can raise IRQs.
  void set_irq_controller(InterruptController* irq) { irq_ = irq; }

protected:
  InterruptController* irq_ = nullptr;
};

} // namespace avrion