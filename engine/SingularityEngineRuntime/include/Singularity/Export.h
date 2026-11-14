#pragma once

#ifdef _WIN32

#ifdef SINGULARITY_ENGINE_RUNTIME_EXPORTS
#define SINGULARITY_ENGINE_RUNTIME_API __declspec(dllexport)
#else
#define SINGULARITY_ENGINE_RUNTIME_API __declspec(dllimport)
#endif

#else

#define SINGULARITY_ENGINE_RUNTIME_API

#endif
