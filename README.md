# VulkanEngine

A small Windows C++ learning project for building toward Vulkan one milestone at a time.

The offline asset path is now `Blender -> GLB -> GLBConverter -> .mesh -> MeshLoader -> MeshData -> GraphicManager -> MeshGpuData -> OpaquePass -> Vulkan`. See [the asset pipeline guide](docs/asset-pipeline.md) for the binary format, converter scope, tests, and manual Blender/Suzanne workflow.

`SingularityEngineRuntime` is the shared top-level orchestrator. `VulkanSandbox`
uses its public API and links only to the runtime; the runtime privately links to
`GraphicsEngine`. Both DLLs deploy beside the app. The runtime owns the existing
`VulkanRender::Renderer` facade, which retains ownership of the window and graphics
resources. See [runtime architecture](docs/runtime.md) for the API and frame flow.

The dependency direction is `VulkanSandbox -> SingularityEngineRuntime -> GraphicsEngine`.
Future PhysiK, ScriptEngine, and AnimationEngine subsystems will be coordinated by
the runtime. They are documented only and are not implemented or linked today.

```text
engine/
  SingularityEngineRuntime/
  GraphicsEngine/
    VulkanContext, GraphicManager, RenderingManager, OpaquePass, WireframePass
  MeshFormat/        (Vulkan-independent)
  PhysiK/            (future)
  ScriptEngine/      (future)
  AnimationEngine/   (future)
```

## Architecture

```text
VulkanSandbox -> SingularityEngineRuntime -> GraphicsEngine
Renderer (owned by runtime)
    +-- Window
    +-- RenderingManager
            +-- Camera (CPU navigation and view/projection math)
            +-- MeshLoader -> CPU MeshData -> static MeshGpuData
            +-- OpaquePass (Lambert fill pipeline)
            +-- WireframePass (triangle edge overlay pipeline)
            +-- GraphicManager
            |       +-- static and dynamic mesh resources
            |       +-- shared depth image, memory, and view
            |       +-- command buffers / sync / submit
            +-- VulkanContext (device and swapchain state)
```

`VulkanSandbox` only knows about `Singularity::SingularityEngineRuntime`. The runtime uses the public `VulkanRender::Renderer` facade; windowing and Vulkan objects remain owned inside `GraphicsEngine`.

`VulkanContext` owns the instance, validation messenger, surface, physical/logical device, queues, swapchain, images, image views, format, extent, and swapchain image layouts.

`GraphicManager` provides static and dynamic GPU mesh allocation with host-visible, coherent buffer memory. `DynamicMeshGpuData` has one persistently mapped vertex buffer per frame slot and one shared index buffer. `MAX_FRAMES_IN_FLIGHT` controls vertex copies and frame resources. `GraphicManager` also owns command resources, semaphores, fences, submission, and one device-local depth target matching the swapchain extent.

`RenderingManager` owns the shared dynamic rendering scope, color/depth attachment setup, and viewport/scissor. `OpaquePass` and `WireframePass` each own their graphics pipeline and layout, receive the selected depth format during construction, and draw the same buffers. Their static and dynamic mesh overloads bind the pipeline, push camera data, bind vertex/index buffers, and draw indexed geometry. Neither pass owns attachments or synchronization.

`RenderingManager` loads `monkey.mesh` through `MeshLoader`, then uploads CPU `MeshData` through `GraphicManager::createMesh(const MeshData&)`. It owns a static `MeshGpuData` and uses the existing static `OpaquePass::record` overload. The runtime development model is Suzanne, converted offline to mesh v2 with imported normals. A self-authored colored cube fixture is also included. There is no hardcoded runtime cube data or per-frame upload for this static asset. The dynamic mesh infrastructure remains available. Destruction waits for the device to become idle before destroying the mesh; `GraphicManager` also waits before destroying its resources, including when later initialization fails.

## Prerequisites

- Visual Studio 2026 with Desktop development with C++
- CMake, Git, Windows SDK, and Vulkan SDK

CMake fetches GLFW and uses the Vulkan SDK `glslc` to build SPIR-V. The renderer requires Vulkan dynamic rendering. Vendored cgltf v1.15 is compiled only into the independent `GLBConverter` tool. Python is optional for asset-pipeline tests and sample regeneration.

## Generate and Build

Run `generate_vs2026.bat`, then open `build/VulkanEngine.slnx` and build the `VulkanSandbox` startup project. Alternatively, after generation:

```bat
cmake --build build --config Debug
```

## Expected Result

A `1280 x 720` window initially titled `SingularityEngine` displays Suzanne with directional Lambert lighting centered at the origin with camera-controlled perspective. Front surfaces hide rear surfaces, regardless of index order. The model comes from the committed `assets/models/monkey.mesh`, deployed with the entire source asset tree to `Assets` beside VulkanSandbox by the root CMake rule. Models retain their existing lowercase `models` directory. The renderer does not parse GLB. See [asset deployment](docs/asset-deployment.md) for the investigation and exact paths. Imported normals reveal the eye sockets, muzzle, cheeks, and ears using ambient plus diffuse lighting. Dark triangle edges overlay visible surfaces by default; F1 toggles the wireframe. See [wireframe details](docs/wireframe-pass.md). The runtime updates the title about every 0.5 seconds with averaged CPU frame-loop FPS and frame time, for example `SingularityEngine | 144.0 FPS | 6.94 ms`. Closing the window shuts down cleanly.

