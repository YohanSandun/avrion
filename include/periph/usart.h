#pragma once
#include "periph/peripheral.h"
#include "core/interrupt_controller.h"
#include <functional>

namespace avrion {

// Interrupt vector numbers for the USART peripheral.
// Set to 0 to disable a particular interrupt source.
struct UsartVectors {
  u8 rxc  = 0;  // RX Complete
  u8 udre = 0;  // Data Register Empty
  u8 txc  = 0;  // TX Complete
};

// Register indices (offsets) within the USART peripheral.
// The Device maps two non-contiguous IO regions to these indices via
// periph_offset, so the USART always receives a consistent index (0-5).
enum UsartReg : u16 {
  UCSR0A = 0,
  UCSR0B = 1,
  UCSR0C = 2,
  UBRR0L = 3,
  UBRR0H = 4,
  UDR0   = 5,
};

// UCSR0A bits
static constexpr u8 kRXC0  = 0x80; // RX Complete (read-only, hardware-set)
static constexpr u8 kTXC0  = 0x40; // TX Complete (write 1 to clear; auto-cleared on ISR)
static constexpr u8 kUDRE0 = 0x20; // Data Register Empty (read-only, hardware-set)
static constexpr u8 kFE0   = 0x10; // Frame Error (read-only)
static constexpr u8 kDOR0  = 0x08; // Data Overrun (read-only)
static constexpr u8 kUPE0  = 0x04; // Parity Error (read-only)
static constexpr u8 kU2X0  = 0x02; // Double USART Transmission Speed
static constexpr u8 kMPCM0 = 0x01; // Multi-processor Communication Mode

// UCSR0B bits
static constexpr u8 kRXCIE0 = 0x80; // RX Complete Interrupt Enable
static constexpr u8 kTXCIE0 = 0x40; // TX Complete Interrupt Enable
static constexpr u8 kUDRIE0 = 0x20; // Data Register Empty Interrupt Enable
static constexpr u8 kRXEN0  = 0x10; // Receiver Enable
static constexpr u8 kTXEN0  = 0x08; // Transmitter Enable
static constexpr u8 kUCSZ02 = 0x04; // Character Size bit 2

// UCSR0C bits
static constexpr u8 kUMSEL01 = 0x80; // USART Mode Select bit 1
static constexpr u8 kUMSEL00 = 0x40; // USART Mode Select bit 0
static constexpr u8 kUPM01   = 0x20; // Parity Mode bit 1
static constexpr u8 kUPM00   = 0x10; // Parity Mode bit 0
static constexpr u8 kUSBS0   = 0x08; // Stop Bit Select (0=1 stop, 1=2 stop)
static constexpr u8 kUCSZ01  = 0x04; // Character Size bit 1
static constexpr u8 kUCSZ00  = 0x02; // Character Size bit 0
static constexpr u8 kUCPOL0  = 0x01; // Clock Polarity (sync mode only)

// Usart models the ATmega USART peripheral in asynchronous mode.
//
// TX is cycle-accurate: writing UDR0 starts a countdown of frame_cycles()
// simulation cycles; when elapsed, the tx_callback fires and UDRE/TXC flags
// are set.
//
// RX uses a single-byte buffer. Call push_rx() to inject a received byte.
// If push_rx() is called while RXC0 is still set (firmware hasn't read UDR0),
// DOR0 is set and the new byte is discarded (data overrun).
//
// Interrupt behaviour:
//   RXC  — re-raised each check while kRXC0 and kRXCIE0 are both set
//           (poll-driven; clears when firmware reads UDR0).
//   UDRE — re-raised each check while kUDRE0 and kUDRIE0 are both set
//           (poll-driven; clears when firmware writes UDR0).
//   TXC  — raised once; flag auto-cleared by registered flag_clear callback
//           when the CPU vectors to the TXC ISR.
class Usart : public Peripheral {
public:
  explicit Usart(UsartVectors vectors);

  u8   read(u16 offset) override;
  void write(u16 offset, u8 value) override;
  void tick(u32 cycles) override;
  void on_irq_controller_set() override;

  // Inject a byte into the RX buffer (e.g. from a simulated host).
  // fe  — frame error flag
  // upe — parity error flag
  void push_rx(u8 data, bool fe = false, bool upe = false);

  // Register a callback invoked each time a byte is fully transmitted.
  void set_tx_callback(std::function<void(u8)> cb) { tx_callback_ = std::move(cb); }

private:
  UsartVectors vectors_;

  // Registers (UCSR0A init: kUDRE0; UCSR0C init: 8-bit async = 0x06)
  u8 ucsra_ = kUDRE0;
  u8 ucsrb_ = 0;
  u8 ucsrc_ = kUCSZ01 | kUCSZ00; // 8-bit, 1 stop, no parity
  u8 ubrr0l_ = 0;
  u8 ubrr0h_ = 0;

  u8 tx_data_ = 0;
  u8 rx_data_ = 0;

  // Remaining simulation cycles until current TX frame completes.
  // 0 means no active transmission.
  u32 tx_cycles_remaining_ = 0;

  std::function<void(u8)> tx_callback_;

  // Baud-rate helpers
  u32 cycles_per_bit() const;
  u32 frame_bit_count() const; // start + data + parity + stop
  u32 frame_cycles() const;

  void check_interrupts();
};

} // namespace avrion
