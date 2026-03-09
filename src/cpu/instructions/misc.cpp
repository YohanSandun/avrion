#include "cpu/avr_cpu.h"

namespace avrion {

void AvrCpu::exec_nop(u16) {
    // nothing
}

void AvrCpu::exec_sei(u16) {
    set_sreg(sreg() | 0x80); // set I bit (bit 7)
}

} // namespace avrion