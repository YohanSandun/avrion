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
  // Calls on_irq_controller_set() so subclasses can register per-vector
  // flag-clear callbacks at attachment time.
  void set_irq_controller(InterruptController* irq) {
    irq_ = irq;
    on_irq_controller_set();
  }

  // Override to perform post-attachment registration (e.g. flag-clear
  // callbacks). Called immediately after irq_ is assigned.
  virtual void on_irq_controller_set() {}

protected:
  InterruptController* irq_ = nullptr;
};

} // namespace avrion