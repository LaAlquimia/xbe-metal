cmake_minimum_required(VERSION 3.20)
project(xbe-metal VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Option for Target Platform: macOS or Wasm
if(EMSCRIPTEN)
    set(DEFAULT_PLATFORM "Wasm")
elseif(APPLE)
    set(DEFAULT_PLATFORM "macOS")
else()
    set(DEFAULT_PLATFORM "Generic")
endif()

set(TARGET_PLATFORM "${DEFAULT_PLATFORM}" CACHE STRING "Target execution platform: macOS or Wasm")
message(STATUS ">> Target Platform: ${TARGET_PLATFORM}")

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Common Compiler Warnings & Flags
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|AppleClang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
endif()

# Core static library: XBE Parser
add_library(xbe_core STATIC
    src/xbe/xbe_parser.cpp
    src/runtime/core/memory.cpp
    src/runtime/core/kernel.cpp
)

# Platform-specific graphics & runtime
if(TARGET_PLATFORM STREQUAL "macOS")
    enable_language(OBJCXX)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit REQUIRED)
    find_library(COCOA_FRAMEWORK Cocoa REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore REQUIRED)

    add_library(xbe_runtime STATIC
        src/runtime/gpu/metal/metal_device.mm
        src/runtime/gpu/render_device.cpp
    )
    target_link_libraries(xbe_runtime PUBLIC
        xbe_core
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
    )
elseif(TARGET_PLATFORM STREQUAL "Wasm")
    message(STATUS "Configuring for WebAssembly with Emscripten")
    add_definitions(-DXBE_TARGET_WASM)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_WEBGPU=1 -s ALLOW_MEMORY_GROWTH=1")

    add_library(xbe_runtime STATIC
        src/runtime/gpu/webgpu/webgpu_device.cpp
        src/runtime/gpu/render_device.cpp
    )
    target_link_libraries(xbe_runtime PUBLIC xbe_core)
endif()

# CLI Tool: xbe-dump
add_executable(xbe-dump
    tools/xbe-dump/main.cpp
)
target_link_libraries(xbe-dump PRIVATE xbe_core)

# Target: xbe-runner (Main application launcher)
if(TARGET_PLATFORM STREQUAL "macOS")
    add_executable(xbe-runner
        src/main.cpp
    )
    target_link_libraries(xbe-runner PRIVATE xbe_runtime)
elseif(TARGET_PLATFORM STREQUAL "Wasm")
    add_executable(xbe-runner
        src/main.cpp
    )
    target_link_libraries(xbe-runner PRIVATE xbe_runtime)
    set_target_properties(xbe-runner PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS "-s WASM=1 -s USE_WEBGPU=1 -s EXPORTED_RUNTIME_METHODS=['ccall','cwrap'] --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    )
endif()
