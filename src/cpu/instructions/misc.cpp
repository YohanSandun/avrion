#include "cpu/avr_cpu.h"
#include "device/device_config.h"
#include "memory/memory_map.h"

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

u8 AvrCpu::exec_reti(u16) {
    u32 ret = mem_.read8(st_.sp++);
    ret |= static_cast<u32>(mem_.read8(st_.sp++)) << 8;
    if (cfg_.has_22_bit_pc) {
        ret |= static_cast<u32>(mem_.read8(st_.sp++)) << 16;
    }
    set_pc(ret);
    set_sreg(sreg() | 0x80); // set I bit (bit 7)

    return cfg_.has_22_bit_pc ? 5 : 4;
}

} // namespace avrion