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

add_subdirectory(engine/VulkanRenderDLL)
add_subdirectory(apps/VulkanSandbox)
