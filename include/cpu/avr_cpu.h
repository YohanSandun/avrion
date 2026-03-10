#pragma once
#include "core/types.h"
#include "cpu/cpu_state.h"

namespace avrion {

class MemoryMap;
struct DeviceConfig;

class AvrCpu {
public:
  AvrCpu(MemoryMap& mem, const DeviceConfig& cfg);

  void reset();
  u32 step_instruction();

  // CpuSnapshot snapshot() const;

  u8  reg(u8 i) const { return st_.r[i]; }
  void set_reg(u8 i, u8 v) { st_.r[i] = v; }

  u8  sreg() const { return st_.sreg; }
  void set_sreg(u8 v) { st_.sreg = v; }

  u32 pc() const { return st_.pc; }
  void set_pc(u32 v) { st_.pc = v; }

  u16 sp() const { return st_.sp; }
  void set_sp(u16 v) { st_.sp = v; }

  u16 x() const;
  void set_x(u16 v);

  u16 z() const;
  void set_z(u16 v);

  // Misc
  u8 exec_nop(u16 opcode);
  u8 exec_sei(u16 opcode);
  u8 exec_cli(u16 opcode);

  // ALU
  u8 exec_eor(u16 opcode);
  u8 exec_cpi(u16 opcode);
  u8 exec_cpc(u16 opcode);
  u8 exec_ori(u16 opcode);
  u8 exec_and(u16 opcode);
  u8 exec_add(u16 opcode);
  u8 exec_adc(u16 opcode);
  u8 exec_subi(u16 opcode);
  u8 exec_sbci(u16 opcode);

  // Data transfer
  u8 exec_out(u16 opcode);
  u8 exec_in(u16 opcode);
  u8 exec_ldi(u16 opcode);
  u8 exec_st_x(u16 opcode);
  u8 exec_st_x_post_inc(u16 opcode);
  u8 exec_st_x_pre_dec(u16 opcode);
  u8 exec_lds(u16 opcode);
  u8 exec_sts(u16 opcode);
  u8 exec_lpm(u16 opcode);
  u8 exec_lpm_z(u16 opcode);
  u8 exec_lpm_z_post_inc(u16 opcode);
  u8 exec_movw(u16 opcode);

  // Branching
  u8 exec_jmp(u16 opcode);
  u8 exec_rjmp(u16 opcode);
  u8 exec_brne(u16 opcode);
  u8 exec_call(u16 opcode);
  u8 exec_breq(u16 opcode);

private:
  MemoryMap& mem_;
  const DeviceConfig& cfg_;
  CpuState st_;

  u32 dispatch_and_exec(u16 opcode);
};

} // namespace avrion