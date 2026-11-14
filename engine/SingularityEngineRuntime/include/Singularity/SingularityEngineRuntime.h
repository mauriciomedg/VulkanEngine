#pragma once

#include "Export.h"
#include <memory>
#include <cstdint>

namespace VulkanRender
{
class Renderer;
}

namespace Singularity
{

// Owns subsystem lifetimes and coordinates frames through their public APIs.
class SINGULARITY_ENGINE_RUNTIME_API SingularityEngineRuntime
{
public:
    SingularityEngineRuntime();
    ~SingularityEngineRuntime();

    SingularityEngineRuntime(const SingularityEngineRuntime&) = delete;
    SingularityEngineRuntime& operator=(const SingularityEngineRuntime&) = delete;

    bool shouldClose() const;
    // GraphicsEngine retains its existing frame timing and window/event handling.
    void runFrame();

private:
    void updatePerformanceTitle(float deltaTime);

    std::unique_ptr<VulkanRender::Renderer> m_graphics;
    double m_fpsAccumulator = 0.0;
    uint64_t m_fpsFrameCount = 0;
};

}
