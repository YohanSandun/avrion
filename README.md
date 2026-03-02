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
