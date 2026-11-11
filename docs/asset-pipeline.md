# Offline mesh pipeline

```text
Blender
   |
   v
GLB -> GLBConverter -> .mesh -> MeshLoader -> MeshData
                                             |
                                             v
                                       GraphicManager
                                             |
                                             v
                                        MeshGpuData
                                             |
                                             v
                                         OpaquePass
                                             |
                                             v
                                           Vulkan
```

`GLBConverter` is an independent console target in the Tools solution folder.
It uses vendored [cgltf v1.15](https://github.com/jkuhlmann/cgltf/tree/v1.15), with
the upstream MIT license preserved. Only the tool compiles cgltf. Neither the
runtime loader nor the shared format header includes Vulkan/glTF dependencies.

## Build and convert

Generate the solution normally, then build:

```powershell
cmake --build build --config Debug
.\build\tools\GLBConverter\Debug\GLBConverter.exe input.glb output.mesh
```

Missing arguments print `Usage: GLBConverter <input.glb> <output.mesh>` and return
nonzero. Parse, validation, unsupported content, and file I/O failures also return
nonzero with a diagnostic. Success prints input/output names and vertex, index,
and triangle counts. Parent output directories must already exist.

Conversion is an explicit offline operation. The committed sample
`assets/models/test.mesh` was converted from the self-authored `test.glb` cube.
To update it, run GLBConverter with `assets/models/test.mesh` as the output.
Additional converted models, such as `assets/models/monkey.mesh`, belong in the same
source tree. Builds do not regenerate or overwrite source meshes.

The root `CMakeLists.txt`, after declaring VulkanSandbox, owns the single
`VulkanEngineAssets` deployment target. It copies the entire existing source
`assets` tree into `$<TARGET_FILE_DIR:VulkanSandbox>/Assets` on every app build.
It preserves the existing lowercase `models` subdirectory; no folders are invented.
This always-run dependency also deploys newly added files when the executable does
not relink. The app-local POST_BUILD rule still copies only VulkanRenderDLL.

CMake configures the runtime models path as
`$<TARGET_FILE_DIR:VulkanSandbox>/Assets/models`; renderer source is unchanged and
launching from another working directory works. Paths derive from the target and
project directories, without hardcoded developer or configuration paths. Full
relocatable packaging remains a future step.

To change the default rendered model, convert it to `assets/models/test.mesh`,
build VulkanSandbox to deploy it, then launch. Adding another model to the source
tree deploys it without editing CMake, but does not change the renderer's selection
of `test.mesh`. See [the deployment investigation](asset-deployment.md).

## Supported subset

The tool reads GLB with embedded buffers, appends all indexed triangle primitives
from every mesh definition once, and offsets indices by each appended vertex base.
It preserves stored vertex coordinates; it does not swap axes, normalize the mesh,
or bake node transforms. Scene instances are not duplicated. Empty geometry and
unsupported primitive topologies fail rather than silently disappearing.

Required POSITION is non-sparse, unnormalized float VEC3. Optional COLOR_0 is
non-sparse RGB/RGBA, using floats or normalized uint8/uint16 components. Accessor
offsets, bufferView offsets, and stride are respected through cgltf reader helpers.
RGBA discards alpha. Without COLOR_0, the converter supplies RGB `(0.8, 0.8, 0.8)`.
Indices must be non-sparse scalar uint8, uint16, or uint32 and become uint32 on disk.

Normals, tangents, UVs, additional attributes, materials, textures, scene hierarchy,
node transforms, glTF cameras/lights, and scene instances are ignored with warnings
when encountered. Animations, skinning, morph targets, Draco, meshopt, GPU instancing,
required extensions, external buffers, unsupported accessor formats, and unsupported
topologies are rejected. No material, texture, animation, scene graph, or compression
support is implemented. Required extensions fail even when their optional appearance
data could otherwise be ignored.

## Blender workflow (manual)

1. Create a cube, low-poly sphere, or Suzanne around the origin. Suzanne is the next
   useful manual test after the included cube; it has not been exported/tested here.
2. Apply object transforms. Use one mesh at a size visible from camera `(0,0,3)`.
   Node transforms are ignored by the converter; verify the export's stored geometry.
3. Triangulate for a controlled first test if needed.
4. Export glTF Binary (`.glb`), with geometry and optional vertex colors. Disable
   animation, skinning, morphs, instancing, and compression. Avoid material extensions.
5. Build GLBConverter, convert to `assets/models/test.mesh`, build VulkanSandbox,
   then launch the sandbox.
6. Use WASD/QE and mouse look to inspect the model and depth occlusion. Without
   lighting, an uncolored curved model uses flat neutral color; vertex colors help.

Blender and GLB files are not generated from an automated Blender session.

## Version 1 binary format

Every integer is little-endian uint32. Every float is little-endian IEEE-754 binary32.
Serialization writes fields individually, never native vectors, handles, or structs.

| Offset | Size | Value |
| --- | --- | --- |
| 0 | 4 | Magic bytes `MESH` (`0x4853454d` as little-endian uint32) |
| 4 | 4 | Version `1` |
| 8 | 4 | Vertex count |
| 12 | 4 | Index count |
| 16 | 24 × vertex count | Vertices: XYZ position, then RGB color, six floats |
| 16 + 24 × vertex count | 4 × index count | Triangle uint32 indices |

The Vulkan-independent `engine/MeshFormat/include/MeshFormat.h` defines `Vertex`,
CPU `MeshData`, header constants, validation, and explicit scalar serialization.
Static assertions verify IEEE float and 24-byte Vertex layout (color at offset 12).
Counts must be nonzero, at most 4 million vertices and 12 million indices, with an
index count divisible by three. These caps also ensure counts fit uint32. Positions
and colors must be finite, and indices must be within the vertex array.

`MeshLoader::load(const std::filesystem::path&)` returns CPU `MeshData`. It validates
magic/version, capped counts, and exact expected file size using uint64 arithmetic
before allocation, then reads and validates content. Truncated or trailing data,
non-finite components, and invalid indices fail clearly.

`GraphicManager::createMesh(const MeshData&)` uploads the CPU arrays using the existing
static buffer implementation. `RenderingManager` owns the resulting `MeshGpuData` and
uses the existing static `OpaquePass::record` overload. CPU geometry is released after
upload. The dynamic path remains available for future simulation. Camera, projection,
push constants, depth resources, image barriers, and frame synchronization are unchanged.

## Verification

With Python available when configuring CMake, run:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

`AssetPipeline` generates temporary GLB fixtures with nonzero offsets and stride,
uint8/uint16/uint32 indices, default/RGB/RGBA colors, and multiple primitives. It runs
the actual converter and runtime loader, checks binary counts/content and index rebasing,
and exercises malformed GLBs and corrupt `.mesh` files. Test fixtures use only the
Python standard library and remain under `build/asset-tests`.

The included runtime model has 8 vertices and 36 indices (12 triangles). The known
pre-existing swapchain-acquire synchronization-validation hazard and missing Steam
layer warnings remain documented in the main README; this task does not change barriers.

## Next milestone

Extend CPU MeshData only as necessary for PhysiK, load a suitable surface mesh, create
DynamicMeshGpuData, and initialize PhysiK from CPU geometry. Each simulation step will
copy simulated positions to `vertexBuffer[currentFrame]` before OpaquePass. PhysiK is
not implemented here.
