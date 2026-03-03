add_executable(avr_cli
  src/main.cpp
)

target_compile_definitions(avr_cli PRIVATE
  BLINK_HEX_PATH="${CMAKE_SOURCE_DIR}/tests/data/blink.hex"
)

target_link_libraries(avr_cli PRIVATE avr_core)
