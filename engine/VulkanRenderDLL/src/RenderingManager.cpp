#include "RenderingManager.h"

#include "Window.h"

#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace VulkanRender
{
namespace
{

constexpr std::array<Vertex, 4> kQuadVertices = {{
    {{-0.5f, -0.5f}, {1.0f, 0.1f, 0.1f}},
    {{0.5f, -0.5f}, {0.1f, 1.0f, 0.1f}},
    {{0.5f, 0.5f}, {0.1f, 0.3f, 1.0f}},
    {{-0.5f, 0.5f}, {1.0f, 1.0f, 0.1f}}
}};

constexpr std::array<uint32_t, 6> kQuadIndices = {
    0, 1, 2,
    2, 3, 0
};

}

RenderingManager::RenderingManager(const Window& window)
    : m_window(window)
    , m_context(window)
    , m_graphics(m_context)
    , m_opaquePass(m_context)
{
    m_testMesh = m_graphics.createDynamicMesh(
        kQuadVertices.data(),
        kQuadVertices.size(),
        kQuadIndices.data(),
        kQuadIndices.size());
}

RenderingManager::~RenderingManager()
{
    const VkDevice device = m_context.getDevice();
    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);
        m_graphics.destroyDynamicMesh(m_testMesh);
    }
}

void RenderingManager::renderFrame()
{
    const auto now = std::chrono::steady_clock::now();
    const float deltaTime = std::chrono::duration<float>(now - m_previousFrameTime).count();
    m_previousFrameTime = now;
    m_camera.update(m_window.getNativeHandle(), deltaTime);

    m_graphics.waitForCurrentFrame();

    uint32_t imageIndex = 0;
    const VkResult acquireResult = vkAcquireNextImageKHR(
        m_context.getDevice(),
        m_context.getSwapchain(),
        UINT64_MAX,
        m_graphics.getImageAvailableSemaphore(),
        VK_NULL_HANDLE,
        &imageIndex);

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        throw std::runtime_error("Vulkan swapchain is out of date; recreation is not implemented yet");
    }

    if (acquireResult == VK_SUBOPTIMAL_KHR)
    {
        if (!m_loggedSuboptimalSwapchain)
        {
            std::cout << "Vulkan swapchain is suboptimal; resize handling is not implemented yet.\n";
            m_loggedSuboptimalSwapchain = true;
        }
    }
    else if (acquireResult != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to acquire Vulkan swapchain image");
    }

    // Only this frame slot is idle; other slots may still be read by the GPU.
    const float seconds = std::chrono::duration<float>(
        std::chrono::steady_clock::now() - m_animationStart).count();
    auto vertices = kQuadVertices;
    for (size_t i = 0; i < vertices.size(); ++i)
    {
        const float phase = seconds * 2.0f + static_cast<float>(i) * 1.2f;
        vertices[i].position[0] += 0.12f * std::sin(phase);
        vertices[i].position[1] += 0.12f * std::cos(phase * 1.1f);
    }
    m_graphics.updateDynamicMesh(m_testMesh, m_graphics.getCurrentFrameIndex(),
        vertices.data(), vertices.size());

    m_graphics.resetCurrentFrameFence();
    m_graphics.resetCurrentCommandBuffer();

    recordFrame(m_graphics.getCurrentCommandBuffer(), imageIndex);
    m_graphics.submitCurrentFrame(imageIndex);

    VkSemaphore waitSemaphores[] = {
        m_graphics.getRenderFinishedSemaphore(imageIndex)
    };
    VkSwapchainKHR swapchains[] = {
        m_context.getSwapchain()
    };

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = waitSemaphores;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(m_context.getPresentQueue(), &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        throw std::runtime_error("Vulkan swapchain is out of date after presentation; recreation is not implemented yet");
    }

    if (presentResult == VK_SUBOPTIMAL_KHR)
    {
        if (!m_loggedSuboptimalSwapchain)
        {
            std::cout << "Vulkan swapchain is suboptimal; resize handling is not implemented yet.\n";
            m_loggedSuboptimalSwapchain = true;
        }
    }
    else if (presentResult != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to present Vulkan swapchain image");
    }

    m_graphics.advanceFrame();
}

void RenderingManager::recordFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    VkResult result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to begin Vulkan command buffer recording");
    }

    transitionSwapchainImage(
        commandBuffer,
        imageIndex,
        m_context.getSwapchainImageLayout(imageIndex),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        0,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

    m_opaquePass.record(commandBuffer, imageIndex, m_testMesh,
        m_graphics.getCurrentFrameIndex());

    transitionSwapchainImage(
        commandBuffer,
        imageIndex,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        0,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to record Vulkan command buffer");
    }

    m_context.setSwapchainImageLayout(imageIndex, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
}

void RenderingManager::transitionSwapchainImage(
    VkCommandBuffer commandBuffer,
    uint32_t imageIndex,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags srcAccessMask,
    VkAccessFlags dstAccessMask,
    VkPipelineStageFlags srcStage,
    VkPipelineStageFlags dstStage)
{
    VkImageSubresourceRange colorRange = {};
    colorRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    colorRange.baseMipLevel = 0;
    colorRange.levelCount = 1;
    colorRange.baseArrayLayer = 0;
    colorRange.layerCount = 1;

    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_context.getSwapchainImage(imageIndex);
    barrier.subresourceRange = colorRange;

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStage,
        dstStage,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &barrier);
}

}
