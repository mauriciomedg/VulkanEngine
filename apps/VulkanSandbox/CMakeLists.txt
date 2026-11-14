add_executable(VulkanSandbox
    src/main.cpp
)

target_link_libraries(VulkanSandbox
    PRIVATE
        SingularityEngineRuntime
)

set_target_properties(VulkanSandbox PROPERTIES
    FOLDER "Apps"
)

add_custom_command(TARGET VulkanSandbox POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SingularityEngineRuntime>
        $<TARGET_FILE:GraphicsEngine>
        $<TARGET_FILE_DIR:VulkanSandbox>
    COMMENT "Copying SingularityEngineRuntime and GraphicsEngine beside VulkanSandbox"
)
