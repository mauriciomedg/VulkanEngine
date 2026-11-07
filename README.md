# VulkanEngine

A small Windows C++ learning project for building toward Vulkan one milestone at a time.

## Architecture

```text
Renderer
    |
    +-- RenderingManager
            |
            +-- Camera (CPU navigation and view/projection math)
            +-- test DynamicMeshGpuData
            +-- OpaquePass
            +-- GraphicManager
            |       +-- static and dynamic mesh creation / destruction
            |       +-- command buffers / sync / submit
            +-- VulkanContext
```

`VulkanSandbox` only knows about the public `VulkanRender::Renderer` API. Windowing and Vulkan objects are owned by `VulkanRenderDLL` behind that same boundary.

`VulkanContext` owns low-level Vulkan environment state: instance, validation, surface, physical/logical device, queues, swapchain, images, image views, format, extent, and swapchain image layouts.

`GraphicManager` provides generic static and dynamic GPU mesh creation and destruction using host-visible, coherent memory and the shared buffer allocation helpers. `DynamicMeshGpuData` has one independent, persistently mapped vertex buffer per frame slot and one shared static index buffer. The common `MAX_FRAMES_IN_FLIGHT` constant in `MeshData.h` controls both vertex copies and frame resources. `GraphicManager` also owns the command pool, command buffers, frame semaphores, fences, and graphics queue submission resources.

`OpaquePass` owns the opaque graphics pipeline and pipeline layout. Its `record` overloads accept a static `MeshGpuData` or a `DynamicMeshGpuData` plus frame slot, select the appropriate vertex buffer, and record dynamic rendering, viewport/scissor setup, mesh buffer binding, and indexed drawing.

`RenderingManager.cpp` contains the temporary CPU quad data (4 vertices and 6 uint32 indices). `RenderingManager` owns the dynamic `m_testMesh`, creates it after the graphics resources and pass are constructed, and destroys it after waiting for the device to become idle, before member teardown. After waiting for the current frame fence and acquiring an image, it deforms a fresh copy of the original CPU positions using elapsed time and copies it into only the current slot. Colors and the six shared indices stay unchanged. It also owns the frame sequence: acquire swapchain image, update current-slot vertices, reset frame resources, transition image layouts, execute `OpaquePass`, submit, present, and advance the frame.

```text
temporary CPU quad
        |
        v
GraphicManager::createDynamicMesh
        |
        v
DynamicMeshGpuData (2 vertex buffers + 1 index buffer)
        |
        v
wait slot fence -> update mapped vertices -> OpaquePass::record(slot)
        |
        v
vkCmdDrawIndexed
```

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

The renderer clears the swapchain image and draws one continuously deforming, indexed, interpolated-color quad using a Vulkan graphics pipeline, two vertex buffers, and one shared index buffer. Each vertex allocation is mapped once during creation and unmapped once during destruction; frame updates only copy into mapped memory after that slot is idle. The index allocation is uploaded and unmapped once at creation. Closing the window shuts the application down cleanly.

In a Debug build, startup also initializes the Vulkan instance, enables `VK_LAYER_KHRONOS_validation`, creates a `VkDebugUtilsMessengerEXT`, verifies and enables dynamic rendering, and creates the Vulkan surface, swapchain, command resources, synchronization objects, graphics pipeline, and dynamic quad buffers.

## Camera Navigation

`RenderingManager` owns a Vulkan-independent `Camera` and a reference to the longer-lived `Window`. Each frame it computes delta time with `std::chrono::steady_clock`, updates camera input, and then runs the existing dynamic mesh/render sequence.

The camera stores position, yaw/pitch in degrees, movement speed (3 units/second), mouse sensitivity (0.1 degrees per cursor unit), FOV (60 degrees), near/far planes (0.1/1000), orientation vectors, and previous mouse coordinates. It starts at `(0, 0, 3)` with yaw `-90` and pitch `0`, looking toward negative Z. Forward comes from yaw/pitch; right is normalized `cross(forward, worldUp)` and up is normalized `cross(right, forward)`, with world up `(0, 1, 0)`.

W/S move forward/back, A/D strafe, and Q/E move down/up along world up. Movement uses delta time. The cursor is captured while focused; mouse movement changes yaw/pitch, with pitch clamped to +/-89 degrees. The first mouse sample, including after focus returns, initializes cursor history without rotating. Escape closes the application.

Minimal `Vec3` and column-major `Mat4` types avoid adding a math dependency. The view matrix is right-handed; the perspective matrix uses negative-Z forward, zero-to-one depth, and inverted Y for Vulkan. Camera matrices remain CPU-side for this milestone, so the animated quad's rendering is unchanged. No camera GPU buffers or shader changes are included.

## Next Milestone

Change vertex positions from 2D to 3D, then upload model/view/projection matrices to the vertex shader using push constants or a uniform buffer. After that, add a depth buffer, render a 3D mesh, and connect PhysiK surface positions. These steps, lighting, and materials are not implemented yet.

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
