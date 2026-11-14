#pragma once

#include "Export.h"

#include <memory>
#include <string>

namespace VulkanRender
{

class RenderingManager;
class Window;

class GRAPHICS_ENGINE_API Renderer
{
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void run();
    // Single-frame facade for hosts that orchestrate multiple engine subsystems.
    bool shouldClose() const;
    // Returns GraphicsEngine's existing frame delta in seconds.
    float runFrame();
    void setWindowTitle(const std::string& title);

private:
    std::unique_ptr<Window> m_window;
    std::unique_ptr<RenderingManager> m_renderingManager;
};

}
