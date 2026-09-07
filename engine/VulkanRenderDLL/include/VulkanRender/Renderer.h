#pragma once

#include "Export.h"

#include <memory>

namespace VulkanRender
{

class Window;

class VULKAN_RENDER_API Renderer
{
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void run();

private:
    std::unique_ptr<Window> m_window;
};

}
