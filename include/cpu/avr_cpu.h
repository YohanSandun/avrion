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

  u8  reg(u8 i) const { return st_.r[i & 31]; }
  void set_reg(u8 i, u8 v) { st_.r[i & 31] = v; }

  u8  sreg() const { return st_.sreg; }
  void set_sreg(u8 v) { st_.sreg = v; }

  u16 pc() const { return st_.pc; }
  void set_pc(u16 v) { st_.pc = v; }

  u16 sp() const { return st_.sp; }
  void set_sp(u16 v) { st_.sp = v; }

private:
  MemoryMap& mem_;
  const DeviceConfig& cfg_;
  CpuState st_;

  u32 dispatch_and_exec(u16 opcode);

  // exec functions... (table driven)
  // static void exec_ldi(void* cpu, u16 opcode);
  // static void exec_add(void* cpu, u16 opcode);
};

} // namespace avrion