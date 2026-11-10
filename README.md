# xbe-metal 🍏⚙️

> **Static Recompiler & Multiplatform Runtime for Original Xbox Binaries**  
> Targeting **Apple Silicon (macOS / Metal)** and **WebAssembly (WebGPU / Canvas)**.

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Platform: macOS (Apple Silicon)](https://img.shields.io/badge/Platform-macOS%20ARM64-black?logo=apple)](https://apple.com)
[![Platform: WebAssembly](https://img.shields.io/badge/Platform-WebAssembly%20%2F%20WebGPU-654FF0?logo=webassembly)](https://webassembly.org)

---

## 📖 Overview

`xbe-metal` is a high-performance static recompilation framework and runtime environment designed to take original Xbox executables (`.xbe`) and run them natively on modern devices without traditional emulation overhead.

Instead of interpreting x86 machine code inside an emulator VM at runtime, `xbe-metal`:
1. **Parses & Analyzes** the Xbox executable (`.xbe`), extracting sections, headers, and certificates.
2. **Statically Recompiles** x86 (Intel Pentium III) machine code ahead-of-time into clean, portable C++20.
3. **Executes on Modern Platforms** with a lightweight high-level emulation (HLE) runtime:
   - **macOS (Apple Silicon):** Native ARM64 binary with direct **Apple Metal** graphics and **CoreAudio**.
   - **WebAssembly (Wasm):** In-browser execution powered by **WebGPU** / HTML5 Canvas and **WebAudio**.

---

## 🏛️ Architecture

```
                  ┌──────────────────────┐
                  │    Game (default.xbe)│
                  └──────────┬───────────┘
                             │
                             ▼
                  ┌──────────────────────┐
                  │      xbe-parser      │
                  │ (Headers, Cert, Sec) │
                  └──────────┬───────────┘
                             │
                             ▼
                  ┌──────────────────────┐
                  │     recompiler       │
                  │   (x86 ➔ C++20 AST)  │
                  └──────────┬───────────┘
                             │
            ┌────────────────┴────────────────┐
            ▼                                 ▼
   [ Native macOS Build ]            [ WebAssembly Build ]
   - Architecture: ARM64             - Architecture: wasm32 / wasm64
   - Graphics: Metal                 - Graphics: WebGPU / Canvas
   - Audio: CoreAudio                - Audio: WebAudio
   - Compiler: Apple Clang           - Compiler: Emscripten (emcc)
```

---

## 🚀 Building & Running

### Requirements

* **macOS:**
  * Xcode Command Line Tools (`xcode-select --install`)
  * CMake 3.20+ (`brew install cmake`)
  * macOS 12.0+ with Apple Silicon (M1/M2/M3/M4) recommended

* **WebAssembly (Optional for Web builds):**
  * Emscripten SDK (`emsdk`)

### 1. Building for Native macOS (Apple Silicon + Metal)

```bash
# Clone the repository
git clone https://github.com/your-username/xbe-metal.git
cd xbe-metal

# Configure and compile
cmake -B build -DTARGET_PLATFORM=macOS
cmake --build build -j$(sysctl -n hw.ncpu)

# Run the XBE inspector CLI tool
./build/tools/xbe-dump path/to/default.xbe
```

### 2. Building for WebAssembly (WebGPU)

```bash
# Activate Emscripten environment
source /path/to/emsdk/emsdk_env.sh

# Configure and compile with Emscripten
emcmake cmake -B build-web -DTARGET_PLATFORM=Wasm
cmake --build build-web

# Serve locally
npx serve web/
# Open http://localhost:3000 to launch the in-browser runner
```

---

## 📁 Repository Structure

```
xbe-metal/
├── CMakeLists.txt              # Root CMake build configuration
├── LICENSE                     # MIT License
├── README.md                   # Project documentation
├── include/
│   ├── xbe/                    # XBE binary structures and parser interfaces
│   └── runtime/
│       ├── core/               # Memory map, threads, sync, kernel HLE
│       ├── gpu/                # Cross-platform GPU abstraction (IRenderDevice)
│       │   ├── metal/          # Apple Metal backend (macOS)
│       │   └── webgpu/         # WebGPU backend (WebAssembly)
│       ├── audio/              # CoreAudio & WebAudio backends
│       └── io/                 # Virtual File System (VFS) & Gamepad input
├── src/
│   ├── xbe/                    # XBE parser implementation
│   ├── recompiler/             # Ahead-of-time code translation engine
│   └── runtime/                # HLE implementation files
├── tools/
│   └── xbe-dump/               # CLI inspection tool for .xbe files
└── web/
    ├── index.html              # WebAssembly in-browser runner UI
    └── web_runner.js           # JS bridge for WebGPU / Canvas
```

---

## ⚖️ Legal Disclaimer

`xbe-metal` is an independent, clean-room educational and preservation research project. It does **not** contain any proprietary Microsoft Xbox BIOS, SDK, or copyrighted game code. Users must supply their own legally dumped games. Xbox is a registered trademark of Microsoft Corporation.
