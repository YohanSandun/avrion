add_executable(avr_gui
  src/main.cpp
)

target_link_libraries(avr_gui PRIVATE imgui glfw OpenGL::GL)

if (APPLE)
  target_link_libraries(avr_gui PRIVATE
    "-framework Cocoa"
    "-framework IOKit"
    "-framework CoreVideo"
  )
endif()
