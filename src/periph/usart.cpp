#include "periph/usart.h"

namespace avrion {

Usart::Usart(UsartVectors vectors)
    : vectors_(vectors) {}

void Usart::on_irq_controller_set() {
  if (!irq_) return;

  // Only TXC has a flag-clear callback: the hardware clears TXC0 automatically
  // when the CPU vectors to the TXC ISR.
  // RXC and UDRE are poll-driven — they re-fire each check_interrupts() call
  // as long as the flag and its enable bit are both set, which is the correct
  // AVR behaviour (firmware must read/write UDR0 to de-assert them).
  if (vectors_.txc)
    irq_->register_flag_clear(vectors_.txc, [this] { ucsra_ &= static_cast<u8>(~kTXC0); });
}

u8 Usart::read(u16 offset) {
  switch (offset) {
  case UCSR0A: return ucsra_;
  case UCSR0B: return ucsrb_;
  case UCSR0C: return ucsrc_;
  case UBRR0L: return ubrr0l_;
  case UBRR0H: return ubrr0h_ & 0x0F; // only 4 bits are implemented
  case UDR0: {
    u8 data = rx_data_;
    // Reading UDR0 consumes the byte and clears RX status flags
    ucsra_ &= static_cast<u8>(~(kRXC0 | kFE0 | kDOR0 | kUPE0));
    return data;
  }
  default: return 0xFF;
  }
}

void Usart::write(u16 offset, u8 value) {
  switch (offset) {
  case UCSR0A:
    // Writing 1 to TXC0 clears it; U2X0 and MPCM0 are user-writable.
    // Hardware flags (RXC0, UDRE0, FE0, DOR0, UPE0) are read-only.
    if (value & kTXC0)
      ucsra_ &= static_cast<u8>(~kTXC0);
    ucsra_ = static_cast<u8>((ucsra_ & ~(kU2X0 | kMPCM0)) | (value & (kU2X0 | kMPCM0)));
    break;

  case UCSR0B:
    ucsrb_ = value;
    check_interrupts();
    break;

  case UCSR0C:
    ucsrc_ = value;
    break;

  case UBRR0L:
    ubrr0l_ = value;
    break;

  case UBRR0H:
    ubrr0h_ = value & 0x0F;
    break;

  case UDR0:
    if (!(ucsrb_ & kTXEN0))
      break; // Transmitter not enabled; ignore write
    tx_data_ = value;
    ucsra_ &= static_cast<u8>(~kUDRE0); // Buffer no longer empty
    tx_cycles_remaining_ = frame_cycles();
    check_interrupts();
    break;

  default:
    break;
  }
}

void Usart::tick(u32 cycles) {
  if (tx_cycles_remaining_ == 0) return;

  if (cycles >= tx_cycles_remaining_) {
    tx_cycles_remaining_ = 0;
    // Frame complete: fire callback and set UDRE0 + TXC0
    if (tx_callback_)
      tx_callback_(tx_data_);
    ucsra_ |= kUDRE0 | kTXC0;
    check_interrupts();
  } else {
    tx_cycles_remaining_ -= cycles;
  }
}

void Usart::push_rx(u8 data, bool fe, bool upe) {
  if (ucsra_ & kRXC0) {
    // Previous byte has not been read — data overrun
    ucsra_ |= kDOR0;
    return;
  }
  rx_data_ = data;
  ucsra_ |= kRXC0;
  if (fe)  ucsra_ |= kFE0;
  if (upe) ucsra_ |= kUPE0;
  check_interrupts();
}

// ---- Private helpers -------------------------------------------------------

u32 Usart::cycles_per_bit() const {
  u16 ubrr = static_cast<u16>((ubrr0h_ & 0x0F) << 8) | ubrr0l_;
  u32 divisor = (ucsra_ & kU2X0) ? 8u : 16u;
  return divisor * (static_cast<u32>(ubrr) + 1u);
}

u32 Usart::frame_bit_count() const {
  // Start bit (always 1)
  u32 bits = 1;

  // Data bits: UCSZ02:UCSZ01:UCSZ00 (3-bit field)
  u8 ucsz = static_cast<u8>(((ucsrb_ & kUCSZ02) ? 4u : 0u)
                           | ((ucsrc_ & kUCSZ01) ? 2u : 0u)
                           | ((ucsrc_ & kUCSZ00) ? 1u : 0u));
  switch (ucsz) {
  case 0: bits += 5; break;
  case 1: bits += 6; break;
  case 2: bits += 7; break;
  case 3: bits += 8; break; // default 8-bit
  case 7: bits += 9; break; // 9-bit mode
  default: bits += 8; break;
  }

  // Parity bits (UPM01:UPM00 — 0x00 = no parity, 0x10/0x11 = even/odd = 1 bit)
  if (ucsrc_ & kUPM01)
    bits += 1;

  // Stop bits (USBS0: 0=1 stop, 1=2 stop)
  bits += (ucsrc_ & kUSBS0) ? 2u : 1u;

  return bits;
}

u32 Usart::frame_cycles() const {
  return frame_bit_count() * cycles_per_bit();
}

void Usart::check_interrupts() {
  if (!irq_) return;

  if ((ucsrb_ & kRXCIE0) && (ucsrb_ & kRXEN0) && (ucsra_ & kRXC0))
    irq_->raise(vectors_.rxc);

  if ((ucsrb_ & kUDRIE0) && (ucsrb_ & kTXEN0) && (ucsra_ & kUDRE0))
    irq_->raise(vectors_.udre);

  if ((ucsrb_ & kTXCIE0) && (ucsrb_ & kTXEN0) && (ucsra_ & kTXC0))
    irq_->raise(vectors_.txc);
}

} // namespace avrion
