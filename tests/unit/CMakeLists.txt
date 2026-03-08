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
