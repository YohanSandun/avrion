#include <catch2/catch_test_macros.hpp>

#include "periph/usart.h"
#include "core/interrupt_controller.h"
#include "device/atmega328p.h"

using namespace avrion;

// ---------------------------------------------------------------------------
// Helper: a USART with ATmega328P USART0 vectors and an attached IRQ controller
// ---------------------------------------------------------------------------
static Usart make_usart0(InterruptController& irq)
{
    Usart u(UsartVectors{ .rxc = 18, .udre = 19, .txc = 20 });
    u.set_irq_controller(&irq);
    return u;
}

// ---------------------------------------------------------------------------
// 1. Register read/write round-trip
// ---------------------------------------------------------------------------
TEST_CASE("Usart - register read/write round-trip", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    // UCSR0C: read back written value
    u.write(UCSR0C, 0x8E);
    REQUIRE(u.read(UCSR0C) == 0x8E);

    // UBRR0L
    u.write(UBRR0L, 0xCF);
    REQUIRE(u.read(UBRR0L) == 0xCF);

    // UBRR0H — only 4 bits are implemented
    u.write(UBRR0H, 0xFF);
    REQUIRE(u.read(UBRR0H) == 0x0F);

    // UCSR0B: written and read back
    u.write(UCSR0B, 0x58); // RXEN0 | TXEN0 | RXCIE0
    REQUIRE(u.read(UCSR0B) == 0x58);
}

// ---------------------------------------------------------------------------
// 2. Initial state: kUDRE0 set; TXEN/RXEN disabled
// ---------------------------------------------------------------------------
TEST_CASE("Usart - initial state", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    REQUIRE(u.read(UCSR0A) & kUDRE0); // data register empty on reset
    REQUIRE_FALSE(u.read(UCSR0A) & kRXC0);
    REQUIRE_FALSE(u.read(UCSR0A) & kTXC0);
    REQUIRE_FALSE(u.read(UCSR0B) & kTXEN0);
    REQUIRE_FALSE(u.read(UCSR0B) & kRXEN0);
    // Default character size is 8-bit (UCSZ01|UCSZ00)
    REQUIRE(u.read(UCSR0C) == (kUCSZ01 | kUCSZ00));
}

// ---------------------------------------------------------------------------
// 3. UDR0 write with TXEN=0: no countdown started, UDRE stays set
// ---------------------------------------------------------------------------
TEST_CASE("Usart - write UDR0 without TXEN has no effect", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    u.write(UDR0, 0x41);
    REQUIRE(u.read(UCSR0A) & kUDRE0); // still empty — nothing was loaded

    u.tick(10000);
    REQUIRE_FALSE(u.read(UCSR0A) & kTXC0); // no TX completed
}

// ---------------------------------------------------------------------------
// Helper: compute expected frame cycles at 9600 baud, 16 MHz, 8N1
// UBRR = 103, divisor = 16, cpb = 16*(103+1) = 1664
// frame bits = 1+8+1 = 10, frame_cycles = 16640
// ---------------------------------------------------------------------------
static constexpr u16 kUBRR_9600 = 103;
static constexpr u32 kCPB_9600  = 16u * (kUBRR_9600 + 1u); // cycles per bit
static constexpr u32 kFrame_9600 = 10u * kCPB_9600;         // 8N1 = 10 bits

// ---------------------------------------------------------------------------
// 4. UDR0 write with TXEN=1: kUDRE0 clears, countdown starts
// ---------------------------------------------------------------------------
TEST_CASE("Usart - UDR0 write with TXEN clears UDRE and starts countdown", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    // 9600 baud @ 16 MHz
    u.write(UBRR0L, kUBRR_9600 & 0xFF);
    u.write(UBRR0H, (kUBRR_9600 >> 8) & 0x0F);
    u.write(UCSR0B, kTXEN0);

    u.write(UDR0, 0x41);
    REQUIRE_FALSE(u.read(UCSR0A) & kUDRE0); // buffer now occupied

    // Not complete yet after one cycle
    u.tick(1);
    REQUIRE_FALSE(u.read(UCSR0A) & kTXC0);
    REQUIRE_FALSE(u.read(UCSR0A) & kUDRE0);
}

