# Runtime asset deployment investigation

## Previous mechanism

At parent commit `5c2ddd3`, no command copied `test.mesh` into VulkanSandbox's
output directory. Repository CMake/scripts and generated Visual Studio projects
were searched for the filename, directory/file copy commands, configure-time copy,
custom commands, build events, and deployment/output properties.

`tools/GLBConverter/CMakeLists.txt:5-17` owned the special-case generator:

```cmake
set(VULKAN_ENGINE_ASSET_DIR "${PROJECT_BINARY_DIR}/assets/models" PARENT_SCOPE)
add_custom_command(
    OUTPUT "${PROJECT_BINARY_DIR}/assets/models/test.mesh"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/assets/models"
    COMMAND $<TARGET_FILE:GLBConverter> "${PROJECT_SOURCE_DIR}/assets/models/test.glb"
        "${PROJECT_BINARY_DIR}/assets/models/test.mesh"
    DEPENDS GLBConverter "${PROJECT_SOURCE_DIR}/assets/models/test.glb"
    VERBATIM
    COMMENT "Converting sample GLB to engine mesh"
)
add_custom_target(VulkanEngineAssets DEPENDS "${PROJECT_BINARY_DIR}/assets/models/test.mesh")
set_target_properties(VulkanEngineAssets PROPERTIES FOLDER "Tools")
```

The source was `assets/models/test.glb`; the output was
`${PROJECT_BINARY_DIR}/assets/models/test.mesh`, not an executable-relative asset
folder. `engine/GraphicsEngine/CMakeLists.txt` declared the DLL's dependency on
`VulkanEngineAssets` and supplied `VULKAN_RENDER_ASSET_DIR` from that generated
directory. Building VulkanSandbox built its linked DLL, which built the asset
target. The runtime loaded this file directly from the build tree. Generated
`build/tools/GLBConverter/VulkanEngineAssets.vcxproj` confirmed the converter
invocation and output; the sandbox project contained only the DLL copy command.

## Replacement

The tool-local generator, exported generated path, and DLL-to-asset dependency
were removed. `assets/models/test.mesh` is now committed, converted explicitly
offline using GLBConverter, alongside the existing `monkey.mesh`. The converter
remains a separate executable and does not stage runtime files.

Root `CMakeLists.txt`, after `add_subdirectory(apps/VulkanSandbox)`, declares one
`VulkanEngineAssets` custom target with one `cmake -E copy_directory` command:

- Source: `${PROJECT_SOURCE_DIR}/assets` (existing repository casing).
- Destination: `$<TARGET_FILE_DIR:VulkanSandbox>/Assets`.
- Runtime model directory: `$<TARGET_FILE_DIR:VulkanSandbox>/Assets/models`.
- Trigger: `add_dependencies(VulkanSandbox VulkanEngineAssets)`.

An always-run target is used instead of a root-level TARGET POST_BUILD command:
CMake requires that build event to be declared in the directory that created the
target, and POST_BUILD alone would miss asset-only changes when no relink occurs.
The directory-copy target runs even for an otherwise up-to-date app build and
needs no per-model file list or reconfiguration. CMake 3.24's target-directory
generator expression does not add a dependency back to VulkanSandbox, avoiding a
cycle. The existing app-local DLL copy rule is unchanged.

The source tree remains lowercase `assets/models` rather than being renamed for
this cleanup. On Windows, `Assets/Models/test.mesh` also resolves to the deployed
`Assets/models/test.mesh`. No shader/texture directories are created because those
folders do not exist in the source asset tree. Existing shader compilation is
unchanged. Directory copying adds/updates files; it does not prune obsolete output
files, so remove old deployment output when a pristine package is needed.

## Verification

Regenerate and clean/rebuild VulkanSandbox. Check `Assets/models/test.mesh` and
`Assets/models/monkey.mesh` beneath the target output against their source hashes,
and GraphicsEngine.dll against the built DLL. Add a temporary source asset and
build the already up-to-date app again: the new asset must deploy without CMake
edits or relinking. Also build another configuration to check the target-derived
destination. Renderer code, pipeline, and asset format are unchanged.
