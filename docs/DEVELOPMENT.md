# Development Guide

## 1. Project Conventions

This repository follows a Google-style C++ project layout:

- `include/` contains public headers
- `src/` contains implementation and internal headers
- `tests/` contains test code grouped by scope
- `resources/` contains runtime assets (QML, shaders)
- `tools/` contains scripts/utilities

## 2. Include Rules

Use path-based includes from module roots:

- Public API header:

```cpp
#include "raytracer/RayTracer.h"
```

- App internal header:

```cpp
#include "app/RayTracerFboItem.h"
```

- Backend internal header:

```cpp
#include "backends/GpuPathTracer.h"
#include "backends/vulkan/VulkanPathTracer.h"
```

Test helper include:

```cpp
#include "unit/TestHelpers.h"
```

## 3. Formatting and Style

- `.clang-format` is based on Google C++ style.
- Keep C++ standard at C++17.
- Prefer clear module boundaries over cross-module include shortcuts.

## 4. Typical Local Workflow

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

CI-equivalent tests-only workflow:

```bash
cmake -S . -B build-ci -DBUILD_APP=OFF -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-ci --config Release --target raytracer_tests
ctest --test-dir build-ci -C Release --output-on-failure
```

When changing Vulkan shader source:

```bash
cmake --build build --target regen_spv
cmake --build build
```

## 5. Adding New Code

### New app/UI feature

1. Add/modify code in `src/app/`
2. Update QML in `resources/qml/`
3. Register/bridge C++ objects in `src/app/main.cpp` if required
4. Add tests in `tests/unit/` where possible

### New backend

1. Add implementation in `src/backends/` (or subdirectory)
2. Wire selection logic in `src/app/RayTracerFboItem.cpp`
3. Add CMake options and sources in `CMakeLists.txt`
4. Add runtime fallback behavior and error messages

## 6. Testing Scope

Current tests focus on CPU ray tracing math/object logic in `tests/unit/`.

Expanded coverage includes:

- math utilities and random sampling constraints
- `AABB` and `surrounding_box` behavior
- `Sphere` and `HitableList` bounding-box behavior
- `BVHNode` hit and bounding-box behavior
- camera ray generation and aperture offset constraints
- material scatter invariants for Lambertian/Metal/Dielectric

Recommended additions:

- backend initialization tests (where feasible)
- parameter validation tests for render settings
- regression tests for scene construction and deterministic math paths

## 7. Legacy Module Policy

`src/legacy/` is excluded from current targets. Keep it isolated:

- do not add new dependencies from active modules into legacy code
- if reusing code, migrate it explicitly into `src/app` or `src/backends`

## 8. GitHub Workflow

Workflow files:

- `.github/workflows/ci.yml`
  - builds and runs `raytracer_tests` on Ubuntu and Windows
  - uses `BUILD_APP=OFF` to decouple unit tests from Qt runtime packaging
- `.github/workflows/coverage.yml`
  - runs instrumented unit tests on Ubuntu
  - generates `lcov` summary + HTML report
  - uploads coverage artifacts for inspection
