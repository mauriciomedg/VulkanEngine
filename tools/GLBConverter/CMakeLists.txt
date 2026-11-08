add_executable(GLBConverter main.cpp MeshWriter.cpp MeshWriter.h)
target_link_libraries(GLBConverter PRIVATE MeshFormat)
set_target_properties(GLBConverter PROPERTIES FOLDER "Tools")

# Always build a runnable sample asset from the committed, self-authored GLB.
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
