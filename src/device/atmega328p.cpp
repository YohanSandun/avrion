#include "device/atmega328p.h"
#include "device/device_config.h"
#include "periph/gpio.h"

namespace avrion
{    ATmega328P::ATmega328P()
        : Device(make_atmega328p_config())
    {
        add_peripheral("PORTB", std::make_unique<GpioPort>());
        add_peripheral("PORTC", std::make_unique<GpioPort>());
        add_peripheral("PORTD", std::make_unique<GpioPort>());

        wire_from_config();
    }

} // namespace avrion