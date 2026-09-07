#pragma once

#ifdef _WIN32

#ifdef VULKAN_RENDER_EXPORTS
#define VULKAN_RENDER_API __declspec(dllexport)
#else
#define VULKAN_RENDER_API __declspec(dllimport)
#endif

#else

#define VULKAN_RENDER_API

#endif
