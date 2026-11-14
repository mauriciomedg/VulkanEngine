#pragma once

#include "Camera.h"
#include "GraphicManager.h"
#include "OpaquePass.h"
#include "WireframePass.h"
#include "VulkanContext.h"

#include <chrono>

namespace VulkanRender
{

class Window;

class RenderingManager
{
public:
    explicit RenderingManager(const Window& window);
    ~RenderingManager();

    RenderingManager(const RenderingManager&) = delete;
    RenderingManager& operator=(const RenderingManager&) = delete;

    void renderFrame();

private:
    void recordFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void transitionSwapchainImage(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        VkImageLayout oldLayout,
        VkImageLayout newLayout,
        VkAccessFlags srcAccessMask,
        VkAccessFlags dstAccessMask,
        VkPipelineStageFlags srcStage,
        VkPipelineStageFlags dstStage);

    const Window& m_window;
    Camera m_camera;
    VulkanContext m_context;
    GraphicManager m_graphics;
    OpaquePass m_opaquePass;
    WireframePass m_wireframePass;
    MeshGpuData m_testMesh;
    std::chrono::steady_clock::time_point m_previousFrameTime = std::chrono::steady_clock::now();
    bool m_loggedSuboptimalSwapchain = false;
    bool m_wireframeEnabled = true;
};

}
