#include "device/atmega328p.h"
#include "periph/gpio.h"
#include "periph/usart.h"
#include "intel_hex_decoder.h"

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include <queue>
#include <string>

using namespace avrion;
using namespace emscripten;

class Simulator {
public:
    Simulator() {
        dev_ = std::make_unique<ATmega328P>();
        attach_usart_callback();
    }

    void load_hex(const std::string& hex_content) {
        auto flash = IntelHexDecoder::decodeString(hex_content);
        dev_->load_flash(0, flash.data(), flash.size());
    }

    void reset() {
        // Flush pending serial state before resetting the CPU.
        while (!rx_queue_.empty()) rx_queue_.pop();
        tx_buf_.clear();
        dev_->reset();
        // Peripheral objects survive reset; re-attach the callback so the
        // Usart's internal tx_callback_ still points at our (now cleared) tx_buf_.
        attach_usart_callback();
    }

    void run_cycles(int n) {
        if (n <= 0) return;

        // Inject one pending RX byte whenever the USART buffer is free
        // (RXC0 clear means firmware has consumed the previous byte).
        auto* usart = dev_->get_peripheral<Usart>("USART0");
        if (usart && !rx_queue_.empty()) {
            if (!(dev_->read_data(0x00C0) & kRXC0)) {
                usart->push_rx(rx_queue_.front());
                rx_queue_.pop();
            }
        }

        dev_->run_cycles(static_cast<uint64_t>(n));
    }

    // Returns all bytes transmitted since the last call (drain the buffer).
    std::string poll_serial_output() {
        std::string out;
        out.swap(tx_buf_);
        return out;
    }

    // Queue bytes to be injected into the USART0 RX one-by-one as the
    // firmware reads them (single-byte USART buffer, rate-limited by RXC0).
    void send_serial_input(const std::string& text) {
        for (unsigned char c : text)
            rx_queue_.push(c);
    }

    // Returns a JS object: { port: string, pins: [{index, is_output, level, pullup}] }
    val get_port_state(const std::string& port_name) {
        val result = val::object();
        result.set("port", port_name);

        val pins_arr = val::array();

        GpioPort* port = dev_->get_peripheral<GpioPort>(port_name);
        if (port) {
            auto snap = port->snapshot();
            for (int i = 0; i < 8; ++i) {
                val pin = val::object();
                pin.set("index", i);
                pin.set("is_output", snap[i].is_output);
                pin.set("level", snap[i].level);
                pin.set("pullup", snap[i].pullup);
                pins_arr.call<void>("push", pin);
            }
        }

        result.set("pins", pins_arr);
        return result;
    }

    void set_pin_input(const std::string& port_name, int pin, bool high) {
        GpioPort* port = dev_->get_peripheral<GpioPort>(port_name);
        if (port && pin >= 0 && pin < 8)
            port->set_input_level(pin, high);
    }

    double total_cycles() const {
        return static_cast<double>(dev_->total_cycles());
    }

    int get_pc() const {
        // PC is exposed via read_data at the CPU level; read SRAM isn't viable
        // for PC. We expose it by reading the raw CPU registers via data space.
        // SP low byte is at cfg.spl_data_addr; PC is internal to AvrCpu.
        // For now return the raw SP as a proxy for debug inspection.
        const auto& cfg = dev_->config();
        uint16_t spl = dev_->read_data(cfg.spl_data_addr);
        uint16_t sph = dev_->read_data(cfg.sph_data_addr);
        return static_cast<int>((sph << 8) | spl);
    }

    // Read a byte from data space (registers, IO, SRAM)
    int read_data(int addr) {
        if (addr < 0 || addr > 0xFFFF) return -1;
        return static_cast<int>(dev_->read_data(static_cast<uint16_t>(addr)));
    }

private:
    std::unique_ptr<ATmega328P> dev_;
    std::string          tx_buf_;   // accumulates bytes from USART0 TX callback
    std::queue<uint8_t>  rx_queue_; // bytes waiting to be injected into USART0 RX

    void attach_usart_callback() {
        auto* usart = dev_->get_peripheral<Usart>("USART0");
        if (usart) {
            usart->set_tx_callback([this](uint8_t byte) {
                tx_buf_ += static_cast<char>(byte);
            });
        }
    }
};

EMSCRIPTEN_BINDINGS(avrion) {
    class_<Simulator>("Simulator")
        .constructor<>()
        .function("load_hex",      &Simulator::load_hex)
        .function("reset",         &Simulator::reset)
        .function("run_cycles",    &Simulator::run_cycles)
        .function("get_port_state",&Simulator::get_port_state)
        .function("set_pin_input", &Simulator::set_pin_input)
        .function("total_cycles",  &Simulator::total_cycles)
        .function("get_pc",              &Simulator::get_pc)
        .function("read_data",           &Simulator::read_data)
        .function("poll_serial_output",  &Simulator::poll_serial_output)
        .function("send_serial_input",   &Simulator::send_serial_input);
}
