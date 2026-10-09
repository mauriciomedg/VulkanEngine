# VulkanEngine

A small Windows C++ learning project for building toward Vulkan one milestone at a time.

## Architecture

```text
VulkanSandbox.exe
        |
        v
VulkanRenderDLL.dll
        |
        +-- Window
        |
        +-- GLFW
        |
        +-- Vulkan instance, validation, debug messenger, surface
        |
        +-- Vulkan implementation later
```

`VulkanSandbox` only knows about the public `VulkanRender::Renderer` API. Windowing is owned by `VulkanRenderDLL`, and Vulkan objects will be added behind that same boundary in a later milestone.

## Prerequisites

- Visual Studio 2026
- Desktop development with C++
- CMake
- Git, used by CMake `FetchContent` to obtain GLFW
- Windows SDK
- Vulkan SDK

This project uses Vulkan dynamic rendering for the first graphics pipeline milestone, so the Vulkan SDK must be installed and discoverable by CMake. The build also uses the Vulkan SDK `glslc` compiler to generate SPIR-V shaders.

## Generate Project

From any directory, run:

```bat
generate_vs2026.bat
```

The Visual Studio solution and all generated CMake files are written under:

```text
build/
```

## Build

Open the generated solution in `build/` and build:

```text
VulkanSandbox
```

`VulkanSandbox` is configured as the Visual Studio startup project where supported.

## Expected Result

Running the application opens a `1280 x 720` window named:

```text
Vulkan Mini Engine
```

The renderer clears the swapchain image and draws one interpolated-color triangle using a Vulkan graphics pipeline. Closing the window shuts the application down cleanly.

In a Debug build, startup also initializes the Vulkan instance, enables `VK_LAYER_KHRONOS_validation`, creates a `VkDebugUtilsMessengerEXT`, verifies and enables dynamic rendering, and creates the Vulkan surface, swapchain, command resources, synchronization objects, and graphics pipeline.

## Next Milestone

The next Vulkan milestone should introduce real vertex and index buffers while leaving the `VulkanSandbox` public-facing code unchanged:

```text
Create a VkBuffer for vertices
        |
        v
allocate and bind Vulkan memory
        |
        v
upload quad vertex data
        |
        v
create an index buffer
        |
        v
bind vertex and index buffers
        |
        v
vkCmdDrawIndexed
        |
        v
render a quad
```

That milestone should be the first introduction to Vulkan buffer memory and explicit GPU memory management.
