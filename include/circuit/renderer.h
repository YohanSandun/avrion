#pragma once
#include <string>
#include <unordered_map>
#include "imgui.h"
#include "circuit/schema.h"

namespace circuit {

// net_id -> is_high (true = logic high).  Updated every frame from the emulator.
using NetLevels = std::unordered_map<std::string, bool>;

class CircuitRenderer {
public:
    CircuitRenderer()  = default;
    ~CircuitRenderer();

    // Call once after parsing; loads textures and caches component data.
    void load_circuit(const CircuitDef& def);

    // Call every ImGui frame inside an ImGui window.
    void render();

    // Reset pan/zoom to fit all components on screen.
    void reset_view();

    // Push live net levels from the emulator (call each frame before render()).
    void set_net_levels(const NetLevels& levels) { m_net_levels = levels; }

private:
    // --- state ---
    CircuitDef m_def;

    // Pan / zoom
    ImVec2 m_canvas_offset{0.0f, 0.0f};
    float  m_canvas_scale {1.0f};

    // Texture cache: path -> GL texture id (as ImTextureID)
    std::unordered_map<std::string, ImTextureID> m_texture_cache;

    // Live simulation state: net_id -> logic level
    NetLevels m_net_levels;

    // Reverse index built in load_circuit: "inst_id.pin_name" -> net_id
    std::unordered_map<std::string, std::string> m_pin_to_net;

    // --- helpers ---
    ImTextureID load_texture(const std::string& path);

    // Returns true if the named pin on this component's connected net is logic-high.
    bool is_pin_high(const ComponentInstance& inst, const std::string& pin_name) const;

    // Convert a world point to canvas (screen-space) position.
    ImVec2 world_to_canvas(const ImVec2& origin, float wx, float wy) const;

    // Compute the world position of a pin for a given component instance.
    ImVec2 pin_world_pos(const ComponentInstance& inst,
                         const PinDef&            pin,
                         const RenderDef&         render) const;

    void draw_component(ImDrawList* dl, const ImVec2& origin,
                        const ComponentInstance& inst);
    void draw_wires(ImDrawList* dl, const ImVec2& origin);
    void draw_net_labels(ImDrawList* dl, const ImVec2& origin);
};

} // namespace circuit
