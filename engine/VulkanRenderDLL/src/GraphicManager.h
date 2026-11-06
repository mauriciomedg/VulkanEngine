#pragma once

#include "MeshData.h"

#include <cstddef>
#include <cstdint>
#include <vector>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

class VulkanContext;

class GraphicManager
{
public:
    explicit GraphicManager(VulkanContext& context);
    ~GraphicManager();

    GraphicManager(const GraphicManager&) = delete;
    GraphicManager& operator=(const GraphicManager&) = delete;

    uint32_t getCurrentFrameIndex() const;
    VkCommandBuffer getCurrentCommandBuffer() const;
    VkSemaphore getImageAvailableSemaphore() const;
    VkSemaphore getRenderFinishedSemaphore(uint32_t imageIndex) const;
    VkFence getCurrentInFlightFence() const;
    MeshGpuData createMesh(
        const Vertex* vertices,
        size_t vertexCount,
        const uint32_t* indices,
        size_t indexCount);
    void destroyMesh(MeshGpuData& mesh);
    DynamicMeshGpuData createDynamicMesh(
        const Vertex* vertices,
        size_t vertexCount,
        const uint32_t* indices,
        size_t indexCount);
    // Caller must wait for this frame slot to finish using the mesh before updating.
    void updateDynamicMesh(
        DynamicMeshGpuData& mesh,
        uint32_t frameIndex,
        const Vertex* vertices,
        size_t vertexCount);
    // Caller must ensure all GPU use has finished before destruction.
    void destroyDynamicMesh(DynamicMeshGpuData& mesh);

    void waitForCurrentFrame() const;
    void resetCurrentFrameFence() const;
    void resetCurrentCommandBuffer() const;
    void submitCurrentFrame(uint32_t imageIndex) const;
    void advanceFrame();

private:
    uint32_t findMemoryType(
        uint32_t typeFilter,
        VkMemoryPropertyFlags properties) const;
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer& buffer,
        VkDeviceMemory& bufferMemory);
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();

    VulkanContext& m_context;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
};

}
