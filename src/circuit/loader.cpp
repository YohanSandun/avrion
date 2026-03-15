#include "circuit/loader.h"
#include <yaml-cpp/yaml.h>
#include <stdexcept>
#include <filesystem>

namespace circuit {

// -------------------------------------------------------------------------
// Library YAML parser
// -------------------------------------------------------------------------
static PinSide parse_side(const std::string& s) {
    if (s == "left")   return PinSide::Left;
    if (s == "right")  return PinSide::Right;
    if (s == "top")    return PinSide::Top;
    if (s == "bottom") return PinSide::Bottom;
    return PinSide::Right;
}

ComponentLibEntry load_library(const std::string& yaml_path) {
    YAML::Node doc;
    try {
        doc = YAML::LoadFile(yaml_path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Failed to load library '" + yaml_path + "': " + e.what());
    }

    ComponentLibEntry entry;
    entry.type     = doc["type"]     ? doc["type"].as<std::string>()     : "";
    entry.category = doc["category"] ? doc["category"].as<std::string>() : "";

    if (doc["pins"] && doc["pins"].IsSequence()) {
        for (const auto& pn : doc["pins"]) {
            PinDef pd;
            pd.name   = pn["name"]   ? pn["name"].as<std::string>() : "";
            pd.side   = pn["side"]   ? parse_side(pn["side"].as<std::string>()) : PinSide::Right;
            pd.offset = pn["offset"] ? pn["offset"].as<float>() : 0.0f;
            if (pn["gpio"] && pn["gpio"].IsMap()) {
                GpioMapping gm;
                gm.port_name = pn["gpio"]["port"] ? pn["gpio"]["port"].as<std::string>() : "";
                gm.bit       = pn["gpio"]["bit"]  ? pn["gpio"]["bit"].as<int>()          : 0;
                pd.gpio = std::move(gm);
            }
            entry.pins.push_back(std::move(pd));
        }
    }

    if (doc["render"]) {
        const auto& rn = doc["render"];
        entry.render.asset_path      = rn["asset"]            ? rn["asset"].as<std::string>()            : "";
        entry.render.pin_anchor_mode = rn["pin_anchor_mode"]  ? rn["pin_anchor_mode"].as<std::string>()  : "metadata";
        if (rn["size"] && rn["size"].IsSequence() && rn["size"].size() >= 2) {
            entry.render.size[0] = rn["size"][0].as<float>();
            entry.render.size[1] = rn["size"][1].as<float>();
        }
    }

    if (doc["variants"] && doc["variants"].IsSequence()) {
        for (const auto& vn : doc["variants"]) {
            Variant v;
            v.id         = vn["id"]    ? vn["id"].as<std::string>()    : "";
            v.asset_path = vn["asset"] ? vn["asset"].as<std::string>() : "";
            entry.variants.push_back(std::move(v));
        }
    }

    return entry;
}

// -------------------------------------------------------------------------
// Circuit YAML parser
// -------------------------------------------------------------------------
CircuitDef load_circuit(const std::string& yaml_path, const std::string& assets_base) {
    YAML::Node doc;
    try {
        doc = YAML::LoadFile(yaml_path);
    } catch (const YAML::Exception& e) {
        throw std::runtime_error("Failed to load circuit '" + yaml_path + "': " + e.what());
    }

    CircuitDef def;
    def.version = doc["version"] ? doc["version"].as<int>() : 1;

    // --- library aliases ---
    if (doc["library"] && doc["library"].IsMap()) {
        for (const auto& kv : doc["library"]) {
            std::string alias = kv.first.as<std::string>();
            std::string path  = kv.second.as<std::string>();
            def.library_paths[alias] = path;

            // Resolve and load the library file
            std::string lib_file = assets_base + "/libraries/" + path + ".yaml";
            if (std::filesystem::exists(lib_file)) {
                try {
                    def.lib_entries[alias] = load_library(lib_file);
                } catch (...) {
                    // Library file missing or malformed — leave entry absent
                }
            }
        }
    }

    // --- components ---
    if (doc["components"] && doc["components"].IsSequence()) {
        for (const auto& cn : doc["components"]) {
            ComponentInstance inst;
            inst.id    = cn["id"]    ? cn["id"].as<std::string>()    : "";
            inst.type  = cn["type"]  ? cn["type"].as<std::string>()  : "";
            inst.value = cn["value"] && !cn["value"].IsNull() ? cn["value"].as<std::string>() : "";

            if (cn["props"] && cn["props"].IsMap()) {
                for (const auto& pkv : cn["props"]) {
                    inst.props.values[pkv.first.as<std::string>()] =
                        pkv.second.IsNull() ? "" : pkv.second.as<std::string>();
                }
            }

            if (cn["layout"]) {
                const auto& ln = cn["layout"];
                inst.layout.x        = ln["x"]        ? ln["x"].as<float>()        : 0.0f;
                inst.layout.y        = ln["y"]        ? ln["y"].as<float>()        : 0.0f;
                inst.layout.rotation = ln["rotation"] ? ln["rotation"].as<float>() : 0.0f;
                inst.layout.symbol   = ln["symbol"]   ? ln["symbol"].as<std::string>() : "";
            }

            def.components.push_back(std::move(inst));
        }
    }

    // --- nets ---
    if (doc["nets"] && doc["nets"].IsSequence()) {
        for (const auto& nn : doc["nets"]) {
            NetDef nd;
            nd.id = nn["id"] ? nn["id"].as<std::string>() : "";
            if (nn["connects"] && nn["connects"].IsSequence()) {
                for (const auto& c : nn["connects"])
                    nd.connects.push_back(c.as<std::string>());
            }
            def.nets.push_back(std::move(nd));
        }
    }

    // --- wire_routes ---
    if (doc["wire_routes"] && doc["wire_routes"].IsSequence()) {
        for (const auto& wn : doc["wire_routes"]) {
            WireRoute wr;
            wr.net = wn["net"] ? wn["net"].as<std::string>() : "";
            if (wn["points"] && wn["points"].IsSequence()) {
                for (const auto& pt : wn["points"]) {
                    if (pt.IsSequence() && pt.size() >= 2) {
                        wr.points.push_back({pt[0].as<float>(), pt[1].as<float>()});
                    }
                }
            }
            def.wire_routes.push_back(std::move(wr));
        }
    }

    // --- styles ---
    if (doc["styles"]) {
        const auto& sn = doc["styles"];
        def.styles.theme = sn["theme"] ? sn["theme"].as<std::string>() : "default";
        if (sn["grid"]) {
            def.styles.grid.size    = sn["grid"]["size"]    ? sn["grid"]["size"].as<float>()    : 20.0f;
            def.styles.grid.visible = sn["grid"]["visible"] ? sn["grid"]["visible"].as<bool>()  : true;
        }
    }

    return def;
}

} // namespace circuit
