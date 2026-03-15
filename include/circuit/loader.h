#pragma once
#include <string>
#include "circuit/schema.h"
#include "circuit/library.h"

namespace circuit {

// Load a circuit YAML file.
// assets_base: root directory for resolving library paths
//              (e.g. "assets" → looks for "assets/libraries/mcu/atmega328p.yaml")
// Throws std::runtime_error on parse failure.
CircuitDef load_circuit(const std::string& yaml_path,
                        const std::string& assets_base = "assets");

// Load a single component library YAML file.
// Throws std::runtime_error on parse failure.
ComponentLibEntry load_library(const std::string& yaml_path);

} // namespace circuit
