# xbe-metal 🍏⚙️

> **Bare-Metal Static Recompiler & Multiplatform Runtime for `.xbe` Binaries**  
> Ahead-of-Time `x86-32` ➔ `C++20` translation targeting **macOS (ARM64)** and **WebAssembly (WebGPU / Canvas)**.

> [!WARNING]
> **Legal & Clean-Room Disclaimer:** `xbe-metal` (*bare-metal* static recompiler) is an independent, clean-room educational and software preservation project. It is **not** affiliated with, endorsed by, or sponsored by Apple Inc. or Microsoft Corporation. This repository contains **zero** proprietary BIOS, SDKs, or copyrighted game code—users must provide their own legally obtained executables. **Apple®** and **macOS®** are registered trademarks of Apple Inc. **Xbox®** is a registered trademark of Microsoft Corporation.

<!-- Row 1: CI, Codespaces, Release & License -->
[![CI](https://github.com/LaAlquimia/xbe-metal/actions/workflows/ci.yml/badge.svg)](https://github.com/LaAlquimia/xbe-metal/actions/workflows/ci.yml)
[![Codespaces](https://img.shields.io/badge/GitHub-Codespaces-181717?logo=github&logoColor=white)](https://codespaces.new/LaAlquimia/xbe-metal)
[![Version](https://img.shields.io/badge/version-0.1.0--alpha-00c853)](https://github.com/LaAlquimia/xbe-metal/releases)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://github.com/LaAlquimia/xbe-metal/pulls)

<!-- Row 2: Languages, Build System & Toolchains -->
[![Standard: C++20](https://img.shields.io/badge/C%2B%2B-20-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/20)
[![CMake: 3.20+](https://img.shields.io/badge/CMake-3.20%2B-064F8C?logo=cmake&logoColor=white)](https://cmake.org/)
[![Compiler: Clang / LLVM](https://img.shields.io/badge/Compiler-Clang%20%2F%20LLVM-262D3A?logo=llvm&logoColor=white)](https://clang.llvm.org/)
[![Toolchain: Emscripten](https://img.shields.io/badge/Toolchain-Emscripten-654FF0?logo=webassembly&logoColor=white)](https://emscripten.org/)

<!-- Row 3: Architectures, Platforms & GPU Backends -->
[![Execution: Bare-Metal AOT](https://img.shields.io/badge/Execution-Bare--Metal%20AOT-107C10)](#-architecture)
[![Guest ISA: x86-32](https://img.shields.io/badge/Guest%20ISA-x86--32-2ea44f)](#-architecture)
[![Host ISA: ARM64](https://img.shields.io/badge/Host%20ISA-ARM64%20(Darwin)-333333?logo=apple&logoColor=white)](#-architecture)
[![GPU: WebGPU / Native](https://img.shields.io/badge/GPU-WebGPU%20%2F%20Native-005A9C?logo=w3c&logoColor=white)](#-architecture)
[![Target: WebAssembly](https://img.shields.io/badge/Target-WebAssembly%20(Wasm32)-654FF0?logo=webassembly&logoColor=white)](#3-building-for-webassembly-webgpu)

---

## 📖 Overview

`xbe-metal` is a high-performance **bare-metal** static recompilation framework and runtime environment designed to take `.xbe` executables and run them directly on modern hardware without traditional virtual machine or emulator overhead.

Instead of interpreting x86 machine code inside an emulator VM at runtime, `xbe-metal`:
1. **Parses & Analyzes** the `.xbe` binary, decrypting Retail/Debug XOR keys and extracting sections, headers, certificates, and kernel import thunks.
2. **Statically Recompiles** `x86-32` machine code ahead-of-time into clean, portable C++20 translation units (`xbe-recomp`).
3. **Executes Bare-Metal on Modern Platforms** with a lightweight high-level emulation (HLE) runtime:
   - **macOS (ARM64):** Native bare-metal ARM64 executable with hardware-accelerated GPU rendering and CoreAudio.
   - **WebAssembly (Wasm):** In-browser execution powered by **WebGPU** / HTML5 Canvas and **WebAudio**.

---

## 🏛️ Architecture

```
                  ┌──────────────────────┐
                  │   Game (default.xbe) │
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
                  │     xbe-recomp       │
                  │   (x86 ➔ C++20 AOT)  │
                  └──────────┬───────────┘
                             │
            ┌────────────────┴────────────────┐
            ▼                                 ▼
   [ Native macOS Build ]            [ WebAssembly Build ]
   - Architecture: ARM64             - Architecture: wasm32 / wasm64
   - Execution: Bare-Metal Native    - Graphics: WebGPU / Canvas
   - Memory: 64MB/128MB Unified      - Memory: Shared / Linear Wasm
   - Compiler: Clang / LLVM          - Compiler: Emscripten (emcc)
```

---

## 🚀 Quick Start (macOS / ARM64)

### 1. Build & Run the Automated Pipeline Test

You don't need any commercial game files to test `xbe-metal`. The repository includes a clean-room synthetic `.xbe` generator and end-to-end test suite:

```bash
# Clone the repository
git clone https://github.com/LaAlquimia/xbe-metal.git
cd xbe-metal

# Configure and compile
cmake -B build -DTARGET_PLATFORM=macOS
cmake --build build -j$(sysctl -n hw.ncpu)

# Run the end-to-end test suite (XBE Parser + x86 Decoder + C++ Emitter + HLE Kernel)
ctest --test-dir build --output-on-failure -V
```

### 2. Inspect & Recompile an `.xbe` Executable

```bash
# Generate a clean-room test XBE binary
./build/test_xbe_pipeline --write-sample build/homebrew_hello.xbe

# 1. Inspect headers, sections, certificate, and kernel imports
./build/xbe-dump --imports build/homebrew_hello.xbe

# 2. Disassemble x86 instructions and emit C++20 source code
./build/xbe-recomp --disasm build/homebrew_hello.xbe -o build/recompiled_entry.cpp

# 3. Launch the native hardware runtime
./build/xbe-runner build/homebrew_hello.xbe
```

### 3. Building for WebAssembly (WebGPU)

```bash
# Activate Emscripten environment
source /path/to/emsdk/emsdk_env.sh

# Configure and compile with Emscripten
emcmake cmake -B build-web -DTARGET_PLATFORM=Wasm
cmake --build build-web

# Or preview the standalone Web Portal immediately:
npx serve web/
# Open http://localhost:3000 and drag & drop any .xbe file
```

---

## 📁 Repository Structure

```
xbe-metal/
├── .github/workflows/ci.yml    # Automated macOS (ARM64) & WebAssembly CI
├── CMakeLists.txt              # Root CMake build configuration
├── LICENSE                     # MIT License
├── README.md                   # Project documentation
├── include/
│   ├── xbe/                    # XBE binary structures and parser interfaces
│   ├── recompiler/             # x86-32 instruction decoder & C++20 AOT emitter
│   └── runtime/
│       ├── core/               # 64MB/128MB unified memory map & kernel HLE
│       └── gpu/                # Cross-platform GPU abstraction (IRenderDevice)
├── src/
│   ├── xbe/                    # XBE parser implementation
│   ├── recompiler/             # Ahead-of-time x86 ➔ C++20 translation engine
│   └── runtime/                # Memory, Kernel HLE & GPU implementation files
├── tools/
│   ├── xbe-dump/               # CLI inspection tool for .xbe files
│   └── xbe-recomp/             # CLI ahead-of-time x86 to C++20 recompiler
├── tests/
│   └── test_xbe_pipeline.cpp   # End-to-end test suite & synthetic XBE builder
└── web/
    ├── index.html              # Standalone browser portal & drag-and-drop analyzer
    ├── shell.html              # Emscripten WebGPU shell template
    └── web_runner.js           # Client-side XBE parser & WebGPU/Canvas bridge
```

---

## ⚖️ Legal Disclaimer

`xbe-metal` is an independent, clean-room educational and preservation research project. The term *"metal"* in `xbe-metal` refers strictly to **bare-metal** ahead-of-time native code execution. This project does **not** contain any proprietary BIOS, SDK, or copyrighted game code. Users must supply their own legally dumped games. **Apple®** and **macOS®** are registered trademarks of Apple Inc. **Xbox®** is a registered trademark of Microsoft Corporation.