// ---------------------------------------------------------------------------
// 5. TX completes after exactly frame_cycles(); callback fires with correct byte
// ---------------------------------------------------------------------------
TEST_CASE("Usart - TX completes after frame_cycles", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    u.write(UBRR0L, kUBRR_9600 & 0xFF);
    u.write(UBRR0H, (kUBRR_9600 >> 8) & 0x0F);
    u.write(UCSR0B, kTXEN0);

    u8 received = 0;
    u.set_tx_callback([&](u8 byte) { received = byte; });

    u.write(UDR0, 0x41);

    // Tick up to one cycle short — should not be done
    u.tick(kFrame_9600 - 1);
    REQUIRE_FALSE(u.read(UCSR0A) & kTXC0);
    REQUIRE(received == 0); // callback not fired yet

    // Final cycle completes the frame
    u.tick(1);
    REQUIRE(u.read(UCSR0A) & kTXC0);
    REQUIRE(u.read(UCSR0A) & kUDRE0);
    REQUIRE(received == 0x41);
}

// ---------------------------------------------------------------------------
// 6. TX timing with U2X=1 (double speed)
// ---------------------------------------------------------------------------
TEST_CASE("Usart - TX timing with U2X double speed", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    // 9600 baud @ 16 MHz, U2X: UBRR = 207, divisor = 8
    constexpr u16 ubrr_u2x = 207;
    constexpr u32 cpb_u2x  = 8u * (ubrr_u2x + 1u);
    constexpr u32 frame_u2x = 10u * cpb_u2x;

    u.write(UCSR0A, kU2X0);
    u.write(UBRR0L, ubrr_u2x & 0xFF);
    u.write(UBRR0H, (ubrr_u2x >> 8) & 0x0F);
    u.write(UCSR0B, kTXEN0);

    bool fired = false;
    u.set_tx_callback([&](u8) { fired = true; });

    u.write(UDR0, 0xAB);

    u.tick(frame_u2x - 1);
    REQUIRE_FALSE(fired);

    u.tick(1);
    REQUIRE(fired);
    REQUIRE(u.read(UCSR0A) & kTXC0);
}

// ---------------------------------------------------------------------------
// 7. RX: push_rx sets RXC0; reading UDR0 returns data and clears flags
// ---------------------------------------------------------------------------
TEST_CASE("Usart - push_rx sets RXC0; UDR0 read clears it", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    u.write(UCSR0B, kRXEN0);
    u.push_rx(0x5A);

    REQUIRE(u.read(UCSR0A) & kRXC0);
    REQUIRE(u.read(UDR0) == 0x5A);
    REQUIRE_FALSE(u.read(UCSR0A) & kRXC0);
    REQUIRE_FALSE(u.read(UCSR0A) & kFE0);
    REQUIRE_FALSE(u.read(UCSR0A) & kDOR0);
    REQUIRE_FALSE(u.read(UCSR0A) & kUPE0);
}

// ---------------------------------------------------------------------------
// 8. RX overrun: second push_rx while RXC0 is set causes DOR0; original preserved
// ---------------------------------------------------------------------------
TEST_CASE("Usart - RX overrun sets DOR0 and preserves original byte", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    u.write(UCSR0B, kRXEN0);
    u.push_rx(0x11); // first byte (not yet read)
    u.push_rx(0x22); // overrun — should be discarded

    REQUIRE(u.read(UCSR0A) & kDOR0);
    REQUIRE(u.read(UDR0) == 0x11); // original byte preserved
}

// ---------------------------------------------------------------------------
// 9. push_rx with fe/upe flags visible in UCSR0A; cleared on UDR0 read
// ---------------------------------------------------------------------------
TEST_CASE("Usart - push_rx propagates FE0 and UPE0 flags", "[usart]")
{
    {
        InterruptController irq;
        auto u = make_usart0(irq);
        u.push_rx(0xFF, /*fe=*/true, /*upe=*/false);
        REQUIRE(u.read(UCSR0A) & kFE0);
        REQUIRE_FALSE(u.read(UCSR0A) & kUPE0);
        u.read(UDR0); // consume — flags clear
        REQUIRE_FALSE(u.read(UCSR0A) & kFE0);
    }
    {
        InterruptController irq;
        auto u = make_usart0(irq);
        u.push_rx(0xFF, /*fe=*/false, /*upe=*/true);
        REQUIRE(u.read(UCSR0A) & kUPE0);
        u.read(UDR0);
        REQUIRE_FALSE(u.read(UCSR0A) & kUPE0);
    }
}

