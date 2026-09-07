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

This milestone creates a Vulkan instance and surface, so the Vulkan SDK must be installed and discoverable by CMake.

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

Running the application opens an empty `1280 x 720` window named:

```text
Vulkan Mini Engine
```

Closing the window shuts the application down cleanly.

In a Debug build, startup also initializes the Vulkan instance, enables `VK_LAYER_KHRONOS_validation`, creates a `VkDebugUtilsMessengerEXT`, and creates a `VkSurfaceKHR` for the GLFW window.

## Next Milestone

The next Vulkan milestone should enumerate physical devices while leaving the `VulkanSandbox` public-facing code unchanged:

```text
Enumerate VkPhysicalDevice
        |
        v
inspect GPU properties/features
        |
        v
discover queue families
        |
        v
verify graphics + presentation support
        |
        v
select one GPU
```
