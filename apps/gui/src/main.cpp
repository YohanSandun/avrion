#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"
#include "circuit/loader.h"
#include "circuit/renderer.h"

// AVR emulator — same pattern as the CLI app
#include "device/atmega328p.h"
#include "periph/gpio.h"
#include "intel_hex_decoder.h"

#include <GLFW/glfw3.h>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <vector>

static void glfw_error_callback(int error, const char* description) {
  std::fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "Usage: avr_gui <circuit.yaml> [assets_base]\n");
    std::fprintf(stderr, "  circuit.yaml : path to the circuit definition file\n");
    std::fprintf(stderr, "  assets_base  : (optional) root folder for library assets (default: assets)\n");
    return 1;
  }

  const char* yaml_path   = argv[1];
  const char* assets_base = (argc >= 3) ? argv[2] : "assets";

  // --- Load circuit ---
  circuit::CircuitDef def;
  try {
    def = circuit::load_circuit(yaml_path, assets_base);
  } catch (const std::exception& e) {
    std::fprintf(stderr, "Error loading circuit: %s\n", e.what());
    return 1;
  }

  // -----------------------------------------------------------------------
  // Emulator setup — mirrors what the CLI does in wire_trace()
  // -----------------------------------------------------------------------
  avrion::ATmega328P dev;

  // Load firmware if any component specifies props.firmware
  for (const auto& inst : def.components) {
    auto it = inst.props.values.find("firmware");
    if (it != inst.props.values.end() && !it->second.empty()) {
      try {
        auto flash_data = IntelHexDecoder::decodeFile(it->second);
        dev.load_flash(0, flash_data.data(), flash_data.size());
        dev.reset();
        std::fprintf(stderr, "[emu] Loaded '%s' for component '%s'\n",
                     it->second.c_str(), inst.id.c_str());
      } catch (const std::exception& e) {
        std::fprintf(stderr, "[emu] Warning: could not load firmware '%s': %s\n",
                     it->second.c_str(), e.what());
      }
      break; // one MCU
    }
  }

  // Build a port -> [(bit, net_id)] index so GPIO callbacks can update nets.
  // This uses the gpio: {port: PORTB, bit: 5} fields from the library YAML.
  struct BitNetEntry { int bit; std::string net_id; };
  std::unordered_map<std::string, std::vector<BitNetEntry>> port_net_index;

  for (const auto& inst : def.components) {
    auto lib_it = def.lib_entries.find(inst.type);
    if (lib_it == def.lib_entries.end()) continue;
    for (const auto& pin : lib_it->second.pins) {
      if (!pin.gpio) continue;
      const std::string conn_key = inst.id + "." + pin.name;
      for (const auto& net : def.nets) {
        for (const auto& conn : net.connects) {
          if (conn == conn_key)
            port_net_index[pin.gpio->port_name].push_back({pin.gpio->bit, net.id});
        }
      }
    }
  }

  // Shared state: emulator thread writes, render thread reads
  std::mutex         g_levels_mutex;
  circuit::NetLevels g_net_levels;

  // Register one on_change callback per port (same API as CLI's wire_trace)
  for (const auto& [port_name, entries] : port_net_index) {
    auto* port = dev.get_peripheral<avrion::GpioPort>(port_name.c_str());
    if (!port) {
      std::fprintf(stderr, "[emu] Warning: peripheral '%s' not found\n", port_name.c_str());
      continue;
    }
    port->set_on_change([&g_levels_mutex, &g_net_levels, entries]
                        (uint8_t /*old_pins*/, uint8_t new_pins) {
      std::lock_guard<std::mutex> lk(g_levels_mutex);
      for (const auto& e : entries)
        g_net_levels[e.net_id] = static_cast<bool>((new_pins >> e.bit) & 1u);
    });
  }

  // Run emulator on a background thread (same as CLI's run_realtime)
  std::atomic<bool> g_emu_stop{false};
  std::thread emu_thread([&dev, &g_emu_stop]() {
    try {
      dev.run_realtime(g_emu_stop);
    } catch (const std::exception& e) {
      std::fprintf(stderr, "[emu] Fatal: %s\n", e.what());
    }
  });

  // --- GLFW / OpenGL setup ---
  glfwSetErrorCallback(glfw_error_callback);
  if (!glfwInit()) {
    g_emu_stop.store(true);
    emu_thread.join();
    return 1;
  }

  const char* glsl_version = "#version 130";
  GLFWwindow* window = glfwCreateWindow(1280, 720, "AVRion Circuit Viewer", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    g_emu_stop.store(true);
    emu_thread.join();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init(glsl_version);

  // --- Circuit renderer ---
  circuit::CircuitRenderer renderer;
  renderer.load_circuit(def);

  // --- Main loop ---
  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Push latest net levels (written by emulator callbacks) into the renderer
    {
      std::lock_guard<std::mutex> lk(g_levels_mutex);
      renderer.set_net_levels(g_net_levels);
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Full-screen circuit canvas — transparent window so only the canvas bg is visible
    int fb_w, fb_h;
    glfwGetFramebufferSize(window, &fb_w, &fb_h);
    ImGui::SetNextWindowPos({0.0f, 0.0f});
    ImGui::SetNextWindowSize({static_cast<float>(fb_w), static_cast<float>(fb_h)});
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::Begin("##circuit_root", nullptr,
                 ImGuiWindowFlags_NoTitleBar      | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoMove          | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoScrollbar     | ImGuiWindowFlags_NoBackground);
    ImGui::PopStyleColor();
    renderer.render();
    ImGui::End();

    ImGui::Render();
    glViewport(0, 0, fb_w, fb_h);
    glClearColor(0.1f, 0.11f, 0.13f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    glfwSwapBuffers(window);
  }

  // Stop and join the emulator thread cleanly
  g_emu_stop.store(true);
  if (emu_thread.joinable()) emu_thread.join();

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
