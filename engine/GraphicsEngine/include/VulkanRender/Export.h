#pragma once

#ifdef _WIN32

#ifdef GRAPHICS_ENGINE_EXPORTS
#define GRAPHICS_ENGINE_API __declspec(dllexport)
#else
#define GRAPHICS_ENGINE_API __declspec(dllimport)
#endif

#else

#define GRAPHICS_ENGINE_API

#endif