The loaded static mesh uses one vertex buffer and one index buffer, uploaded once. The dynamic mesh path still supports two persistently mapped vertex buffers and one shared index buffer, with updates after the frame slot is idle. Existing fences, semaphore waits/signals, frame indexing, presentation, and swapchain layout barriers remain unchanged.

Debug builds enable `VK_LAYER_KHRONOS_validation`. For synchronization validation, launch with `VK_LAYER_VALIDATE_SYNC=1` in the process environment. The current swapchain acquire/layout barrier has a pre-existing synchronization-validation `WRITE_AFTER_READ` hazard: the submission waits at `COLOR_ATTACHMENT_OUTPUT`, while the incoming color barrier uses `TOP_OF_PIPE` as its source stage. This milestone preserves that barrier as requested; a follow-up must align the execution dependency. Missing Steam overlay/Fossilize layer manifests may also produce loader warnings on the development machine.

## Camera and GPU Matrices

`RenderingManager` owns a Vulkan-independent `Camera` and a reference to the longer-lived `Window`. Each frame it computes delta time with `std::chrono::steady_clock`, updates camera input, then runs the mesh/render sequence.

The camera starts at `(0, 0, 3)`, yaw `-90` degrees, pitch `0`, looking toward negative Z. Speed is 3 units/second, mouse sensitivity 0.1 degrees per cursor unit, FOV 60 degrees, and near/far planes 0.1/1000. Forward comes from yaw/pitch; right is normalized `cross(forward, worldUp)` and up is normalized `cross(right, forward)`, with world up `(0, 1, 0)`.

W/S approach/retreat, A/D strafe, and Q/E move vertically. Captured mouse movement rotates the view; pitch clamps to +/-89 degrees. The first mouse sample, including after focus returns, initializes cursor history without rotation. F1 toggles the wireframe overlay on supported GPUs; quick taps toggle once and key repeats are ignored. Escape closes the application.

`Vertex` contains `float position[3]`, `float normal[3]`, and `float color[3]` (36 bytes, offsets 0/12/24). Locations 0/1/2 respectively use `VK_FORMAT_R32G32B32_SFLOAT`. `Mat4` stores column-major `elements[column * 4 + row]`, matching GLSL's explicit `column_major` layout. The right-handed projection uses negative-Z forward, Vulkan depth [0, 1], and inverted Y.

`RenderingManager::recordFrame` builds identity model, view, and projection matrices using swapchain aspect ratio, rejecting an empty extent. `OpaquePass::recordMesh` pushes them after pipeline binding and before drawing. The vertex shader computes `projection * view * vec4(position, 1)` with an identity model. World-space normals feed Lambert lighting; the camera does not rotate the light. Existing push constant layouts are preserved.

`CameraPushConstants` contains three 64-byte matrices (192 bytes). `OpaquePass` checks `maxPushConstantsSize` before pipeline creation. Limits below 192 but at least 128 select view/projection only and an implicit identity model in a second shader variant. Smaller limits fail clearly. CMake builds both variants, and compile-time assertions verify CPU sizes/offsets. The tested RTX 3080 Laptop GPU reports 256 bytes and uses the 192-byte layout. No descriptors or uniform buffers are used.

## Depth Buffer

`GraphicManager` tests optimal-tiling depth attachment support in this order: `VK_FORMAT_D32_SFLOAT`, `VK_FORMAT_D32_SFLOAT_S8_UINT`, then `VK_FORMAT_D24_UNORM_S8_UINT`. It fails clearly if none is supported. The development GPU selects `VK_FORMAT_D32_SFLOAT` at 1280 x 720.

The image is 2D, single-sampled, one mip/layer, with `VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT` and `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`. Its view selects the depth aspect. Before first use, `prepareDepthAttachment` transitions from `UNDEFINED` to `DEPTH_STENCIL_ATTACHMENT_OPTIMAL`. This combined layout avoids requiring the optional `separateDepthStencilLayouts` feature and works with all three candidate formats; stencil formats transition both aspects, while stencil testing remains disabled.

Subsequent frames keep the same layout. A depth barrier orders the single shared image's previous writes against the next frame's depth reads/writes at early/late fragment tests. This prevents overlapping frame submissions from racing on the shared target without adding fences or changing the frame model.

The pipeline declares the selected depth attachment format, enables depth test/write, and uses `VK_COMPARE_OP_LESS`. Dynamic rendering clears depth to `1.0`, uses `DONT_CARE` store, and retains the existing color attachment. Cleanup destroys the image view, image, then memory before device destruction. Partial initialization failures also release depth allocations.

```text
GLB -> GLBConverter -> .mesh -> MeshLoader -> MeshData
                                                |
                                                v
                        GraphicManager -> static vertex/index buffers
                                                |
                                                v
Camera -> view/projection -----------------> OpaquePass
                                                +-- swapchain color attachment
                                                +-- shared depth attachment
                                                |
                                                v
                                         vkCmdDrawIndexed
                                                |
                                                v
                                         depth-tested 3D image
                                                |
                                                v
                                              present
```

Resize/recreation is not implemented. Future resizing must recreate the swapchain, swapchain views, depth image/memory/view, and possibly the pipeline if attachment formats change.

## Next Milestone

Load a suitable surface mesh into CPU `MeshData`, extend it only if necessary for PhysiK, and create `DynamicMeshGpuData`. Initialize PhysiK from CPU geometry, step the simulation using delta time, and update the current frame's positions before `OpaquePass::record` for live soft-body rendering. PhysiK, model/normal matrices, materials, textures, and further passes beyond opaque/wireframe are not implemented. See [asset pipeline lighting](docs/asset-pipeline.md#directional-lambert-lighting) for the current normals and lighting conventions.
