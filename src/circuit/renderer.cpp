#include "circuit/renderer.h"
#include "circuit/library.h"
#include "imgui.h"
#include <stb_image.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>

// OpenGL — use the platform-appropriate header
#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <windows.h>
#  include <GL/gl.h>
// GL_CLAMP_TO_EDGE is GL 1.2 — not in old Windows gl.h; define it manually
#  ifndef GL_CLAMP_TO_EDGE
#    define GL_CLAMP_TO_EDGE 0x812F
#  endif
#else
#  include <GL/gl.h>
#endif

namespace circuit {

// A zero ImTextureID means "no texture" on all ImGui backends
static constexpr ImTextureID kNoTexture = static_cast<ImTextureID>(0);

// ---------------------------------------------------------------------------
// Destructor — release cached textures
// ---------------------------------------------------------------------------
CircuitRenderer::~CircuitRenderer() {
    for (auto& [path, tex] : m_texture_cache) {
        GLuint id = static_cast<GLuint>(static_cast<uintptr_t>(tex));
        if (id) glDeleteTextures(1, &id);
    }
}

// ---------------------------------------------------------------------------
// load_circuit
// ---------------------------------------------------------------------------
void CircuitRenderer::load_circuit(const CircuitDef& def) {
    m_def = def;

    // Build pin-to-net reverse index so is_pin_high() is O(1) per call.
    m_pin_to_net.clear();
    for (const auto& net : def.nets) {
        for (const auto& conn : net.connects) {
            m_pin_to_net[conn] = net.id;
        }
    }

    // Pre-load textures for all components
    for (const auto& inst : m_def.components) {
        auto it = m_def.lib_entries.find(inst.type);
        if (it == m_def.lib_entries.end()) continue;
        const ComponentLibEntry& lib = it->second;

        // Pick asset path: variant override first, then default
        std::string asset_path = lib.render.asset_path;
        if (!inst.layout.symbol.empty()) {
            for (const auto& v : lib.variants) {
                if (v.id == inst.layout.symbol && !v.asset_path.empty()) {
                    asset_path = v.asset_path;
                    break;
                }
            }
        }
        if (!asset_path.empty())
            load_texture(asset_path);
    }
    reset_view();
}

// ---------------------------------------------------------------------------
// reset_view
// ---------------------------------------------------------------------------
void CircuitRenderer::reset_view() {
    m_canvas_offset = {0.0f, 0.0f};
    m_canvas_scale  = 1.0f;
}

// ---------------------------------------------------------------------------
// load_texture — decode PNG via stb_image, upload to OpenGL, cache it
// ---------------------------------------------------------------------------
ImTextureID CircuitRenderer::load_texture(const std::string& path) {
    auto it = m_texture_cache.find(path);
    if (it != m_texture_cache.end()) return it->second;

    int w = 0, h = 0, channels = 0;
    // stb_image loads top-to-bottom; GL stores row-0 at texture-bottom.
    // UV(0,0) in GL = texture bottom = image top-left, so standard {0,0}→{1,1}
    // UVs in AddImage display the image correctly — no flip needed.
    stbi_uc* data = stbi_load(path.c_str(), &w, &h, &channels, STBI_rgb_alpha);
    if (!data) {
        std::fprintf(stderr, "[circuit] Failed to load texture '%s': %s\n",
                     path.c_str(), stbi_failure_reason());
        m_texture_cache[path] = kNoTexture;
        return kNoTexture;
    }

    GLuint tex_id = 0;
    glGenTextures(1, &tex_id);
    glBindTexture(GL_TEXTURE_2D, tex_id);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    ImTextureID imgui_tex = static_cast<ImTextureID>(static_cast<uintptr_t>(tex_id));
    m_texture_cache[path] = imgui_tex;
    return imgui_tex;
}

// ---------------------------------------------------------------------------
// is_pin_high — returns true if the net connected to inst.id+"."pin_name
//               is currently logic-high according to m_net_levels.
// ---------------------------------------------------------------------------
bool CircuitRenderer::is_pin_high(const ComponentInstance& inst,
                                   const std::string& pin_name) const {
    auto pit = m_pin_to_net.find(inst.id + "." + pin_name);
    if (pit == m_pin_to_net.end()) return false;
    auto lit = m_net_levels.find(pit->second);
    return lit != m_net_levels.end() && lit->second;
}

// ---------------------------------------------------------------------------
// Coordinate helpers
// ---------------------------------------------------------------------------
ImVec2 CircuitRenderer::world_to_canvas(const ImVec2& origin, float wx, float wy) const {
    return {origin.x + m_canvas_offset.x + wx * m_canvas_scale,
            origin.y + m_canvas_offset.y + wy * m_canvas_scale};
}

ImVec2 CircuitRenderer::pin_world_pos(const ComponentInstance& inst,
                                       const PinDef&            pin,
                                       const RenderDef&         render) const {
    float cw = render.size[0];
    float ch = render.size[1];
    float cx = inst.layout.x;
    float cy = inst.layout.y;

    float px = 0.0f, py = 0.0f;
    switch (pin.side) {
        case PinSide::Left:   px = cx;        py = cy + pin.offset; break;
        case PinSide::Right:  px = cx + cw;   py = cy + pin.offset; break;
        case PinSide::Top:    px = cx + pin.offset; py = cy;         break;
        case PinSide::Bottom: px = cx + pin.offset; py = cy + ch;    break;
    }
    return {px, py};
}

// ---------------------------------------------------------------------------
// draw_component
// ---------------------------------------------------------------------------
void CircuitRenderer::draw_component(ImDrawList* dl, const ImVec2& origin,
                                      const ComponentInstance& inst) {
    auto lib_it = m_def.lib_entries.find(inst.type);

    float scale = m_canvas_scale;
    float cx    = inst.layout.x;
    float cy    = inst.layout.y;

    // Default box size in case we have no lib entry
    float cw = 80.0f, ch = 120.0f;

    std::string asset_path;
    const ComponentLibEntry* lib = nullptr;

    if (lib_it != m_def.lib_entries.end()) {
        lib  = &lib_it->second;
        cw   = lib->render.size[0];
        ch   = lib->render.size[1];

        asset_path = lib->render.asset_path;
        if (!inst.layout.symbol.empty()) {
            for (const auto& v : lib->variants) {
                if (v.id == inst.layout.symbol && !v.asset_path.empty()) {
                    asset_path = v.asset_path;
                    break;
                }
            }
        }
    }

    ImVec2 tl = world_to_canvas(origin, cx, cy);
    ImVec2 br = {tl.x + cw * scale, tl.y + ch * scale};

    // Determine LED glow for "display" category components:
    // if anode pin ("A") is driven high, apply a warm tint.
    ImU32 img_tint = IM_COL32(255, 255, 255, 255);
    bool  led_on   = false;
    if (lib && lib->category == "display") {
        led_on   = is_pin_high(inst, "A");
        if (led_on)
            img_tint = IM_COL32(255, 120, 60, 255); // warm orange glow
    }

    // Draw body — PNG texture or fallback rectangle
    ImTextureID tex = asset_path.empty() ? kNoTexture : load_texture(asset_path);
    if (tex) {
        // UV (0,0)=texture-bottom=image-top-left → renders correctly
        dl->AddImage(tex, tl, br, {0.0f, 0.0f}, {1.0f, 1.0f}, img_tint);
    } else {
        // Fallback: schematic-style box
        ImU32 fill = (led_on && lib && lib->category == "display")
                         ? IM_COL32(200, 60, 20, 255)   // LED lit: warm red
                         : IM_COL32(52, 76, 102, 255);  // default: slate blue
        dl->AddRectFilled(tl, br, fill);
        dl->AddRect(tl, br, IM_COL32(110, 185, 255, 255), 0.0f, 0, 2.0f);

        // Component ID (bold white)
        ImVec2 id_pos   = {tl.x + 5.0f * scale, tl.y + 5.0f * scale};
        dl->AddText(id_pos, IM_COL32(255, 255, 255, 255), inst.id.c_str());

        // Component type (light blue, below ID)
        ImVec2 type_pos = {tl.x + 5.0f * scale, tl.y + 18.0f * scale};
        dl->AddText(type_pos, IM_COL32(160, 210, 255, 220), inst.type.c_str());

        // Value label if set
        if (!inst.value.empty()) {
            ImVec2 val_pos = {tl.x + 5.0f * scale, tl.y + 31.0f * scale};
            dl->AddText(val_pos, IM_COL32(220, 255, 180, 200), inst.value.c_str());
        }
    }

    // Draw pins
    if (lib) {
        for (const auto& pin : lib->pins) {
            ImVec2 wp  = pin_world_pos(inst, pin, lib->render);
            ImVec2 cp  = world_to_canvas(origin, wp.x, wp.y);

            // Small pin dot
            dl->AddCircleFilled(cp, 3.5f * scale, IM_COL32(255, 225, 50, 255));
            dl->AddCircle(cp, 3.5f * scale, IM_COL32(200, 160, 20, 255), 12, 1.0f);

            // Pin label (only rendered if scale >= 0.6 to avoid clutter)
            if (scale >= 0.6f) {
                ImVec2 text_pos = cp;
                constexpr float lpad = 6.0f;
                switch (pin.side) {
                    case PinSide::Left:   text_pos.x -= lpad + 30.0f; text_pos.y -= 6.0f; break;
                    case PinSide::Right:  text_pos.x += lpad;          text_pos.y -= 6.0f; break;
                    case PinSide::Top:    text_pos.x += lpad;          text_pos.y -= 16.0f; break;
                    case PinSide::Bottom: text_pos.x += lpad;          text_pos.y += 2.0f;  break;
                }
                dl->AddText(text_pos, IM_COL32(255, 235, 100, 240), pin.name.c_str());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// draw_wires
// ---------------------------------------------------------------------------
void CircuitRenderer::draw_wires(ImDrawList* dl, const ImVec2& origin) {
    for (const auto& route : m_def.wire_routes) {
        if (route.points.size() < 2) continue;

        // Wire colour reflects live net level:
        //   unknown (no emulator data yet) = muted teal-green
        //   high                           = bright green
        //   low                            = dark grey
        ImU32 wire_color = IM_COL32(50, 180, 90, 200); // unknown / default
        auto  lv         = m_net_levels.find(route.net);
        if (lv != m_net_levels.end())
            wire_color = lv->second ? IM_COL32(0, 255, 60, 255)   // HIGH
                                    : IM_COL32(70, 80, 70, 255);  // LOW

        for (size_t i = 0; i + 1 < route.points.size(); ++i) {
            ImVec2 a = world_to_canvas(origin, route.points[i][0],   route.points[i][1]);
            ImVec2 b = world_to_canvas(origin, route.points[i+1][0], route.points[i+1][1]);
            dl->AddLine(a, b, wire_color, 2.0f * m_canvas_scale);
            dl->AddCircleFilled(a, 3.0f * m_canvas_scale, wire_color);
        }
    }
}

// ---------------------------------------------------------------------------
// draw_net_labels
// ---------------------------------------------------------------------------
void CircuitRenderer::draw_net_labels(ImDrawList* dl, const ImVec2& origin) {
    if (m_canvas_scale < 0.5f) return;

    // For each named net (e.g. vcc, gnd), find connected component pins and
    // draw a small net-id label near the first wire segment midpoint.
    for (const auto& route : m_def.wire_routes) {
        if (route.points.empty()) continue;
        // Label near the first midpoint (between point 0 and 1 if available)
        float lx = route.points[0][0];
        float ly = route.points[0][1];
        if (route.points.size() >= 2) {
            lx = (route.points[0][0] + route.points[1][0]) * 0.5f;
            ly = (route.points[0][1] + route.points[1][1]) * 0.5f;
        }
        ImVec2 pos = world_to_canvas(origin, lx, ly - 12.0f);
        // small background box for readability
        ImVec2 text_end = {pos.x + static_cast<float>(route.net.size()) * 7.0f + 4.0f,
                           pos.y + 13.0f};
        dl->AddRectFilled({pos.x - 2.0f, pos.y - 1.0f}, text_end, IM_COL32(22, 24, 28, 200), 2.0f);
        dl->AddText(pos, IM_COL32(100, 255, 140, 255), route.net.c_str());
    }
}

// ---------------------------------------------------------------------------
// render — main entry point, call every frame inside an ImGui window
// ---------------------------------------------------------------------------
void CircuitRenderer::render() {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0.0f, 0.0f});

    // Toolbar
    if (ImGui::Button("Reset View")) reset_view();
    ImGui::SameLine();
    ImGui::Text("Zoom: %.0f%%", m_canvas_scale * 100.0f);

    ImVec2 canvas_size = ImGui::GetContentRegionAvail();
    if (canvas_size.x < 50.0f) canvas_size.x = 50.0f;
    if (canvas_size.y < 50.0f) canvas_size.y = 50.0f;

    ImGui::BeginChild("##circuit_canvas", canvas_size, false,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImVec2 canvas_origin = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Background
    dl->AddRectFilled(canvas_origin,
                      {canvas_origin.x + canvas_size.x, canvas_origin.y + canvas_size.y},
                      IM_COL32(22, 24, 28, 255));

    // Grid (only when visible and scale >= 0.4)
    if (m_def.styles.grid.visible && m_canvas_scale >= 0.4f) {
        float gs = m_def.styles.grid.size * m_canvas_scale;
        float ox = std::fmod(m_canvas_offset.x, gs);
        float oy = std::fmod(m_canvas_offset.y, gs);
        constexpr ImU32 grid_col = IM_COL32(48, 52, 60, 255);
        for (float x = ox; x < canvas_size.x; x += gs)
            dl->AddLine({canvas_origin.x + x, canvas_origin.y},
                        {canvas_origin.x + x, canvas_origin.y + canvas_size.y},
                        grid_col);
        for (float y = oy; y < canvas_size.y; y += gs)
            dl->AddLine({canvas_origin.x, canvas_origin.y + y},
                        {canvas_origin.x + canvas_size.x, canvas_origin.y + y},
                        grid_col);
    }

    // Capture input
    ImGui::InvisibleButton("##canvas_input", canvas_size,
                            ImGuiButtonFlags_MouseButtonLeft |
                            ImGuiButtonFlags_MouseButtonRight);
    bool hovered = ImGui::IsItemHovered();

    // Pan: drag with left button
    if (hovered && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        m_canvas_offset.x += delta.x;
        m_canvas_offset.y += delta.y;
    }

    // Zoom: mouse wheel
    if (hovered) {
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            ImVec2 mouse_pos = ImGui::GetIO().MousePos;
            float  mx        = mouse_pos.x - canvas_origin.x - m_canvas_offset.x;
            float  my        = mouse_pos.y - canvas_origin.y - m_canvas_offset.y;

            float old_scale = m_canvas_scale;
            m_canvas_scale *= (wheel > 0.0f ? 1.1f : 0.9f);
            m_canvas_scale  = std::clamp(m_canvas_scale, 0.1f, 10.0f);

            // Zoom toward cursor
            float ratio = m_canvas_scale / old_scale;
            m_canvas_offset.x -= mx * (ratio - 1.0f);
            m_canvas_offset.y -= my * (ratio - 1.0f);
        }
    }

    // Draw content
    dl->PushClipRect(canvas_origin,
                     {canvas_origin.x + canvas_size.x, canvas_origin.y + canvas_size.y},
                     true);

    draw_wires(dl, canvas_origin);

    for (const auto& inst : m_def.components)
        draw_component(dl, canvas_origin, inst);

    draw_net_labels(dl, canvas_origin);

    dl->PopClipRect();

    ImGui::EndChild();
    ImGui::PopStyleVar();
}

} // namespace circuit
