#include <Singularity/SingularityEngineRuntime.h>
#include <VulkanRender/Renderer.h>

#include <cmath>
#include <iomanip>
#include <sstream>

namespace Singularity
{

SingularityEngineRuntime::SingularityEngineRuntime()
    : m_graphics(std::make_unique<VulkanRender::Renderer>())
{
}

SingularityEngineRuntime::~SingularityEngineRuntime() = default;

bool SingularityEngineRuntime::shouldClose() const
{
    return m_graphics->shouldClose();
}

void SingularityEngineRuntime::runFrame()
{
    const float deltaTime = m_graphics->runFrame();
    updatePerformanceTitle(deltaTime);
}

void SingularityEngineRuntime::updatePerformanceTitle(float deltaTime)
{
    // Reuse the graphics frame clock; this is CPU-observed loop frequency, not GPU timing.
    if (!std::isfinite(deltaTime) || deltaTime <= 0.0f) return;

    m_fpsAccumulator += deltaTime;
    ++m_fpsFrameCount;

    constexpr double FPS_UPDATE_INTERVAL = 0.5;
    if (m_fpsAccumulator < FPS_UPDATE_INTERVAL || m_fpsFrameCount == 0) return;

    const double fps = static_cast<double>(m_fpsFrameCount) / m_fpsAccumulator;
    const double frameMs = m_fpsAccumulator * 1000.0 / static_cast<double>(m_fpsFrameCount);
    std::ostringstream title;
    title << "SingularityEngine | " << std::fixed << std::setprecision(1)
          << fps << " FPS | " << std::setprecision(2) << frameMs << " ms";
    m_graphics->setWindowTitle(title.str());

    m_fpsAccumulator = 0.0;
    m_fpsFrameCount = 0;
}

}
