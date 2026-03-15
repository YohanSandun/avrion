add_executable(test_intel_hex_decoder
  test_intel_hex_decoder.cpp
)

target_link_libraries(test_intel_hex_decoder PRIVATE avr_core Catch2::Catch2WithMain)

target_compile_definitions(test_intel_hex_decoder PRIVATE
  TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

include(Catch)
catch_discover_tests(test_intel_hex_decoder)

# ---- Instruction unit tests — ALU ------------------------------------------
add_executable(test_instructions_alu
  test_instructions_alu.cpp
)

target_link_libraries(test_instructions_alu PRIVATE avr_core Catch2::Catch2WithMain)

catch_discover_tests(test_instructions_alu)

# ---- Instruction unit tests — Branch ---------------------------------------
add_executable(test_instructions_branch
  test_instructions_branch.cpp
)

target_link_libraries(test_instructions_branch PRIVATE avr_core Catch2::Catch2WithMain)

catch_discover_tests(test_instructions_branch)

# ---- Instruction unit tests — Data Transfer --------------------------------
add_executable(test_instructions_data_transfer
  test_instructions_data_transfer.cpp
)

target_link_libraries(test_instructions_data_transfer PRIVATE avr_core Catch2::Catch2WithMain)

catch_discover_tests(test_instructions_data_transfer)

# ---- Instruction unit tests — Misc --------------------------------
add_executable(test_instructions_misc
  test_instructions_misc.cpp
)

target_link_libraries(test_instructions_misc PRIVATE avr_core Catch2::Catch2WithMain)

catch_discover_tests(test_instructions_misc)

# ---- Timer8 unit tests -----------------------------------------------------
add_executable(test_timer8
  test_timer8.cpp
)

target_link_libraries(test_timer8 PRIVATE avr_core Catch2::Catch2WithMain)

catch_discover_tests(test_timer8)

# ---- Interrupt unit tests ---------------------------------------------------
add_executable(test_interrupts
  test_interrupts.cpp
)

target_link_libraries(test_interrupts PRIVATE avr_core Catch2::Catch2WithMain)

catch_discover_tests(test_interrupts)

# ---- USART unit tests -------------------------------------------------------
add_executable(test_usart
  test_usart.cpp
)

target_link_libraries(test_usart PRIVATE avr_core Catch2::Catch2WithMain)

catch_discover_tests(test_usart)
