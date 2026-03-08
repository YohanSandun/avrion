add_executable(test_intel_hex_decoder
  test_intel_hex_decoder.cpp
)

target_link_libraries(test_intel_hex_decoder PRIVATE avr_core Catch2::Catch2WithMain)

target_compile_definitions(test_intel_hex_decoder PRIVATE
  TEST_DATA_DIR="${CMAKE_SOURCE_DIR}/tests/data"
)

include(Catch)
catch_discover_tests(test_intel_hex_decoder)

# ---- Instruction unit tests ------------------------------------------------
add_executable(test_instructions
  test_instructions.cpp
)

target_link_libraries(test_instructions PRIVATE avr_core Catch2::Catch2WithMain)

catch_discover_tests(test_instructions)
