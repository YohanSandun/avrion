#include "periph/gpio.h"

namespace avrion {

u8 GpioPort::read(u16 offset)
{
    switch (offset) {
    case PIN:  return pin_latched_;
    case DDR:  return ddr_;
    case PORT: return port_;
    default:   return 0xFF;
    }
}

void GpioPort::write(u16 offset, u8 value)
{
    switch (offset) {
    case DDR:
        ddr_ = value;
        recompute_pins();
        break;
    case PORT:
        port_ = value;
        recompute_pins();
        break;
    case PIN:
        // Writing 1 to PIN toggles the corresponding PORT bit (AVR feature)
        port_ ^= value;
        recompute_pins();
        break;
    default:
        break;
    }
}

void GpioPort::set_input_level(int bit, bool high)
{
    if (bit < 0 || bit > 7) return;
    if (high)
        ext_in_ |= static_cast<u8>(1u << bit);
    else
        ext_in_ &= static_cast<u8>(~(1u << bit));
    recompute_pins();
}

std::array<PinState, 8> GpioPort::snapshot() const
{
    std::array<PinState, 8> result{};
    for (int i = 0; i < 8; ++i) {
        const u8 mask = static_cast<u8>(1u << i);
        result[i].is_output = (ddr_ & mask) != 0;
        result[i].level     = (pin_latched_ & mask) != 0;
        result[i].pullup    = !result[i].is_output && (port_ & mask) != 0;
    }
    return result;
}

void GpioPort::recompute_pins()
{
    // Output pins: driven by PORT register
    // Input pins: driven by external signal (ext_in_), with optional pull-up
    const u8 output_levels = ddr_ & port_;
    const u8 input_levels  = static_cast<u8>(~ddr_) & ext_in_;
    pin_latched_ = output_levels | input_levels;
}

} // namespace avrion
