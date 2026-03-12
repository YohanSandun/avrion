#include <catch2/catch_test_macros.hpp>

#include "periph/timer8.h"
#include "core/interrupt_controller.h"

using namespace avrion;

// ---------------------------------------------------------------------------
// Helper: create a Timer8 with ATmega328P Timer0 interrupt vectors
// and attach a real InterruptController for testing.
// ---------------------------------------------------------------------------
static Timer8 make_timer0(InterruptController& irq)
{
    Timer8 timer(Timer8Vectors{ .ovf = 16, .compa = 14, .compb = 15 });
    timer.set_irq_controller(&irq);
    return timer;
}

// Writing / reading back all registers
TEST_CASE("Timer8 - register read/write round-trip", "[timer8]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, 0x83);
    REQUIRE(timer.read(TCCRxA) == 0x83);

    timer.write(TCCRxB, 0x05);
    REQUIRE(timer.read(TCCRxB) == 0x05);

    timer.write(TCNTx, 0xAB);
    REQUIRE(timer.read(TCNTx) == 0xAB);

    timer.write(OCRxA, 0x7F);
    REQUIRE(timer.read(OCRxA) == 0x7F);

    timer.write(OCRxB, 0x3F);
    REQUIRE(timer.read(OCRxB) == 0x3F);

    timer.write(TIMSKx, 0x07);
    REQUIRE(timer.read(TIMSKx) == 0x07);

    // TIFR: writing 1 clears flags
    timer.write(TIFRx, 0xFF);
    REQUIRE(timer.read(TIFRx) == 0x00);
}

// Clock stopped → no counting
TEST_CASE("Timer8 - clock stopped does not count", "[timer8]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxB, 0x00); // CS=0 → stopped
    timer.write(TCNTx, 0x00);
    timer.tick(1000);
    REQUIRE(timer.read(TCNTx) == 0x00);
}

// Normal mode (WGM=0), prescaler /1 — counts up, overflows at 0xFF→0x00
TEST_CASE("Timer8 - normal mode counts up", "[timer8]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, 0x00); // WGM=0 (normal)
    timer.write(TCCRxB, 0x01); // CS=1 (clk/1)
    timer.write(TCNTx, 0x00);

    timer.tick(10);
    REQUIRE(timer.read(TCNTx) == 10);
}

TEST_CASE("Timer8 - normal mode overflow at 0xFF", "[timer8]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, 0x00);
    timer.write(TCCRxB, 0x01);
    timer.write(TCNTx, 0xFE);

    timer.tick(2); // 0xFE → 0xFF → 0x00
    REQUIRE(timer.read(TCNTx) == 0x00);
}

// CTC mode (WGM=2) — clear on compare match with OCRxA
TEST_CASE("Timer8 - CTC mode clears at OCR0A", "[timer8]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, kWGM01); // WGM=2 (CTC)
    timer.write(TCCRxB, 0x01);   // CS=1
    timer.write(OCRxA, 0x04);    // TOP=4
    timer.write(TCNTx, 0x00);

    timer.tick(5); // 0→1→2→3→4→(clear to 0)
    REQUIRE(timer.read(TCNTx) == 0x00);

    timer.tick(3); // 0→1→2→3
    REQUIRE(timer.read(TCNTx) == 3);
}

// Prescaler /8 — only counts once every 8 CPU cycles
TEST_CASE("Timer8 - prescaler divides clock correctly", "[timer8]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, 0x00);
    timer.write(TCCRxB, 0x02); // CS=2 (clk/8)
    timer.write(TCNTx, 0x00);

    timer.tick(7);
    REQUIRE(timer.read(TCNTx) == 0); // not yet

    timer.tick(1); // 8 total
    REQUIRE(timer.read(TCNTx) == 1);

    timer.tick(16); // 24 total → 3 timer ticks
    REQUIRE(timer.read(TCNTx) == 3);
}

// Overflow interrupt fires when enabled
TEST_CASE("Timer8 - overflow interrupt raises IRQ", "[timer8][interrupt]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, 0x00);
    timer.write(TCCRxB, 0x01);     // CS=1
    timer.write(TIMSKx, kTOIE);    // enable overflow interrupt
    timer.write(TCNTx, 0xFE);

    REQUIRE(!irq.is_pending(16));

    timer.tick(2); // 0xFE → 0xFF → 0x00 (overflow)
    REQUIRE(irq.is_pending(16));
}

// Compare A interrupt
TEST_CASE("Timer8 - compare A interrupt raises IRQ", "[timer8][interrupt]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, 0x00);
    timer.write(TCCRxB, 0x01);     // CS=1
    timer.write(OCRxA, 0x05);
    timer.write(TIMSKx, kOCIEA);   // enable compare A interrupt
    timer.write(TCNTx, 0x00);

    REQUIRE(!irq.is_pending(14));

    timer.tick(5); // reaches 0x05
    REQUIRE(irq.is_pending(14));
}

// Compare B interrupt
TEST_CASE("Timer8 - compare B interrupt raises IRQ", "[timer8][interrupt]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, 0x00);
    timer.write(TCCRxB, 0x01);
    timer.write(OCRxB, 0x03);
    timer.write(TIMSKx, kOCIEB);
    timer.write(TCNTx, 0x00);

    REQUIRE(!irq.is_pending(15));

    timer.tick(3); // reaches 0x03
    REQUIRE(irq.is_pending(15));
}

// No interrupt when disabled in TIMSK
TEST_CASE("Timer8 - no interrupt when TIMSK disabled", "[timer8][interrupt]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, 0x00);
    timer.write(TCCRxB, 0x01);
    timer.write(TIMSKx, 0x00);     // all interrupts disabled
    timer.write(TCNTx, 0xFE);

    timer.tick(2);
    REQUIRE(!irq.is_pending(16));
}

// Phase-correct PWM mode: counts up then down, TOV at BOTTOM
TEST_CASE("Timer8 - phase correct PWM counts up then down", "[timer8]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    timer.write(TCCRxA, kWGM00);   // WGM=1 (phase correct PWM, TOP=0xFF)
    timer.write(TCCRxB, 0x01);     // CS=1
    timer.write(TCNTx, 0xFD);

    // Count up: 0xFD → 0xFE → 0xFF (TOP, reverse) → 0xFE
    timer.tick(3);
    REQUIRE(timer.read(TCNTx) == 0xFE);
}

// RETI instruction test
TEST_CASE("Timer8 - TIFR write-1-to-clear", "[timer8]")
{
    InterruptController irq;
    auto timer = make_timer0(irq);

    // Manually set overflow flag by overflow
    timer.write(TCCRxA, 0x00);
    timer.write(TCCRxB, 0x01);
    timer.write(TIMSKx, 0x00);   // disable interrupts so flags remain
    timer.write(TCNTx, 0xFF);

    timer.tick(1); // overflow → TOV set in TIFR
    REQUIRE((timer.read(TIFRx) & kTOV) != 0);

    // Write 1 to TOV to clear it
    timer.write(TIFRx, kTOV);
    REQUIRE((timer.read(TIFRx) & kTOV) == 0);
}
