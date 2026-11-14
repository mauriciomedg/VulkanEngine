cmake_minimum_required(VERSION 3.24)

project(VulkanEngine
    VERSION 0.1.0
    DESCRIPTION "A small Windows C++ Vulkan learning project"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT VulkanSandbox)

find_package(Vulkan REQUIRED)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(glfw)

set_target_properties(glfw PROPERTIES FOLDER "Dependencies")
if(TARGET update_mappings)
    set_target_properties(update_mappings PROPERTIES FOLDER "Dependencies")
endif()

add_library(MeshFormat INTERFACE)
target_include_directories(MeshFormat INTERFACE "${PROJECT_SOURCE_DIR}/engine/MeshFormat/include")
# Match the deployed models directory without changing runtime renderer code.
set(VULKAN_ENGINE_ASSET_DIR "$<TARGET_FILE_DIR:VulkanSandbox>/Assets/models")
add_subdirectory(tools/GLBConverter)
add_subdirectory(engine/GraphicsEngine)
add_subdirectory(apps/VulkanSandbox)

# The repository's source directory is lowercase assets/models. Preserve its
# contents/casing inside the runtime Assets directory. Always copy on app builds,
# including builds where only assets changed and the executable is up to date.
# TARGET POST_BUILD cannot be attached here to a target created in a subdirectory.
add_custom_target(VulkanEngineAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${PROJECT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:VulkanSandbox>/Assets"
    COMMENT "Copying runtime assets beside VulkanSandbox"
    VERBATIM
)
set_target_properties(VulkanEngineAssets PROPERTIES FOLDER "Assets")
add_dependencies(VulkanSandbox VulkanEngineAssets)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