// ---------------------------------------------------------------------------
// 10. RXCIE0 interrupt fires on push_rx; re-fires while RXC0 is set
// ---------------------------------------------------------------------------
TEST_CASE("Usart - RXCIE0 raises RXC interrupt; clears when UDR0 read", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);
    u.write(UCSR0B, kRXEN0 | kRXCIE0);

    REQUIRE_FALSE(irq.any_pending());

    u.push_rx(0xAB);
    REQUIRE(irq.any_pending());
    REQUIRE(irq.highest_pending() == 18);

    // Simulate ISR entry — flag-clear callback NOT registered for RXC,
    // so the IRQ remains pending until firmware reads UDR0.
    irq.clear(18);
    // Re-check_interrupts is driven by tick; simulate next tick
    u.tick(1); // triggers check_interrupts when TX is idle (no-op on counters)
    // Without reading UDR0, RXC0 is still set so interrupt should re-raise
    u.push_rx(0); // won't work (overrun), use tick path — but check_interrupts only
    // called in tick if tx_cycles_remaining_ > 0. Let's trigger via write to UCSR0B.
    u.write(UCSR0B, kRXEN0 | kRXCIE0); // triggers check_interrupts
    REQUIRE(irq.any_pending()); // re-raised

    // Now read UDR0 — clears RXC0
    (void)u.read(UDR0);
    irq.clear(18);
    u.write(UCSR0B, kRXEN0 | kRXCIE0);
    REQUIRE_FALSE(irq.any_pending());
}

// ---------------------------------------------------------------------------
// 11. UDRIE0: fires immediately (UDRE0 set on reset); clears when UDR0 written
// ---------------------------------------------------------------------------
TEST_CASE("Usart - UDRIE0 fires immediately; cleared when TX starts", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    // Enabling TXEN + UDRIE0: UDRE0 is already set, so IRQ should fire
    u.write(UCSR0B, kTXEN0 | kUDRIE0);
    REQUIRE(irq.any_pending());
    REQUIRE(irq.highest_pending() == 19);

    // Writing UDR0 consumes the buffer — UDRE0 cleared, IRQ should no longer be pending
    u.write(UBRR0L, kUBRR_9600 & 0xFF);
    u.write(UDR0, 0x55);
    irq.clear(19);
    u.write(UCSR0B, kTXEN0 | kUDRIE0); // re-check
    REQUIRE_FALSE(irq.any_pending()); // UDRE0 now clear
}

// ---------------------------------------------------------------------------
// 12. TXCIE0: fires after TX; TXC0 auto-cleared on ISR entry via flag_clear
// ---------------------------------------------------------------------------
TEST_CASE("Usart - TXCIE0 fires after TX complete; TXC0 auto-cleared on ISR", "[usart]")
{
    InterruptController irq;
    auto u = make_usart0(irq);

    u.write(UBRR0L, kUBRR_9600 & 0xFF);
    u.write(UCSR0B, kTXEN0 | kTXCIE0);
    u.write(UDR0, 0x77);

    u.tick(kFrame_9600);

    REQUIRE(u.read(UCSR0A) & kTXC0);
    REQUIRE(irq.any_pending());
    REQUIRE(irq.highest_pending() == 20);

    // Simulate CPU vectoring to ISR — clears TXC0 via registered callback
    irq.clear(20);
    REQUIRE_FALSE(u.read(UCSR0A) & kTXC0);
    REQUIRE_FALSE(irq.any_pending());
}

// ---------------------------------------------------------------------------
// 13. ATmega328P integration: USART0 accessible via MemoryMap at 0xC0/0xC4/0xC6
// ---------------------------------------------------------------------------
TEST_CASE("Usart - ATmega328P integration via MemoryMap", "[usart][integration]")
{
    ATmega328P mcu;

    // get_peripheral should return a valid Usart pointer
    auto* u = mcu.get_peripheral<Usart>("USART0");
    REQUIRE(u != nullptr);

    // Initial state: UCSR0A at 0xC0 should have UDRE0 set
    REQUIRE(mcu.read_data(0x00C0) & kUDRE0);

    // Write UCSR0B at 0xC1: enable TX
    u8 ucsrb_val = mcu.read_data(0x00C1);
    mcu.write_data(0x00C1, static_cast<u8>(ucsrb_val | kTXEN0));
    REQUIRE(mcu.read_data(0x00C1) & kTXEN0);

    // Write UBRR0L at 0xC4
    mcu.write_data(0x00C4, 0x67);
    REQUIRE(mcu.read_data(0x00C4) == 0x67);

    // Write UDR0 at 0xC6: should start TX (UDRE0 clears)
    mcu.write_data(0x00C6, 0x41);
    REQUIRE_FALSE(mcu.read_data(0x00C0) & kUDRE0);
}
