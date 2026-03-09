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

  // Misc
  void exec_nop(u16 opcode);

  // ALU
  void exec_eor(u16 opcode);
  void exec_cpi(u16 opcode);
  void exec_cpc(u16 opcode);

  // Data transfer
  void exec_out(u16 opcode);
  void exec_ldi(u16 opcode);
  void exec_st_x(u16 opcode);
  void exec_st_x_post_inc(u16 opcode);
  void exec_st_x_pre_dec(u16 opcode);

  // Branching
  void exec_jmp(u16 opcode);
  void exec_rjmp(u16 opcode);
  void exec_brne(u16 opcode);
  void exec_call(u16 opcode);

private:
  MemoryMap& mem_;
  const DeviceConfig& cfg_;
  CpuState st_;

  u32 dispatch_and_exec(u16 opcode);
};

} // namespace avrion