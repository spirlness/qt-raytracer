# Qt Ray Tracer

A Qt 6 based ray tracing demo with a QML control panel, CPU path tracing, and optional GPU compute backends (OpenGL, Vulkan, CUDA).

This repository has been reorganized to follow Google-style C++ project layout conventions:

- `include/` for public headers
- `src/` for implementation and private module headers
- `tests/` for automated tests
- `resources/` for QML and shader assets
- `tools/` for project scripts

## Features

- Qt Quick viewport using scene graph texture updates
- CPU path tracing with tiled rendering and progress reporting
- Optional GPU compute backends:
  - OpenGL compute shader
  - Vulkan compute pipeline
  - CUDA kernel backend
- Runtime backend controls from QML UI
- Unit tests using GoogleTest

## Repository Layout

```text
include/
  raytracer/
    RayTracer.h
src/
  app/
    main.cpp
    RayTracerFboItem.cpp
    RayTracerFboItem.h
  backends/
    CudaPathTracer.cpp
    CudaPathTracer.h
    CudaPathTracerKernel.cu
    GpuPathTracer.cpp
    GpuPathTracer.h
    vulkan/
      VulkanPathTracer.cpp
      VulkanPathTracer.h
      VulkanPathTracerSpv.h
  legacy/
    MainWindow.cpp
    MainWindow.h
    RenderWidget.cpp
    RenderWidget.h
resources/
  qml/
    Main.qml
    BlurLayer.qml
  shaders/
    pathtrace_vulkan.comp
tests/
  unit/
tools/
  spv_to_header.py
docs/
  BUILDING.md
  ARCHITECTURE.md
  DEVELOPMENT.md
```

## Build Quick Start

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

### Optional Build Flags

- Enable CUDA backend:

```bash
cmake -S . -B build -DENABLE_CUDA=ON
```

- Enable/disable Vulkan compute backend:

```bash
cmake -S . -B build -DENABLE_VULKAN_COMPUTE=ON
```

## Run

On Windows, run:

```bash
build/raytracer_app.exe
```

You can also choose the Qt Quick graphics API at startup:

```bash
build/raytracer_app.exe --graphics-api opengl
build/raytracer_app.exe --graphics-api vulkan
```

## Vulkan Shader Regeneration

The Vulkan compute shader source is stored at `resources/shaders/pathtrace_vulkan.comp`.
Regenerate the embedded SPIR-V header with:

```bash
cmake --build build --target regen_spv
```

## Documentation

- Build and toolchain details: `docs/BUILDING.md`
- Runtime and module design: `docs/ARCHITECTURE.md`
- Coding workflow and conventions: `docs/DEVELOPMENT.md`
