#pragma once
#include "core/types.h"
#include <array>
#include <bitset>
#include <functional>

namespace avrion {

// ATmega328P has 26 interrupt vectors (vector 0 = RESET).
// This controller manages pending + enabled flags for all IRQs.
// Vector numbers follow the datasheet (1-based), with 0 = RESET.
static constexpr u8 kMaxInterruptVectors = 32;

class InterruptController {
public:
  using FlagClearFn = std::function<void()>;

  void raise(u8 vector) {
    if (vector > 0 && vector < kMaxInterruptVectors)
      pending_.set(vector);
  }

  // Register a callback invoked when the CPU vectors to this interrupt.
  // Used by peripherals to clear their hardware flag at the correct moment
  // (identical to real AVR behaviour: flag clears when ISR is entered, not
  // when the request is first posted).
  void register_flag_clear(u8 vector, FlagClearFn fn) {
    if (vector > 0 && vector < kMaxInterruptVectors)
      flag_clears_[vector] = std::move(fn);
  }

  void clear(u8 vector) {
    if (vector < kMaxInterruptVectors) {
      pending_.reset(vector);
      // Clear the corresponding hardware flag now that the CPU has vectored.
      if (flag_clears_[vector])
        flag_clears_[vector]();
    }
  }

  bool any_pending() const { return pending_.any(); }

  u8 highest_pending() const {
    for (u8 i = 1; i < kMaxInterruptVectors; ++i) {
      if (pending_.test(i))
        return i;
    }
    return 0;
  }

  bool is_pending(u8 vector) const {
    return vector < kMaxInterruptVectors && pending_.test(vector);
  }

  void clear_all() { pending_.reset(); }

private:
  std::bitset<kMaxInterruptVectors> pending_;
  std::array<FlagClearFn, kMaxInterruptVectors> flag_clears_{};
};

} // namespace avrion
