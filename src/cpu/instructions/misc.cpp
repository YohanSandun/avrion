#include "cpu/avr_cpu.h"

namespace avrion {

u8 AvrCpu::exec_nop(u16) {
    return 1;
}

u8 AvrCpu::exec_sei(u16) {
    set_sreg(sreg() | 0x80); // set I bit (bit 7)
    return 1;
}

u8 AvrCpu::exec_cli(u16) {
    set_sreg(sreg() & ~0x80); // clear I bit (bit 7)
    return 1;
}

} // namespace avrion