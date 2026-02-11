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

- `-DBUILD_APP=OFF` to skip Qt app target
- `-DBUILD_TESTS=ON` to build unit tests
- `-DENABLE_CUDA=ON` to build CUDA backend
- `-DENABLE_VULKAN_COMPUTE=OFF` to disable Vulkan compute backend

Example:

```bash
cmake -S . -B build -DBUILD_APP=ON -DBUILD_TESTS=ON -DENABLE_CUDA=ON -DENABLE_VULKAN_COMPUTE=ON
```

CI-oriented tests-only configure:

```bash
cmake -S . -B build -DBUILD_APP=OFF -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
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

### Coverage (Linux, lcov)

```bash
cmake -S . -B build-coverage -DBUILD_APP=OFF -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_FLAGS="--coverage -O0 -g" \
  -DCMAKE_CXX_FLAGS="--coverage -O0 -g" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage"
cmake --build build-coverage --target raytracer_tests
ctest --test-dir build-coverage --output-on-failure
lcov --capture --directory build-coverage --output-file coverage.info
lcov --remove coverage.info '/usr/*' '*/_deps/*' '*/tests/*' '*/build-coverage/*' --output-file coverage.filtered.info
genhtml coverage.filtered.info --output-directory coverage-html
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

If you only need unit tests in CI, configure with `-DBUILD_APP=OFF` to avoid Qt dependency.

### Vulkan backend unavailable at runtime

Symptoms: UI reports fallback to CPU or OpenGL path.

Action: verify Vulkan driver and SDK runtime availability.

### CUDA backend unavailable

Symptoms: UI reports CUDA initialization/render failure.

Action: verify CUDA toolkit, compatible GPU driver, and build with `-DENABLE_CUDA=ON`.
