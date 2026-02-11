# Building Guide

## 1. Requirements

### Core

- CMake >= 3.16
- C++17 toolchain
- Qt 6 (Quick + OpenGL modules)
- Python 3 (used by SPIR-V conversion script)

### Optional

- Vulkan SDK (for Vulkan compute backend and `glslangValidator`)
- CUDA Toolkit (when building with `-DENABLE_CUDA=ON`)

## 2. Configure

From repository root:

```bash
cmake -S . -B build
```

Common options:

- `-DENABLE_CUDA=ON` to build CUDA backend
- `-DENABLE_VULKAN_COMPUTE=OFF` to disable Vulkan compute backend

Example:

```bash
cmake -S . -B build -DENABLE_CUDA=ON -DENABLE_VULKAN_COMPUTE=ON
```

## 3. Build

```bash
cmake --build build
```

Main app target: `raytracer_app`

Test target: `raytracer_tests`

## 4. Test

```bash
ctest --test-dir build
```

## 5. Vulkan Shader Header Regeneration

`regen_spv` converts `resources/shaders/pathtrace_vulkan.comp` into
`src/backends/vulkan/VulkanPathTracerSpv.h`.

```bash
cmake --build build --target regen_spv
```

### Shader Tool Discovery

The build attempts to find `glslangValidator` via:

- `$VULKAN_SDK/Bin`
- `$VULKAN_SDK/bin`
- `C:/VulkanSDK/*/Bin`

If not found, `regen_spv` prints a message and does not fail app build.

## 6. Running

```bash
build/raytracer_app.exe
```

With explicit graphics API:

```bash
build/raytracer_app.exe --graphics-api opengl
build/raytracer_app.exe --graphics-api vulkan
build/raytracer_app.exe --graphics-api d3d11
build/raytracer_app.exe --graphics-api software
```

## 7. Troubleshooting

### Missing Qt modules

Symptoms: configure fails in `find_package(Qt6 REQUIRED COMPONENTS Quick OpenGL)`.

Action: ensure Qt installation includes Qt Quick and OpenGL components and that CMake can locate Qt.

### Vulkan backend unavailable at runtime

Symptoms: UI reports fallback to CPU or OpenGL path.

Action: verify Vulkan driver and SDK runtime availability.

### CUDA backend unavailable

Symptoms: UI reports CUDA initialization/render failure.

Action: verify CUDA toolkit, compatible GPU driver, and build with `-DENABLE_CUDA=ON`.
