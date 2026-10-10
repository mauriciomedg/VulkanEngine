# VulkanEngine

A small Windows C++ learning project for building toward Vulkan one milestone at a time.

## Architecture

```text
VulkanSandbox.exe
        |
        v
VulkanRenderDLL.dll
        |
        +-- Window
        |
        +-- Renderer
        |
        +-- RenderingManager
        |
        +-- OpaquePass
        |
        +-- GraphicsManager
        |
        +-- VulkanContext
        |
        +-- GLFW + Vulkan
```

`VulkanSandbox` only knows about the public `VulkanRender::Renderer` API. Windowing and Vulkan objects are owned by `VulkanRenderDLL` behind that same boundary.

`VulkanContext` owns low-level Vulkan environment state: instance, validation, surface, physical/logical device, queues, swapchain, images, image views, format, extent, and swapchain image layouts.

`GraphicsManager` owns GPU communication resources: the static quad vertex/index buffers and memory, command pool, command buffers, frame semaphores, fences, memory type selection, buffer creation, and graphics queue submission.

`OpaquePass` owns the opaque graphics pipeline and pipeline layout, then records dynamic rendering, viewport/scissor setup, mesh buffer binding, and indexed drawing.

`RenderingManager` owns the frame sequence: acquire swapchain image, reset frame resources, transition image layouts, execute `OpaquePass`, submit, present, and advance the frame.

## Prerequisites

- Visual Studio 2026
- Desktop development with C++
- CMake
- Git, used by CMake `FetchContent` to obtain GLFW
- Windows SDK
- Vulkan SDK

This project uses Vulkan dynamic rendering for the first graphics pipeline milestone, so the Vulkan SDK must be installed and discoverable by CMake. The build also uses the Vulkan SDK `glslc` compiler to generate SPIR-V shaders.

## Generate Project

From any directory, run:

```bat
generate_vs2026.bat
```

The Visual Studio solution and all generated CMake files are written under:

```text
build/
```

## Build

Open the generated solution in `build/` and build:

```text
VulkanSandbox
```

`VulkanSandbox` is configured as the Visual Studio startup project where supported.

## Expected Result

Running the application opens a `1280 x 720` window named:

```text
Vulkan Mini Engine
```

The renderer clears the swapchain image and draws one indexed, interpolated-color quad using a Vulkan graphics pipeline, vertex buffer, and index buffer. Closing the window shuts the application down cleanly.

In a Debug build, startup also initializes the Vulkan instance, enables `VK_LAYER_KHRONOS_validation`, creates a `VkDebugUtilsMessengerEXT`, verifies and enables dynamic rendering, and creates the Vulkan surface, swapchain, command resources, synchronization objects, graphics pipeline, and static quad buffers.

## Next Milestone

The next Vulkan milestone should introduce safe dynamic vertex updates while leaving the `VulkanSandbox` public-facing code unchanged:

```text
Keep vertex memory persistently mapped
        |
        v
update vertex positions every frame
        |
        v
use per-frame vertex storage or safe synchronization
        |
        v
render dynamic deforming geometry
```

After that, replace the synthetic vertex updates with real PhysiK surface positions and display the live soft-body simulation.

The intended future pass order is:

```text
ShadowPass
        |
        v
OpaquePass
        |
        v
OverlayPass
```
