add_library(VulkanRenderDLL SHARED
    include/VulkanRender/Export.h
    include/VulkanRender/Renderer.h
    src/Renderer.cpp
    src/VulkanContext.cpp
    src/VulkanContext.h
    src/Window.cpp
    src/Window.h
)

target_compile_definitions(VulkanRenderDLL
    PRIVATE
        VULKAN_RENDER_EXPORTS
        $<$<CONFIG:Debug>:VULKAN_RENDER_ENABLE_VALIDATION_LAYERS>
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

set_target_properties(VulkanRenderDLL PROPERTIES
    FOLDER "Engine"
    WINDOWS_EXPORT_ALL_SYMBOLS OFF
)
