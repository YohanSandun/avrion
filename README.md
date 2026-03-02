# avrion
AVR Simulator

## Building

### Prerequisites
- CMake 3.20+
- A C++20-capable compiler (MSVC, GCC, Clang)
- OpenGL drivers

### Configure

```bash
cmake -B build
```

### Build all targets

```bash
cmake --build build
```

### Build a specific target

```bash
# GUI application
cmake --build build --target avr_gui

# CLI application
cmake --build build --target avr_cli
```

### Build in release mode

```bash
cmake --build build --config Release
```

## Testing

### Build and run all tests

```bash
cmake --build build --target test_intel_hex_decoder
ctest --test-dir build --output-on-failure
```

### Run a specific test suite

```bash
ctest --test-dir build --output-on-failure -R <test-name-pattern>
```

### Run tests in release mode

```bash
cmake --build build --config Release --target test_intel_hex_decoder
ctest --test-dir build -C Release --output-on-failure
```
