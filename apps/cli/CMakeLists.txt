add_executable(avr_cli
  src/main.cpp
)

target_link_libraries(avr_cli PRIVATE avr_core)
