include(FetchContent)
include(CTest)

# ---- Catch2 ----------------------------------------------------------------
FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

# ---- Test suites -----------------------------------------------------------
# Add a new subdirectory here for each test category (unit, integration, ...)
add_subdirectory(unit)
