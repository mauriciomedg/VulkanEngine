#include "RenderingManager.h"

#include "Window.h"
#include "MeshLoader.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace VulkanRender
{
RenderingManager::RenderingManager(const Window& window)
    : m_window(window)
    , m_context(window)
    , m_graphics(m_context)
    , m_opaquePass(m_context, m_graphics.getDepthFormat())
    , m_wireframePass(m_context, m_graphics.getDepthFormat())
{
    const auto path = std::filesystem::path(VULKAN_RENDER_ASSET_DIR) / "monkey.mesh";
    const MeshData mesh = MeshLoader::load(path);
    m_testMesh = m_graphics.createMesh(mesh);
    std::cout << "Loaded mesh: " << path.string() << " (" << mesh.vertices.size()
        << " vertices, " << mesh.indices.size() << " indices).\n";

    // Event-based input also catches quick taps processed in one glfwPollEvents call.
    glfwSetWindowUserPointer(m_window.getNativeHandle(), this);
    glfwSetKeyCallback(m_window.getNativeHandle(), [](GLFWwindow* window, int key, int, int action, int)
    {
        auto& manager = *static_cast<RenderingManager*>(glfwGetWindowUserPointer(window));
        if (key == GLFW_KEY_F1 && action == GLFW_PRESS && manager.m_context.supportsWireframe())
        {
            manager.m_wireframeEnabled = !manager.m_wireframeEnabled;
            std::cout << "Wireframe overlay: " << (manager.m_wireframeEnabled ? "ON" : "OFF") << '\n';
        }
    });
}

RenderingManager::~RenderingManager()
{
    glfwSetKeyCallback(m_window.getNativeHandle(), nullptr);
    glfwSetWindowUserPointer(m_window.getNativeHandle(), nullptr);
    const VkDevice device = m_context.getDevice();
    if (device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(device);
        m_graphics.destroyMesh(m_testMesh);
    }
}

float RenderingManager::renderFrame()
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
    return deltaTime;
}

void RenderingManager::recordFrame(VkCommandBuffer commandBuffer, uint32_t imageIndex)
{
    const VkExtent2D extent = m_context.getSwapchainExtent();
    if (extent.width == 0 || extent.height == 0)
    {
        throw std::runtime_error("Cannot compute camera projection for an empty swapchain extent");
    }
    CameraPushConstants cameraData{};
    cameraData.model.elements = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    cameraData.view = m_camera.getViewMatrix();
    cameraData.projection = m_camera.getProjectionMatrix(
        static_cast<float>(extent.width) / static_cast<float>(extent.height));

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

    m_graphics.prepareDepthAttachment(commandBuffer);
    VkClearColorValue clearColor = {};
    clearColor.float32[0] = 0.05f;
    clearColor.float32[1] = 0.10f;
    clearColor.float32[2] = 0.20f;
    clearColor.float32[3] = 1.0f;

    VkRenderingAttachmentInfo colorAttachment = {};
    colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAttachment.imageView = m_context.getSwapchainImageView(imageIndex);
    colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.clearValue.color = clearColor;

    VkRenderingAttachmentInfo depthAttachment{};
    depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAttachment.imageView = m_graphics.getDepthImageView();
    depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    renderingInfo.renderArea.offset = {0, 0};
    renderingInfo.renderArea.extent = m_context.getSwapchainExtent();
    renderingInfo.layerCount = 1;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachments = &colorAttachment;
    renderingInfo.pDepthAttachment = &depthAttachment;

    vkCmdBeginRendering(commandBuffer, &renderingInfo);

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    m_opaquePass.record(commandBuffer, m_testMesh, cameraData);
    if (m_wireframeEnabled)
        m_wireframePass.record(commandBuffer, m_testMesh, cameraData);

    vkCmdEndRendering(commandBuffer);

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
