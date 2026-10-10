#include <VulkanRender/Renderer.h>

#include "RenderingManager.h"
#include "Window.h"

#include <iostream>

namespace VulkanRender
{

Renderer::Renderer()
    : m_window(std::make_unique<Window>(1280, 720, "Vulkan Mini Engine"))
    , m_renderingManager(std::make_unique<RenderingManager>(*m_window))
{
}

Renderer::~Renderer() = default;

void Renderer::run()
{
    std::cout << "Starting Vulkan frame loop.\n";

    while (!m_window->shouldClose())
    {
        m_window->pollEvents();
        m_renderingManager->renderFrame();
    }
}

}
