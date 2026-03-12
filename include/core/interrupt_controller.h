#pragma once
#include "core/types.h"
#include <bitset>

namespace avrion {

// ATmega328P has 26 interrupt vectors (vector 0 = RESET).
// This controller manages pending + enabled flags for all IRQs.
// Vector numbers follow the datasheet (1-based), with 0 = RESET.
static constexpr u8 kMaxInterruptVectors = 32;

class InterruptController {
public:
  void raise(u8 vector) {
    if (vector > 0 && vector < kMaxInterruptVectors)
      pending_.set(vector);
  }

  void clear(u8 vector) {
    if (vector < kMaxInterruptVectors)
      pending_.reset(vector);
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
};

} // namespace avrion
