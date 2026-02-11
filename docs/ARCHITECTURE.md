# Architecture

## 1. Overview

The application combines:

- A Qt Quick UI layer (QML)
- A render orchestration item (`RayTracerFboItem`)
- Multiple rendering backends (CPU, OpenGL compute, Vulkan compute, CUDA)

The UI writes rendering parameters into `RayTracerFboItem`, which then selects the active compute path and updates texture content shown in the scene graph.

## 2. Key Modules

### `src/app/main.cpp`

- Parses command-line options (graphics API)
- Configures Qt Quick graphics backend
- Registers `RayTracerFboItem` as a QML type
- Exposes backend switching controller to QML

### `src/app/RayTracerFboItem.*`

Responsibilities:

- Own render state (resolution, samples, max depth, progress)
- Select backend by API + user choice (`computeBackend`)
- Coordinate CPU worker thread path
- Coordinate GPU compute paths
- Upload rendered pixels to a QSG texture node
- Collect and expose runtime stats

### `include/raytracer/RayTracer.h`

- CPU path tracing primitives and algorithms:
  - math types (`Vec3`, `Ray`)
  - scene objects (`Sphere`, `HitableList`, `BVHNode`)
  - materials and camera
  - `ray_color` and `random_scene`

### Backends (`src/backends/*`)

- `GpuPathTracer.*`: OpenGL compute path
- `CudaPathTracer.*` + `CudaPathTracerKernel.cu`: CUDA path
- `vulkan/VulkanPathTracer.*`: Vulkan compute path
- `vulkan/VulkanPathTracerSpv.h`: embedded SPIR-V blob used by Vulkan backend

### Resources (`resources/*`)

- `resources/qml/Main.qml`: control panel and viewport
- `resources/qml/BlurLayer.qml`: optional blur effect layer
- `resources/shaders/pathtrace_vulkan.comp`: Vulkan compute source shader

### Tooling (`tools/*`)

- `tools/spv_to_header.py`: converts `.spv` binary to C++ header array

## 3. Rendering Flow

1. User changes settings in QML.
2. `RayTracerFboItem` receives parameters and starts rendering.
3. Backend selection happens in this order:
   - user selected backend (`auto/opengl/vulkan/cuda/cpu`)
   - runtime API/device capability
   - fallback to CPU when backend init fails
4. Pixel results are uploaded to a scene graph texture.
5. Progress and perf stats are emitted back to QML.

## 4. Backend Selection Rules

- `opengl`: requires Qt Quick OpenGL scene graph API
- `cuda`: requires successful CUDA backend init
- `vulkan`: requires successful Vulkan backend init
- `auto`: prefers OpenGL compute when Qt Quick API is OpenGL, otherwise CPU fallback
- `cpu`: forces CPU path tracing

## 5. Build-Time Architecture

`CMakeLists.txt` defines:

- `raytracer_app` executable for runtime app
- `raytracer_tests` executable for unit tests
- optional CUDA integration via `ENABLE_CUDA`
- optional Vulkan compute integration via `ENABLE_VULKAN_COMPUTE`
- `regen_spv` custom target for shader header regeneration

## 6. Legacy Code

`src/legacy/` keeps earlier QWidget/OpenGL implementation files. They are intentionally not part of active build targets but retained for historical reference and possible migration.
