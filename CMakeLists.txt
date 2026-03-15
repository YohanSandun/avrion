cmake_minimum_required(VERSION 3.20)
project(avr_emu_gui LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Native-only dependencies (GLFW, OpenGL, ImGui) ----
# These are NOT built when cross-compiling to WebAssembly.
if(NOT EMSCRIPTEN)
  find_package(OpenGL REQUIRED)

  # GLFW (vendored)
  add_subdirectory(third_party/glfw)

  # ImGui (vendored sources)
  set(IMGUI_DIR ${CMAKE_SOURCE_DIR}/third_party/imgui)

  add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp

    ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
    ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp
  )

  target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_DIR}/backends
  )

  # ImGui needs GLFW + OpenGL
  target_link_libraries(imgui PUBLIC glfw OpenGL::GL)
endif()

# ---- Core library (shared emulator logic) ----
add_library(avr_core STATIC
  apps/tools/intel_hex_decoder.cpp
  src/cpu/avr_cpu.cpp
  src/cpu/decoder.cpp
  src/cpu/instruction_table.cpp
  src/cpu/instructions/misc.cpp
  src/cpu/instructions/branch.cpp
  src/cpu/instructions/alu.cpp
  src/cpu/instructions/data_transfer.cpp
  src/device/device.cpp
  src/device/atmega328p.cpp
  src/memory/memory_map.cpp
  src/periph/gpio.cpp
  src/periph/timer8.cpp
  src/periph/usart.cpp
)

target_include_directories(avr_core PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

# ---- Applications ----
if(NOT EMSCRIPTEN)
  add_subdirectory(apps/gui)
  add_subdirectory(apps/cli)
endif()

# ---- WebAssembly app (Emscripten only) ----
if(EMSCRIPTEN)
  add_subdirectory(apps/wasm)
endif()

# ---- Tests ----
if(NOT EMSCRIPTEN)
  enable_testing()
  add_subdirectory(tests)
endif()