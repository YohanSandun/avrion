#include <iostream>
#include <cstdio>
#include <exception>
#include <csignal>
#include <atomic>
#include <chrono>
#include "device/atmega328p.h"
#include "periph/gpio.h"
#include "intel_hex_decoder.h"

#ifndef BLINK_HEX_PATH
#  define BLINK_HEX_PATH "tests/data/pattern.hex"
#endif

static std::atomic<bool> g_stop{false};

static void handle_sigint(int) {
    g_stop.store(true);
}

int main() {
    try {
        avrion::ATmega328P dev;

        const std::vector<uint8_t> flash_data = IntelHexDecoder::decodeFile(BLINK_HEX_PATH);
        dev.load_flash(0, flash_data.data(), flash_data.size());
        dev.reset();

        // Print a line to stdout whenever any GPIO output pin changes state.
        // Shows emulated time and cycle delta so timing bugs are immediately visible.
        auto wire_trace = [&](const char* name) {
            auto* port = dev.get_peripheral<avrion::GpioPort>(name);
            if (!port) return;
            port->set_on_change([name, &dev,
                                  last_cycles  = uint64_t{0},
                                  last_wall    = std::chrono::steady_clock::now()](uint8_t old_p, uint8_t new_p) mutable {
                const uint8_t changed = old_p ^ new_p;
                for (int i = 7; i >= 0; --i) {
                    if (changed & (1u << i)) {
                        const bool high = (new_p >> i) & 1;
                        uint64_t cyc      = dev.total_cycles();
                        uint64_t delta    = cyc - last_cycles;
                        double   emu_ms   = static_cast<double>(cyc) * 1000.0
                                           / dev.config().clock_hz;
                        auto     now_wall  = std::chrono::steady_clock::now();
                        double   host_ms   = std::chrono::duration<double, std::milli>(
                                               now_wall - last_wall).count();
                        std::printf("[%9.3fms emu | +%7.3fms host | delta %llu cy] %s%d: %s\n",
                                    emu_ms, host_ms, (unsigned long long)delta,
                                    name, i, high ? "HIGH" : "LOW");
                        last_cycles = cyc;
                        last_wall   = now_wall;
                    }
                }
                std::fflush(stdout);
            });
        };
        wire_trace("PORTB");
        wire_trace("PORTC");
        wire_trace("PORTD");

        std::signal(SIGINT, handle_sigint);
        std::cout << "Running at " << dev.config().clock_hz / 1'000'000 << " MHz... Press Ctrl+C to stop." << std::endl;

        dev.run_realtime(g_stop);

        std::cout << "\nStopped." << std::endl;
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "avr_cli error: " << ex.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "avr_cli error: unknown fatal error" << std::endl;
        return 1;
    }
}