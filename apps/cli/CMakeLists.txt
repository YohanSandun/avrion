add_executable(avr_cli
  src/main.cpp
)

target_include_directories(avr_cli PRIVATE
  ${CMAKE_SOURCE_DIR}/include
)
