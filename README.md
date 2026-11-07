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

`OpaquePass` owns the opaque graphics pipeline, pipeline layout, and camera push constant layouts. Its `record` overloads accept a static `MeshGpuData` or a `DynamicMeshGpuData` plus frame slot and camera matrices, select the appropriate vertex buffer, and record dynamic rendering, camera push constants, viewport/scissor setup, mesh buffer binding, and indexed drawing.

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

Minimal `Vec3` and column-major `Mat4` types avoid adding a math dependency. The view matrix is right-handed; the perspective matrix uses negative-Z forward, zero-to-one depth, and inverted Y for Vulkan. CPU storage is `elements[column * 4 + row]`, matching the shader's explicit `column_major` layout, so no transpose is needed.

`Vertex` now contains `float position[3]` and `float color[3]` (24 bytes), with both attributes using `VK_FORMAT_R32G32B32_SFLOAT`. The quad stays at Z=0 and continues deforming in X/Y. Each frame, `RenderingManager::recordFrame` builds identity model, camera view, and projection matrices using the swapchain aspect ratio, rejecting an empty extent. `OpaquePass::recordMesh` pushes them after binding the pipeline and before drawing. The vertex shader computes `projection * view * model * vec4(position, 1)`; fragment color interpolation is unchanged. Navigation now transforms the scene through the shader: W approaches the quad, S retreats, A/D strafe, Q/E move vertically, and mouse look rotates the view.

`CameraPushConstants` contains three consecutive 64-byte matrices (192 bytes). Before creating the pipeline, `OpaquePass` queries the selected physical device's `maxPushConstantsSize`. Devices supporting 192 bytes use all three matrices. Devices supporting only 128 bytes use `CameraViewProjectionPushConstants` (view/projection) and a shader variant with an implicit identity model; smaller limits fail clearly. CMake builds both shader variants from the same source. Compile-time assertions verify CPU sizes and offsets. The tested RTX 3080 Laptop GPU reports a 256-byte limit and selects the 192-byte layout. No descriptors, uniform buffers, depth attachments, or changes to frame synchronization are needed.

## Next Milestone

Add a depth image, device memory, and image view; attach depth during dynamic rendering and enable depth testing/writes in `OpaquePass`. Then render a simple 3D cube and connect real 3D PhysiK surface geometry. Depth, PhysiK, lighting, materials, and textures are not implemented yet.

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
