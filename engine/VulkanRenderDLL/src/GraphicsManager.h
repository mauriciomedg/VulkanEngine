#pragma once

#include "MeshData.h"

#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

class VulkanContext;

class GraphicsManager
{
public:
    explicit GraphicsManager(VulkanContext& context);
    ~GraphicsManager();

    GraphicsManager(const GraphicsManager&) = delete;
    GraphicsManager& operator=(const GraphicsManager&) = delete;

    VkCommandBuffer getCurrentCommandBuffer() const;
    VkSemaphore getImageAvailableSemaphore() const;
    VkSemaphore getRenderFinishedSemaphore(uint32_t imageIndex) const;
    VkFence getCurrentInFlightFence() const;
    const MeshGpuData& getQuadMesh() const;

    void waitForCurrentFrame() const;
    void resetCurrentFrameFence() const;
    void resetCurrentCommandBuffer() const;
    void submitCurrentFrame(uint32_t imageIndex) const;
    void advanceFrame();

private:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    uint32_t findMemoryType(
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties) const;
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory);
    void createQuadMesh();
    void destroyMesh(MeshGpuData& mesh);
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    VulkanContext& m_context;
    MeshGpuData m_quadMesh;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
};

}
