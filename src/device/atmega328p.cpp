#include "device/atmega328p.h"
#include "device/device_config.h"
#include "periph/gpio.h"
#include "periph/timer8.h"
#include "periph/usart.h"

namespace avrion
{    ATmega328P::ATmega328P()
        : Device(make_atmega328p_config())
    {
        add_peripheral("PORTB", std::make_unique<GpioPort>());
        add_peripheral("PORTC", std::make_unique<GpioPort>());
        add_peripheral("PORTD", std::make_unique<GpioPort>());

        // Timer0: 8-bit timer with OVF(vec 16), COMPA(vec 14), COMPB(vec 15)
        add_peripheral("TIMER0", std::make_unique<Timer8>(
            Timer8Vectors{ .ovf = 16, .compa = 14, .compb = 15 }));

        // USART0: RXC(vec 18), UDRE(vec 19), TXC(vec 20)
        add_peripheral("USART0", std::make_unique<Usart>(
            UsartVectors{ .rxc = 18, .udre = 19, .txc = 20 }));

        wire_from_config();
    }

} // namespace avrion