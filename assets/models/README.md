# Development model

`test.glb` is a self-authored colored cube fixture: 8 shared vertices, 36 uint16
indices, normalized RGBA vertex colors, embedded binary data, and identity node
transform. It includes normalized corner normals (placeholder smooth normals for
shared corners, not hard-edge cube normals). It contains no third-party model data and no Blender file.

Regenerate it with `py tests/asset_pipeline_test.py --sample` from the repository
root (or use `python` on systems with Python on PATH). Python is only needed to
regenerate the fixture or run the optional tests, not to build or run the engine.

`test.mesh` is the committed version-2 conversion of this source using GLBConverter. After
regenerating the GLB, run `build/tools/GLBConverter/Debug/GLBConverter.exe
assets/models/test.glb assets/models/test.mesh` from the repository root.

The root CMake deploys the entire `assets` tree to `Assets` beside VulkanSandbox
on every app build, including `models/test.mesh` and `models/monkey.mesh`.
The runtime loads the deployed `.mesh`; it never parses GLB. Conversion is an
explicit offline operation, not a special-case build step for one model.

`monkey.glb` supplies the Suzanne model used by the renderer. `monkey.mesh` is its
version-2 conversion with normalized imported normals. Regenerate with the same
converter using the monkey input/output paths. Both meshes store POSITION, NORMAL,
and COLOR; older version-1 files must be reconverted.
