cmake_minimum_required(VERSION 3.20)
project(avr_emu_gui LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(OpenGL REQUIRED)

# ---- GLFW ----
# If vendored:
add_subdirectory(third_party/glfw)

# If using system glfw instead, comment out add_subdirectory and use:
# find_package(glfw3 REQUIRED)

# ---- ImGui (vendored sources) ----
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

# ---- Core library (shared emulator logic) ----
add_library(avr_core STATIC
  apps/tools/intel_hex_decoder.cpp
)

target_include_directories(avr_core PUBLIC
  ${CMAKE_SOURCE_DIR}/include
)

# ---- Applications ----
add_subdirectory(apps/gui)
add_subdirectory(apps/cli)

# ---- Tests ----
enable_testing()
add_subdirectory(tests)