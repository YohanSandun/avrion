# avrion

A cycle-accurate AVR 8-bit microcontroller simulator targeting the ATmega328P, written in C++20.

**[🌐 Live Demo — WebAssembly build](https://yohansandun.github.io/avrion/)**
Load any compiled `.hex` file and observe GPIO pin states in real time, directly in the browser.

---

## Apps

| App | Description |
|-----|-------------|
| **CLI** | Command-line runner — loads a `.hex` file and prints GPIO pin changes to stdout at real-time 16 MHz speed |
| **GUI** | Native desktop application built with ImGui + GLFW for visual simulation and inspection |
| **WASM** | React + TypeScript web app powered by an Emscripten-compiled WASM build of the simulator — upload a `.hex`, run, pause, step, and toggle input pins interactively |

---

## Building

### Prerequisites
- CMake 3.20+
- A C++20-capable compiler (MSVC, GCC, Clang)
- OpenGL drivers (for GUI)

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

### Using build.bat (Windows)

```bat
build.bat cli            # build CLI
build.bat cli --run      # build and run CLI
build.bat gui --run      # build and run GUI
build.bat gui --run --config Release
build.bat wasm           # build WebAssembly module (requires Emscripten)
```

---

## WebAssembly Build

### Prerequisites
- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) installed and activated

### Build

```bat
build.bat wasm
```

This runs `emcmake cmake` + `cmake --build` and writes `avrion_wasm.js` and `avrion_wasm.wasm` into `apps/wasm/web/public/`.

### Run the web app locally

```bash
cd apps/wasm/web
npm install
npm run dev
```

Open `http://localhost:5173/`, drop a `.hex` file onto the page, and hit **Run**.

---

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
