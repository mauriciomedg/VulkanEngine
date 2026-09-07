#include <VulkanRender/Renderer.h>

#include "Window.h"

namespace VulkanRender
{

Renderer::Renderer()
    : m_window(std::make_unique<Window>(1280, 720, "Vulkan Mini Engine"))
{
}

Renderer::~Renderer() = default;

void Renderer::run()
{
    while (!m_window->shouldClose())
    {
        m_window->pollEvents();
    }
}

}
