#include <VulkanRender/Renderer.h>

#include "RenderingManager.h"
#include "Window.h"

#include <iostream>

namespace VulkanRender
{

Renderer::Renderer()
    : m_window(std::make_unique<Window>(1280, 720, "SingularityEngine"))
    , m_renderingManager(std::make_unique<RenderingManager>(*m_window))
{
}

Renderer::~Renderer() = default;

void Renderer::run()
{
    std::cout << "Starting Vulkan frame loop.\n";

    while (!shouldClose())
    {
        runFrame();
    }
}

bool Renderer::shouldClose() const
{
    return m_window->shouldClose();
}

float Renderer::runFrame()
{
    m_window->pollEvents();
    return m_renderingManager->renderFrame();
}

void Renderer::setWindowTitle(const std::string& title)
{
    m_window->setTitle(title);
}

}
