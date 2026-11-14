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
not relink. The app-local POST_BUILD rule still copies only GraphicsEngine.

CMake configures the runtime models path as
`$<TARGET_FILE_DIR:VulkanSandbox>/Assets/models`; renderer source is unchanged and
launching from another working directory works. Paths derive from the target and
project directories, without hardcoded developer or configuration paths. Full
relocatable packaging remains a future step.

The renderer currently selects `monkey.mesh`. To replace that model, convert to
`assets/models/monkey.mesh`,
build VulkanSandbox to deploy it, then launch. Adding another model to the source
tree deploys it without editing CMake, but does not change the renderer's selection
of `monkey.mesh`. See [the deployment investigation](asset-deployment.md).

## Supported subset

The tool reads GLB with embedded buffers, appends all indexed triangle primitives
from every mesh definition once, and offsets indices by each appended vertex base.
It preserves stored vertex coordinates; it does not swap axes, normalize the mesh,
or bake node transforms. Scene instances are not duplicated. Empty geometry and
unsupported primitive topologies fail rather than silently disappearing.

Required POSITION and NORMAL are non-sparse, unnormalized float VEC3 with buffer
views and matching accessor counts. Normals must be finite and have length greater
than `1e-8`; the converter normalizes them before storing them. Optional COLOR_0 is
non-sparse RGB/RGBA, using floats or normalized uint8/uint16 components. Accessor
offsets, bufferView offsets, and stride are respected through cgltf reader helpers.
RGBA discards alpha. Without COLOR_0, the converter supplies RGB `(0.8, 0.8, 0.8)`.
Indices must be non-sparse scalar uint8, uint16, or uint32 and become uint32 on disk.

Tangents, UVs, additional attributes, materials, textures, scene hierarchy,
node transforms, glTF cameras/lights, and scene instances are ignored with warnings
when encountered. Animations, skinning, morph targets, Draco, meshopt, GPU instancing,
required extensions, external buffers, unsupported accessor formats, and unsupported
topologies are rejected. No material, texture, animation, scene graph, or compression
support is implemented. Required extensions fail even when their optional appearance
data could otherwise be ignored.

## Blender workflow (manual)

1. Create a cube, low-poly sphere, or Suzanne around the origin. The current runtime
   model is Suzanne (`monkey.mesh`).
2. Apply object transforms. Use one mesh at a size visible from camera `(0,0,3)`.
   Node transforms are ignored by the converter; verify the export's stored geometry.
3. Triangulate for a controlled first test if needed.
4. Export glTF Binary (`.glb`), with geometry, normals, and optional vertex colors. Disable
   animation, skinning, morphs, instancing, and compression. Avoid material extensions.
5. Build GLBConverter, convert to `assets/models/monkey.mesh`, build VulkanSandbox,
   then launch the sandbox.
6. Use WASD/QE and mouse look to inspect the lighting and depth occlusion.
   Lighting must remain fixed to the object/world as the camera moves.

Blender and GLB files are not generated from an automated Blender session.

## Version 2 binary format

Every integer is little-endian uint32. Every float is little-endian IEEE-754 binary32.
Serialization writes fields individually, never native vectors, handles, or structs.

| Offset | Size | Value |
| --- | --- | --- |
| 0 | 4 | Magic bytes `MESH` (`0x4853454d` as little-endian uint32) |
| 4 | 4 | Version `2` |
| 8 | 4 | Vertex count |
| 12 | 4 | Index count |
| 16 | 36 × vertex count | Vertices: XYZ position, XYZ normal, then RGB color, nine floats |
| 16 + 36 × vertex count | 4 × index count | Triangle uint32 indices |

The Vulkan-independent `engine/MeshFormat/include/MeshFormat.h` defines `Vertex`,
CPU `MeshData`, header constants, validation, and explicit scalar serialization.
Static assertions verify IEEE float and 36-byte Vertex layout (position at 0, normal at 12, color at 24).
Counts must be nonzero, at most 4 million vertices and 12 million indices, with an
index count divisible by three. These caps also ensure counts fit uint32. Positions,
normals, and colors must be finite, and indices must be within the vertex array.

`MeshLoader::load(const std::filesystem::path&)` returns CPU `MeshData`. It validates
magic/version, capped counts, and exact expected file size using uint64 arithmetic
before allocation, then reads and validates content. Version-1 files fail with
`Unsupported mesh version: 1, expected 2` and must be regenerated with GLBConverter. Truncated or trailing data,
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
and verifies normals survive conversion and runtime loading. Missing/malformed NORMAL,
count mismatches, zero/tiny/non-finite normals, v1 rejection, non-finite stored normals,
and corrupt `.mesh` files are covered. Test fixtures use only the
Python standard library and remain under `build/asset-tests`.

Suzanne has 1,966 vertices and 2,904 indices (968 triangles). The test cube has
8 vertices and 36 indices; its shared corners use simple normalized corner normals
rather than hard-edge face normals. Both committed meshes use format version 2. The known
pre-existing swapchain-acquire synchronization-validation hazard and missing Steam
layer warnings remain documented in the main README; this task does not change barriers.

Debug verification for this milestone: GLBConverter, GraphicsEngine, VulkanSandbox,
and MeshLoaderCheck build; CTest passes 73 process checks plus binary assertions.
Both committed meshes load through MeshLoader. Suzanne renders with readable facial
shading and depth occlusion; camera rotation preserves the surface lighting.
The debug run enables Khronos validation and reports no rendering validation errors.
The only observed loader messages are the two missing Steam layer manifests noted
above. Synchronization validation was not enabled for this run; its documented
pre-existing hazard is not changed by this milestone.

## Directional Lambert lighting

The old vertex contained POSITION + COLOR (24 bytes); v2 contains POSITION + NORMAL +
COLOR (36 bytes). Vulkan uses one binding with `sizeof(Vertex)` stride and
`offsetof` offsets: location 0 position, 1 normal, 2 color, all float VEC3.

The vertex shader computes `projection * view * vec4(position, 1)` and passes the
normalized imported normal unchanged in world space. The existing camera push
constant layouts are preserved. The model transform is identity; normals are not
transformed by the view matrix. Normal-matrix handling will be added when per-object
transforms are introduced, especially for non-uniform scaling; it is not implemented.
Node transforms are ignored; apply transforms in Blender.

The fragment shader normalizes the interpolated normal `N`. The hardcoded world-space
`lightDirection = normalize(vec3(-0.5, -1.0, -0.3))` is the direction rays travel.
Diffuse is `max(dot(N, -lightDirection), 0.0)`; ambient is `0.15`. Final RGB is
`fragColor * (ambient + diffuse)` with alpha 1. No light descriptors, materials,
textures, specular, shadows, or additional lights are used.

```text
Blender NORMAL -> GLB -> GLBConverter -> Vertex.normal -> .mesh v2
 -> MeshLoader -> MeshData -> GraphicManager -> VkBuffer
 -> Vertex Shader -> Fragment Shader -> Lambert lighting
```
