# Development model

`test.glb` is a self-authored colored cube fixture: 8 shared vertices, 36 uint16
indices, normalized RGBA vertex colors, embedded binary data, and identity node
transform. It contains no third-party model data and no Blender file.

Regenerate it with `py tests/asset_pipeline_test.py --sample` from the repository
root (or use `python` on systems with Python on PATH). Python is only needed to
regenerate the fixture or run the optional tests, not to build or run the engine.

CMake converts this source to `build/assets/models/test.mesh` using GLBConverter.
The runtime loads the generated `.mesh`; it never parses this GLB.
