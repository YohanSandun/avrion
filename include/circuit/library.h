#pragma once
#include <string>
#include <vector>
#include <array>
#include <optional>

namespace circuit {

enum class PinSide { Left, Right, Top, Bottom };

// GPIO mapping for MCU pins — connects a schematic pin name to a
// physical port register bit so the emulator can drive it live.
struct GpioMapping {
    std::string port_name;  // e.g. "PORTB"
    int         bit = 0;    // 0-7
};

struct PinDef {
    std::string                  name;
    PinSide                      side   = PinSide::Right;
    float                        offset = 0.0f;
    std::optional<GpioMapping>   gpio;  // set only for MCU GPIO pins
};

struct Variant {
    std::string id;
    std::string asset_path;
};

struct RenderDef {
    std::string              asset_path;
    std::array<float, 2>     size         = {80.0f, 120.0f}; // [w, h]
    std::string              pin_anchor_mode = "metadata";    // "metadata" | "auto"
};

struct ComponentLibEntry {
    std::string          type;
    std::string          category;
    std::vector<PinDef>  pins;
    RenderDef            render;
    std::vector<Variant> variants;
};

} // namespace circuit
