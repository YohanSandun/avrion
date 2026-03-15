#pragma once
#include <string>
#include <vector>
#include <array>
#include <unordered_map>
#include "circuit/library.h"

namespace circuit {

struct ComponentLayout {
    float       x        = 0.0f;
    float       y        = 0.0f;
    float       rotation = 0.0f;
    std::string symbol;    // optional variant id override
};

struct ComponentProps {
    std::unordered_map<std::string, std::string> values; // arbitrary key->string
};

struct ComponentInstance {
    std::string      id;
    std::string      type;    // refers to library alias key in CircuitDef
    std::string      value;   // schematic value label (e.g. "1k", "10uF")
    ComponentProps   props;
    ComponentLayout  layout;
};

struct NetDef {
    std::string              id;
    std::vector<std::string> connects; // e.g. ["u1.PB0", "r1.1"]
};

struct WireRoute {
    std::string                      net;
    std::vector<std::array<float,2>> points;
};

struct GridStyle {
    float size    = 20.0f;
    bool  visible = true;
};

struct StyleConfig {
    std::string theme = "default";
    GridStyle   grid;
};

struct CircuitDef {
    int version = 1;

    // library alias -> lib-path (e.g. "atmega328p" -> "mcu/atmega328p")
    std::unordered_map<std::string, std::string> library_paths;

    // resolved lib entries keyed by the ALIAS used in components
    std::unordered_map<std::string, ComponentLibEntry> lib_entries;

    std::vector<ComponentInstance> components;
    std::vector<NetDef>            nets;
    std::vector<WireRoute>         wire_routes;
    StyleConfig                    styles;
};

} // namespace circuit
