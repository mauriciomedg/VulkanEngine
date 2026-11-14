#include <Singularity/SingularityEngineRuntime.h>
#include <VulkanRender/Renderer.h>

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
    m_graphics->runFrame();
}

}
