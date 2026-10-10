#pragma once

#include "GraphicsManager.h"
#include "OpaquePass.h"
#include "VulkanContext.h"

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

    VulkanContext m_context;
    GraphicsManager m_graphics;
    OpaquePass m_opaquePass;
    bool m_loggedSuboptimalSwapchain = false;
};

}
