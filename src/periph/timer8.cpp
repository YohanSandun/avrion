#include "periph/timer8.h"

namespace avrion {

Timer8::Timer8(Timer8Vectors vectors)
    : vectors_(vectors) {}

void Timer8::on_irq_controller_set() {
  if (!irq_) return;

  // Register callbacks that clear the hardware TIFR flag when the CPU
  // actually vectors to each ISR.
  if (vectors_.ovf)
    irq_->register_flag_clear(vectors_.ovf,   [this]{ tifr_ &= static_cast<u8>(~kTOV);  });
  if (vectors_.compa)
    irq_->register_flag_clear(vectors_.compa, [this]{ tifr_ &= static_cast<u8>(~kOCFA); });
  if (vectors_.compb)
    irq_->register_flag_clear(vectors_.compb, [this]{ tifr_ &= static_cast<u8>(~kOCFB); });
}

u8 Timer8::read(u16 offset) {
  switch (offset) {
  case TCCRxA: return tccra_;
  case TCCRxB: return tccrb_;
  case TCNTx:  return tcnt_;
  case OCRxA:  return ocra_;
  case OCRxB:  return ocrb_;
  case TIMSKx: return timsk_;
  case TIFRx:  return tifr_;
  default:     return 0xFF;
  }
}

void Timer8::write(u16 offset, u8 value) {
  switch (offset) {
  case TCCRxA: tccra_ = value; break;
  case TCCRxB: tccrb_ = value; break;
  case TCNTx:  tcnt_ = value; prescaler_accum_ = 0; break;
  case OCRxA:  ocra_ = value; break;
  case OCRxB:  ocrb_ = value; break;
  case TIMSKx: timsk_ = value; break;
  case TIFRx:
    // Writing 1 to a flag bit clears it (standard AVR behavior)
    tifr_ &= static_cast<u8>(~value);
    break;
  default: break;
  }
}

void Timer8::tick(u32 cycles) {
  if (cycles == 0) return;

  ClockSelect cs = clock_select();
  if (cs == ClockSelect::Stopped || cs == ClockSelect::ExtFall || cs == ClockSelect::ExtRise)
    return;

  u32 divisor = prescaler_divisor();
  prescaler_accum_ += cycles;

  u32 timer_ticks = prescaler_accum_ / divisor;
  prescaler_accum_ %= divisor;

  if (timer_ticks > 0)
    advance_timer(timer_ticks);
}

WaveformMode Timer8::waveform_mode() const {
  u8 wgm = (tccra_ & (kWGM00 | kWGM01)) | ((tccrb_ & kWGM02) ? 0x04 : 0x00);
  return static_cast<WaveformMode>(wgm);
}

ClockSelect Timer8::clock_select() const {
  return static_cast<ClockSelect>(tccrb_ & 0x07);
}

u32 Timer8::prescaler_divisor() const {
  switch (clock_select()) {
  case ClockSelect::Div1:    return 1;
  case ClockSelect::Div8:    return 8;
  case ClockSelect::Div64:   return 64;
  case ClockSelect::Div256:  return 256;
  case ClockSelect::Div1024: return 1024;
  default:                   return 1;
  }
}

u8 Timer8::top_value() const {
  switch (waveform_mode()) {
  case WaveformMode::CTC:
  case WaveformMode::FastPWM_OCRA:
  case WaveformMode::PhaseCorrectPWM_OCRA:
    return ocra_;
  default:
    return 0xFF;
  }
}

void Timer8::advance_timer(u32 timer_ticks) {
  WaveformMode mode = waveform_mode();
  u8 top = top_value();

  for (u32 t = 0; t < timer_ticks; ++t) {
    switch (mode) {
    case WaveformMode::Normal:
    case WaveformMode::CTC:
    case WaveformMode::FastPWM:
    case WaveformMode::FastPWM_OCRA: {
      tcnt_++;

      // Check compare matches before overflow
      if (tcnt_ == ocra_)
        tifr_ |= kOCFA;
      if (tcnt_ == ocrb_)
        tifr_ |= kOCFB;

      if (mode == WaveformMode::Normal) {
        // Overflow at 0xFF -> 0x00
        if (tcnt_ == 0) {
          tifr_ |= kTOV;
        }
      } else if (mode == WaveformMode::CTC) {
        // Clear on compare match with OCRxA, overflow at TOP
        if (tcnt_ > top) {
          tcnt_ = 0;
          tifr_ |= kTOV;
        }
      } else {
        // Fast PWM: TOP=0xFF wraps naturally as u8 (0xFF->0x00),
        // so detect overflow the same way as Normal mode.
        // FastPWM_OCRA uses OCRxA as top so the > check applies.
        if (mode == WaveformMode::FastPWM) {
          if (tcnt_ == 0) { tifr_ |= kTOV; } // u8 wrapped 0xFF->0x00
        } else {
          if (tcnt_ > top) { tcnt_ = 0; tifr_ |= kTOV; } // FastPWM_OCRA
        }
      }
      break;
    }

    case WaveformMode::PhaseCorrectPWM:
    case WaveformMode::PhaseCorrectPWM_OCRA: {
      if (counting_up_) {
        tcnt_++;
        if (tcnt_ == ocra_) tifr_ |= kOCFA;
        if (tcnt_ == ocrb_) tifr_ |= kOCFB;

        if (tcnt_ >= top) {
          counting_up_ = false;
          // TOV is set at BOTTOM for phase-correct PWM
        }
      } else {
        tcnt_--;
        if (tcnt_ == ocra_) tifr_ |= kOCFA;
        if (tcnt_ == ocrb_) tifr_ |= kOCFB;

        if (tcnt_ == 0) {
          counting_up_ = true;
          tifr_ |= kTOV;
        }
      }
      break;
    }

    default:
      break;
    }

    // Check and raise interrupts after every individual timer tick so that
    // multiple overflows in one batch each generate their own IRQ.
    check_interrupts();
  }
}

void Timer8::check_interrupts() {
  if (!irq_) return;

  // Overflow interrupt
  // Do NOT clear tifr_ here.  TOV0 must remain set until the CPU vectors to
  // the ISR so the millis() race-window compensation (SBIS TIFR0,TOV0) works.
  // The flag is cleared via the registered flag_clear callback in
  // InterruptController::clear() at dispatch time.
  if ((timsk_ & kTOIE) && (tifr_ & kTOV)) {
    irq_->raise(vectors_.ovf);
  }

  // Compare A interrupt
  if ((timsk_ & kOCIEA) && (tifr_ & kOCFA)) {
    irq_->raise(vectors_.compa);
  }

  // Compare B interrupt
  if ((timsk_ & kOCIEB) && (tifr_ & kOCFB)) {
    irq_->raise(vectors_.compb);
  }
}

} // namespace avrion
