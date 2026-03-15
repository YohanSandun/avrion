#include "device/atmega328p.h"
#include "periph/gpio.h"
#include "intel_hex_decoder.h"

#include <emscripten/bind.h>
#include <emscripten/val.h>

using namespace avrion;
using namespace emscripten;

class Simulator {
public:
    Simulator() {
        dev_ = std::make_unique<ATmega328P>();
    }

    void load_hex(const std::string& hex_content) {
        auto flash = IntelHexDecoder::decodeString(hex_content);
        dev_->load_flash(0, flash.data(), flash.size());
    }

    void reset() {
        dev_->reset();
    }

    void run_cycles(int n) {
        if (n > 0)
            dev_->run_cycles(static_cast<uint64_t>(n));
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
        .function("get_pc",        &Simulator::get_pc)
        .function("read_data",     &Simulator::read_data);
}
