if(NOT EMSCRIPTEN)
  return()
endif()

add_executable(avrion_wasm
  src/avrion_wasm.cpp
)

target_link_libraries(avrion_wasm PRIVATE avr_core)

# Output the .js + .wasm files directly into the web app's public directory
# so Vite serves them as static assets during dev and picks them up in the build.
set(WASM_PUBLIC_DIR "${CMAKE_SOURCE_DIR}/apps/wasm/web/public")

set_target_properties(avrion_wasm PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${WASM_PUBLIC_DIR}"
)

# Emscripten link flags:
#  --bind                  enable Embind JS<->C++ bindings
#  MODULARIZE=1            wrap output in a factory function
#  EXPORT_NAME             global name of the factory (UMD, no ES6 module)
#  ALLOW_MEMORY_GROWTH=1   let WASM heap grow as needed
#  ENVIRONMENT=web         target browser only
# NOTE: EXPORT_ES6 is intentionally omitted — the UMD build is loaded via a
# plain <script> tag at runtime (Vite cannot module-import files from /public).
target_link_options(avrion_wasm PRIVATE
  "--bind"
  "-sWASM=1"
  "-sMODULARIZE=1"
  "-sEXPORT_NAME=createAvrionModule"
  "-sEXPORTED_RUNTIME_METHODS=[]"
  "-sALLOW_MEMORY_GROWTH=1"
  "-sENVIRONMENT=web"
  "--no-entry"
)
