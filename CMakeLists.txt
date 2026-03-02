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

add_executable(app
  frontend/main.cpp
)

target_link_libraries(app PRIVATE imgui glfw OpenGL::GL)

if (APPLE)
  target_link_libraries(app PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()