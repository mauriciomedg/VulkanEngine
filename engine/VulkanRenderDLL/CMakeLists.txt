if(NOT Vulkan_GLSLC_EXECUTABLE)
    find_program(Vulkan_GLSLC_EXECUTABLE
        NAMES glslc
        HINTS
            "$ENV{VULKAN_SDK}/Bin"
            "$ENV{VULKAN_SDK}/Bin32"
    )
endif()

if(NOT Vulkan_GLSLC_EXECUTABLE)
    message(FATAL_ERROR "glslc was not found. Install the Vulkan SDK or add glslc to PATH.")
endif()

set(VULKAN_RENDER_SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/shaders")
set(VULKAN_RENDER_SHADER_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/shaders")
set(VULKAN_RENDER_SHADER_OUTPUTS
    "${VULKAN_RENDER_SHADER_OUTPUT_DIR}/triangle.vert.spv"
    "${VULKAN_RENDER_SHADER_OUTPUT_DIR}/triangle.frag.spv"
)

add_custom_command(
    OUTPUT "${VULKAN_RENDER_SHADER_OUTPUT_DIR}/triangle.vert.spv"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${VULKAN_RENDER_SHADER_OUTPUT_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}"
        "${VULKAN_RENDER_SHADER_SOURCE_DIR}/triangle.vert"
        -o "${VULKAN_RENDER_SHADER_OUTPUT_DIR}/triangle.vert.spv"
    DEPENDS "${VULKAN_RENDER_SHADER_SOURCE_DIR}/triangle.vert"
    VERBATIM
    COMMENT "Compiling triangle vertex shader"
)

add_custom_command(
    OUTPUT "${VULKAN_RENDER_SHADER_OUTPUT_DIR}/triangle.frag.spv"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${VULKAN_RENDER_SHADER_OUTPUT_DIR}"
    COMMAND "${Vulkan_GLSLC_EXECUTABLE}"
        "${VULKAN_RENDER_SHADER_SOURCE_DIR}/triangle.frag"
        -o "${VULKAN_RENDER_SHADER_OUTPUT_DIR}/triangle.frag.spv"
    DEPENDS "${VULKAN_RENDER_SHADER_SOURCE_DIR}/triangle.frag"
    VERBATIM
    COMMENT "Compiling triangle fragment shader"
)

add_custom_target(VulkanRenderShaders
    DEPENDS ${VULKAN_RENDER_SHADER_OUTPUTS}
)

add_library(VulkanRenderDLL SHARED
    include/VulkanRender/Export.h
    include/VulkanRender/Renderer.h
    src/GraphicsManager.cpp
    src/GraphicsManager.h
    src/MeshData.h
    src/OpaquePass.cpp
    src/OpaquePass.h
    src/RenderPass.h
    src/Renderer.cpp
    src/RenderingManager.cpp
    src/RenderingManager.h
    src/VulkanContext.cpp
    src/VulkanContext.h
    src/Window.cpp
    src/Window.h
)

target_compile_definitions(VulkanRenderDLL
    PRIVATE
        VULKAN_RENDER_EXPORTS
        $<$<CONFIG:Debug>:VULKAN_RENDER_ENABLE_VALIDATION_LAYERS>
        VULKAN_RENDER_SHADER_DIR="${VULKAN_RENDER_SHADER_OUTPUT_DIR}"
)

target_include_directories(VulkanRenderDLL
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(VulkanRenderDLL
    PRIVATE
        glfw
        Vulkan::Vulkan
)

add_dependencies(VulkanRenderDLL VulkanRenderShaders)

set_target_properties(VulkanRenderDLL PROPERTIES
    FOLDER "Engine"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
