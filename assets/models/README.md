# Development model

`test.glb` is a self-authored colored cube fixture: 8 shared vertices, 36 uint16
indices, normalized RGBA vertex colors, embedded binary data, and identity node
transform. It contains no third-party model data and no Blender file.

Regenerate it with `py tests/asset_pipeline_test.py --sample` from the repository
root (or use `python` on systems with Python on PATH). Python is only needed to
regenerate the fixture or run the optional tests, not to build or run the engine.

`test.mesh` is the committed conversion of this source using GLBConverter. After
regenerating the GLB, run `build/tools/GLBConverter/Debug/GLBConverter.exe
assets/models/test.glb assets/models/test.mesh` from the repository root.

The root CMake deploys the entire `assets` tree to `Assets` beside VulkanSandbox
on every app build, including `models/test.mesh` and `models/monkey.mesh`.
The runtime loads the deployed `.mesh`; it never parses GLB. Conversion is an
explicit offline operation, not a special-case build step for one model.
