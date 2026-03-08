#include "cpu/decoder.h"
#include <array>
#include <stdexcept>

namespace avrion {

namespace {
    std::array<const InstructionDesc*, 65536> g_lut{};
    bool g_initialized = false;
}

void initialize_decoder_lut() {
    if (g_initialized) {
        return;
    }

    g_lut.fill(nullptr);

    const InstructionDesc* begin = instruction_table_begin();
    const InstructionDesc* end   = instruction_table_end();

    for (u32 opcode = 0; opcode <= 0xFFFF; ++opcode) {
        const InstructionDesc* match = nullptr;

        for (const InstructionDesc* it = begin; it != end; ++it) {
            if ((opcode & it->mask) == it->pattern) {
                match = it;
                break;
            }
        }

        g_lut[opcode] = match;
    }

    g_initialized = true;
}

const InstructionDesc* lookup_instruction(u16 opcode) {
    if (!g_initialized) {
        initialize_decoder_lut();
    }
    return g_lut[opcode];
}

} // namespace avrion