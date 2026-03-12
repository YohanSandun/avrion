#pragma once
#include "periph/peripheral.h"
#include "core/interrupt_controller.h"
#include <cstdint>

namespace avrion {

// Interrupt vector numbers for an 8-bit timer.
// Set to 0 to disable a particular interrupt source.
struct Timer8Vectors {
  u8 ovf  = 0;  // Timer overflow
  u8 compa = 0; // Output compare A match
  u8 compb = 0; // Output compare B match
};

// Register indices (offsets) within the timer peripheral.
// The Device maps scattered IO regions to these indices via periph_offset.
enum Timer8Reg : u16 {
  TCCRxA = 0,
  TCCRxB = 1,
  TCNTx  = 2,
  OCRxA  = 3,
  OCRxB  = 4,
  TIMSKx = 5,
  TIFRx  = 6,
};

// TCCR0A bits
static constexpr u8 kWGM00 = 0x01;
static constexpr u8 kWGM01 = 0x02;

// TCCR0B bits
static constexpr u8 kCS00  = 0x01;
static constexpr u8 kCS01  = 0x02;
static constexpr u8 kCS02  = 0x04;
static constexpr u8 kWGM02 = 0x08;

// TIMSKx bits
static constexpr u8 kTOIE  = 0x01;  // overflow interrupt enable
static constexpr u8 kOCIEA = 0x02;  // output compare A interrupt enable
static constexpr u8 kOCIEB = 0x04;  // output compare B interrupt enable

// TIFRx bits
static constexpr u8 kTOV   = 0x01;  // overflow flag
static constexpr u8 kOCFA  = 0x02;  // output compare A flag
static constexpr u8 kOCFB  = 0x04;  // output compare B flag

// Waveform generation modes (3-bit from WGM02:WGM01:WGM00)
enum class WaveformMode : u8 {
  Normal          = 0, // TOP=0xFF
  PhaseCorrectPWM = 1, // TOP=0xFF
  CTC             = 2, // TOP=OCRxA
  FastPWM         = 3, // TOP=0xFF
  // Mode 4 is reserved
  PhaseCorrectPWM_OCRA = 5, // TOP=OCRxA
  // Mode 6 is reserved
  FastPWM_OCRA    = 7, // TOP=OCRxA
};

// Clock select (CS02:CS01:CS00)
enum class ClockSelect : u8 {
  Stopped  = 0,
  Div1     = 1,
  Div8     = 2,
  Div64    = 3,
  Div256   = 4,
  Div1024  = 5,
  ExtFall  = 6, // external clock on T0 pin, falling edge
  ExtRise  = 7, // external clock on T0 pin, rising edge
};

// Timer8 handles reads/writes via register index dispatching.
// The Device maps each non-contiguous register group as a separate IO region
// with appropriate periph_offset values, so the timer always receives a
// consistent register index (0-6) regardless of the actual IO address.
class Timer8 : public Peripheral {
public:
  explicit Timer8(Timer8Vectors vectors);

  u8   read(u16 offset) override;
  void write(u16 offset, u8 value) override;
  void tick(u32 cycles) override;

private:
  Timer8Vectors vectors_;

  // Registers
  u8 tccra_ = 0;
  u8 tccrb_ = 0;
  u8 tcnt_  = 0;
  u8 ocra_  = 0;
  u8 ocrb_  = 0;
  u8 timsk_ = 0;
  u8 tifr_  = 0;

  // Internal prescaler accumulator
  u32 prescaler_accum_ = 0;

  // Phase-correct PWM direction: true = counting up, false = counting down
  bool counting_up_ = true;

  // Helpers
  WaveformMode waveform_mode() const;
  ClockSelect  clock_select() const;
  u32          prescaler_divisor() const;
  u8           top_value() const;

  void advance_timer(u32 timer_ticks);
  void check_interrupts();
};

} // namespace avrion
