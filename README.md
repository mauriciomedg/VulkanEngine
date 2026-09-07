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
        +-- Vulkan implementation later
```

`VulkanSandbox` only knows about the public `VulkanRender::Renderer` API. Windowing is owned by `VulkanRenderDLL`, and Vulkan objects will be added behind that same boundary in a later milestone.

## Prerequisites

- Visual Studio 2026
- Desktop development with C++
- CMake
- Git, used by CMake `FetchContent` to obtain GLFW
- Windows SDK

The Vulkan SDK is not required for this milestone.

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

## Next Milestone

The next Vulkan milestone should add this internal structure to `VulkanRenderDLL` while leaving the `VulkanSandbox` public-facing code unchanged:

```text
Renderer
   |
   +-- Window
   |
   +-- VulkanContext
          |
          +-- VkInstance
          +-- validation layers
          +-- debug messenger
          +-- VkSurfaceKHR
```
