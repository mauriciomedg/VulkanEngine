# SingularityEngineRuntime

The shared `SingularityEngineRuntime` target is the top-level engine orchestrator.
It lives in `engine/SingularityEngineRuntime`, in the Engine solution folder. Its
private dependency is GraphicsEngine; VulkanSandbox links only to the runtime.
The root project remains VulkanEngine; SingularityEngine is the future product name.

## Public API and ownership

Include `<Singularity/SingularityEngineRuntime.h>` and create
`Singularity::SingularityEngineRuntime`. The minimal API is:

```cpp
Singularity::SingularityEngineRuntime engine;
while (!engine.shouldClose())
{
    engine.runFrame();
}
```

The runtime owns a `unique_ptr<VulkanRender::Renderer>`, hidden behind a forward
declaration and out-of-line constructor/destructor. Its header needs no graphics,
GLFW, or Vulkan header. It is non-copyable and exports its API with
`SINGULARITY_ENGINE_RUNTIME_API`, using the existing Windows dllexport/dllimport
convention and private `SINGULARITY_ENGINE_RUNTIME_EXPORTS` definition.

Renderer continues to own Window and RenderingManager. Default window configuration
is 1280 x 720, with base title SingularityEngine. Destruction releases the renderer
and graphics resources before the window through the existing ownership order.
No new configuration system is needed for this milestone.

The existing Renderer public facade now exposes `shouldClose()` and `runFrame()`.
The latter polls events then calls RenderingManager::renderFrame(), exactly as the
old loop body did, and returns its existing frame delta in seconds. Renderer::run() remains available and delegates to these methods.
The runtime controls when a frame runs without accessing RenderingManager, Vulkan
handles, passes, command buffers, or barriers directly.

`runFrame()` intentionally has no dt parameter: GraphicsEngine retains its existing
steady-clock camera/frame timing. That delta now returns through the Renderer facade for runtime performance statistics,
without adding a second clock or changing camera timing. The host creates the runtime, runs the loop, and handles fatal
exceptions. Application source no longer includes or calls GraphicsEngine.

```text
VulkanSandbox
  -> SingularityEngineRuntime::shouldClose / runFrame
  -> GraphicsEngine public Renderer facade
  -> poll window events
  -> RenderingManager::renderFrame
  -> OpaquePass
  -> WireframePass (when enabled)
  -> existing submit/present
```

## Build and deployment

CMake dependency direction is:
`VulkanSandbox -> SingularityEngineRuntime -> GraphicsEngine -> Vulkan/GLFW`.
GraphicsEngine does not include or depend on runtime headers. Both engine libraries
remain SHARED, and their public include directories follow their own targets.

The app POST_BUILD rule copies both SingularityEngineRuntime.dll and GraphicsEngine.dll
beside VulkanSandbox.exe. The remaining GraphicsEngine reference in the app CMake is
for this required DLL deployment; there is no direct app link or C++ dependency.
The centralized VulkanEngineAssets target and its source/destination are unchanged.

## Future coordination (documentation only)

```text
SingularityEngineRuntime
  +-- GraphicsEngine
  +-- PhysiK          [future]
  +-- ScriptEngine    [future]
  +-- AnimationEngine [future]
```

Future physics steps may produce surface positions; the runtime would transfer them
to graphics dynamic-mesh updates before rendering. Future scripts may produce
entity/world state for the runtime. Future animation may produce poses/transforms
that the runtime transfers to graphics. These are intended data flows, not current
APIs or implementations. No placeholder projects, subsystem cross-dependencies,
physics, scripting, animation, ECS, or render graph are introduced.

## Verification

A fresh CMake configuration in `build/singularity-runtime-clean` builds GraphicsEngine,
SingularityEngineRuntime, VulkanSandbox, GLBConverter, and MeshLoaderCheck. The regular
build was also regenerated and built. Both CTest runs pass all 73 asset-pipeline
process checks plus binary assertions; all six compiled shaders pass SPIR-V validation.

PE import inspection confirms VulkanSandbox imports SingularityEngineRuntime.dll,
with no direct GraphicsEngine import; the runtime DLL imports GraphicsEngine.dll.
Both deployed DLLs are present beside the app and deployed assets match their sources.
The fresh sandbox launches and renders Suzanne with Lambert shading, depth, camera
movement, and a working F1 wireframe toggle. Khronos validation is enabled, with only
the existing missing Steam overlay/Fossilize manifest warnings observed. Synchronization
validation was not enabled; its previously documented issue remains unchanged.

All GraphicsEngine implementation files except the public Renderer facade, all shaders,
MeshFormat, GLBConverter, and source assets were compared against the preceding commit
and are unchanged. No runtime headers are included by GraphicsEngine.

## Window-title frame statistics

SingularityEngineRuntime owns the elapsed-time accumulator and completed frame count.
After each successful graphics frame, it samples the existing delta returned by
Renderer::runFrame(). Non-finite, zero, and negative deltas are ignored. Startup
samples cannot trigger a title update until at least 0.5 seconds have accumulated.

At each interval, FPS = frame count / accumulated seconds, and average frame time
in milliseconds = accumulated seconds * 1000 / frame count. The runtime formats
`SingularityEngine | XXX.X FPS | X.XX ms` with fixed precision (one FPS decimal,
two millisecond decimals), then resets both accumulators. String formatting and OS
title updates occur only at this interval, not every frame. The base title is
SingularityEngine until the first sample interval completes.

Renderer::setWindowTitle forwards the string to Window::setTitle, which alone calls
glfwSetWindowTitle. No GLFW handle is exposed to the runtime. The display is enabled
in both Debug and Release, with no framebuffer text, UI pass, or logging per frame.

These values measure CPU-observed completed frame-loop frequency and average frame
time, not GPU execution time. Future Vulkan timestamp queries could provide GPU
frame time and then per-pass GPU timing; neither is implemented here.

Debug and Release builds and asset tests pass in `build/fps-title-check`. An observed
Debug title was `SingularityEngine | 119.8 FPS | 8.35 ms`; repeated window-title
samples held the same text across several frames between interval updates. Camera,
Lambert shading, depth, mesh loading, and the F1 wireframe toggle remain functional.
The Debug validation run reports only the previously documented Steam manifest
warnings, with no rendering validation errors. The regular build's pre-existing
Debug PDB could not be overwritten during verification; the fresh output avoids it.
