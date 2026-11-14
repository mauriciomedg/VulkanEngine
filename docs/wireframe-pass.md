# Wireframe overlay

`RenderingManager` owns one dynamic rendering scope for the opaque and wireframe
passes. Color and depth clear once, at the start of that scope. OpaquePass retains
its Lambert shaders, FILL rasterization, existing culling, LESS depth test, and depth
writes. WireframePass overlays the same `MeshGpuData` buffers and index count;
there is one vertex upload and one index upload, not a second mesh allocation.

## Device support

`VulkanContext::pickPhysicalDevice` queries `VkPhysicalDeviceFeatures` and logs
`fillModeNonSolid` for each GPU. The selected device's optional support is recorded.
`createLogicalDevice` enables `deviceFeatures.fillModeNonSolid` through
`VkDeviceCreateInfo::pEnabledFeatures` only when supported. `wideLines` stays off.
Without support, selection still succeeds, a warning explains that WireframePass
is disabled, no LINE pipeline is created, and its record calls are no-ops.

## Pipeline

| Setting | WireframePass |
| --- | --- |
| Topology | TRIANGLE_LIST; native polygon lines, including triangulation diagonals |
| Polygon mode | LINE |
| Line width | 1.0 |
| Culling | NONE |
| Depth test | ON |
| Depth write | OFF |
| Depth compare | LESS_OR_EQUAL |
| Fragment RGBA | (0.05, 0.05, 0.05, 1.0), independent of vertex color |
| Blending / multisampling | Disabled / one sample |
| Depth bias | Constant -1.0, slope -1.0, clamp 0.0 |

The initial unbiased overlay showed broken line coverage from equal-depth precision.
A conservative negative bias is applied only to the wireframe pipeline, moving its
fragment depth slightly toward the camera. The constructor exposes constant/slope
factors for tuning; there is no added dynamic state or vertex-position adjustment.
OpaquePass depth values and depth synchronization are unchanged.

Both pipelines declare the same swapchain color and selected depth formats via
`VkPipelineRenderingCreateInfo`. RenderingManager sets dynamic viewport/scissor once.
`wireframe.vert` reads only position at location 0, using `sizeof(Vertex)` stride and
`offsetof(Vertex, position)`. It applies the same identity-model view/projection
calculation as OpaquePass. `wireframe.frag` writes the fixed color. CMake builds both
192-byte and 128-byte camera variants using the existing shader build process.

The existing camera structures and static layout assertions moved from OpaquePass.h
to CameraPushConstants.h so both passes share their ABI. The legacy model slot in
the 192-byte layout remains unused; no model or normal transform is introduced.

## Toggle and ownership

`m_wireframeEnabled` starts true. F1 toggles it with a GLFW key-press callback;
repeats are ignored. RenderingManager registers the callback and its window user
pointer after successful initialization and removes them on destruction. Both
passes also accept the existing DynamicMeshGpuData/current-frame vertex buffer
without defining a wireframe-specific mesh abstraction.

```text
RenderingManager
  -> transition swapchain image
  -> prepare existing depth attachment
  -> vkCmdBeginRendering (color clear, depth clear)
  -> set viewport/scissor
  -> OpaquePass: FILL, depth write ON
  -> WireframePass if enabled/supported: LINE, depth write OFF
  -> vkCmdEndRendering
  -> transition to PRESENT
  -> submit
  -> present
```

The asset pipeline, mesh format/version, deployment, frame slot logic, acquire,
submit/present, fences, semaphores, swapchain transitions, and depth barriers are
unchanged. There is no edge extraction, geometry shader, descriptor-based color,
PhysiK, or pass scheduler.

## Verification

- Debug builds of VulkanRenderDLL, VulkanSandbox, GLBConverter, and MeshLoaderCheck pass.
- CTest AssetPipeline passes all 73 process checks plus binary assertions, unchanged.
- Both wireframe vertex variants and the fragment SPIR-V pass `spirv-val`.
- RTX 3080 Laptop GPU reports fillModeNonSolid support; the device log confirms it
  is enabled. Only one vertex upload and one index upload occur.
- Visual checks show shaded Suzanne plus triangle edges, with no obvious back-side
  bleed-through during inspected camera rotation. F1 switches OFF and ON correctly;
  Lambert lighting, camera movement, and depth occlusion remain intact.
- Khronos validation is enabled; no rendering validation errors occur. The only
  observed messages are the existing missing Steam overlay/Fossilize manifests.
- Synchronization validation is not enabled for this run. The known prior hazard
  documented in README remains outside this change. Frame submission/presentation,
  swapchain transition implementation, and opaque pipeline creation were also
  compared directly with the preceding commit and are unchanged.
- The unsupported-feature fallback and 128-byte camera layout were reviewed/compiled;
  this GPU exercises the supported feature and 192-byte layout at runtime.

Added files: WireframePass.h/.cpp, CameraPushConstants.h, wireframe.vert/.frag,
and this guide. Modified files: OpaquePass.h/.cpp, RenderPass.h,
RenderingManager.h/.cpp, VulkanContext.h/.cpp, the renderer CMakeLists.txt, and
README.md. Asset files, converter, mesh format, loader, and deployment are untouched.
