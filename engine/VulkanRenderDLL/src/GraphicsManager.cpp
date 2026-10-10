#include "GraphicsManager.h"

#include "VulkanContext.h"

#include <array>
#include <cstring>
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

GraphicsManager::GraphicsManager(VulkanContext& context)
    : m_context(context)
{
    createQuadMesh();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

GraphicsManager::~GraphicsManager()
{
    const VkDevice device = m_context.getDevice();

    for (VkSemaphore semaphore : m_imageAvailableSemaphores)
    {
        if (semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }

    for (VkSemaphore semaphore : m_renderFinishedSemaphores)
    {
        if (semaphore != VK_NULL_HANDLE)
        {
            vkDestroySemaphore(device, semaphore, nullptr);
        }
    }

    for (VkFence fence : m_inFlightFences)
    {
        if (fence != VK_NULL_HANDLE)
        {
            vkDestroyFence(device, fence, nullptr);
        }
    }

    m_imageAvailableSemaphores.clear();
    m_renderFinishedSemaphores.clear();
    m_inFlightFences.clear();

    if (m_commandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(device, m_commandPool, nullptr);
        m_commandPool = VK_NULL_HANDLE;
        m_commandBuffers.clear();
    }

    destroyMesh(m_quadMesh);
}

VkCommandBuffer GraphicsManager::getCurrentCommandBuffer() const
{
    return m_commandBuffers[m_currentFrame];
}

VkSemaphore GraphicsManager::getImageAvailableSemaphore() const
{
    return m_imageAvailableSemaphores[m_currentFrame];
}

VkSemaphore GraphicsManager::getRenderFinishedSemaphore(uint32_t imageIndex) const
{
    return m_renderFinishedSemaphores.at(imageIndex);
}

VkFence GraphicsManager::getCurrentInFlightFence() const
{
    return m_inFlightFences[m_currentFrame];
}

const MeshGpuData& GraphicsManager::getQuadMesh() const
{
    return m_quadMesh;
}

void GraphicsManager::waitForCurrentFrame() const
{
    const VkResult result = vkWaitForFences(
        m_context.getDevice(),
        1,
        &m_inFlightFences[m_currentFrame],
        VK_TRUE,
        UINT64_MAX);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to wait for Vulkan in-flight frame fence");
    }
}

void GraphicsManager::resetCurrentFrameFence() const
{
    const VkResult result =
        vkResetFences(m_context.getDevice(), 1, &m_inFlightFences[m_currentFrame]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to reset Vulkan in-flight frame fence");
    }
}

void GraphicsManager::resetCurrentCommandBuffer() const
{
    const VkResult result = vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to reset Vulkan command buffer");
    }
}

void GraphicsManager::submitCurrentFrame(uint32_t imageIndex) const
{
    VkSemaphore waitSemaphores[] = {
        m_imageAvailableSemaphores[m_currentFrame]
    };
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    VkSemaphore signalSemaphores[] = {
        m_renderFinishedSemaphores.at(imageIndex)
    };

    VkCommandBuffer commandBuffer = m_commandBuffers[m_currentFrame];

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    const VkResult result = vkQueueSubmit(
        m_context.getGraphicsQueue(),
        1,
        &submitInfo,
        m_inFlightFences[m_currentFrame]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to submit Vulkan frame commands");
    }
}

void GraphicsManager::advanceFrame()
{
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

uint32_t GraphicsManager::findMemoryType(
    uint32_t typeFilter,
    VkMemoryPropertyFlags properties) const
{
    VkPhysicalDeviceMemoryProperties memoryProperties = {};
    vkGetPhysicalDeviceMemoryProperties(m_context.getPhysicalDevice(), &memoryProperties);

    for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        const bool typeSupported = (typeFilter & (1u << i)) != 0;
        const bool hasRequestedProperties =
            (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;

        if (typeSupported && hasRequestedProperties)
        {
            std::cout << "Selected Vulkan memory type " << i
                << " for properties 0x" << std::hex << properties << std::dec << ".\n";
            return i;
        }
    }

    throw std::runtime_error("Failed to find a compatible Vulkan memory type");
}

void GraphicsManager::createBuffer(
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer& buffer,
    VkDeviceMemory& bufferMemory)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    const VkDevice device = m_context.getDevice();
    VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan buffer");
    }

    VkMemoryRequirements memoryRequirements = {};
    vkGetBufferMemoryRequirements(device, buffer, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo = {};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex =
        findMemoryType(memoryRequirements.memoryTypeBits, properties);

    result = vkAllocateMemory(device, &allocateInfo, nullptr, &bufferMemory);
    if (result != VK_SUCCESS)
    {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to allocate Vulkan buffer memory");
    }

    result = vkBindBufferMemory(device, buffer, bufferMemory, 0);
    if (result != VK_SUCCESS)
    {
        vkFreeMemory(device, bufferMemory, nullptr);
        vkDestroyBuffer(device, buffer, nullptr);
        bufferMemory = VK_NULL_HANDLE;
        buffer = VK_NULL_HANDLE;
        throw std::runtime_error("Failed to bind Vulkan buffer memory");
    }
}

void GraphicsManager::createQuadMesh()
{
    m_quadMesh.vertexBufferSize = sizeof(kQuadVertices[0]) * kQuadVertices.size();
    createBuffer(
        m_quadMesh.vertexBufferSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_quadMesh.vertexBuffer,
        m_quadMesh.vertexMemory);

    void* vertexData = nullptr;
    VkResult result = vkMapMemory(
        m_context.getDevice(),
        m_quadMesh.vertexMemory,
        0,
        m_quadMesh.vertexBufferSize,
        0,
        &vertexData);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to map Vulkan vertex buffer memory");
    }

    std::memcpy(vertexData, kQuadVertices.data(), static_cast<size_t>(m_quadMesh.vertexBufferSize));
    vkUnmapMemory(m_context.getDevice(), m_quadMesh.vertexMemory);

    m_quadMesh.indexBufferSize = sizeof(kQuadIndices[0]) * kQuadIndices.size();
    createBuffer(
        m_quadMesh.indexBufferSize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        m_quadMesh.indexBuffer,
        m_quadMesh.indexMemory);

    void* indexData = nullptr;
    result = vkMapMemory(
        m_context.getDevice(),
        m_quadMesh.indexMemory,
        0,
        m_quadMesh.indexBufferSize,
        0,
        &indexData);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to map Vulkan index buffer memory");
    }

    std::memcpy(indexData, kQuadIndices.data(), static_cast<size_t>(m_quadMesh.indexBufferSize));
    vkUnmapMemory(m_context.getDevice(), m_quadMesh.indexMemory);

    m_quadMesh.indexCount = static_cast<uint32_t>(kQuadIndices.size());
    std::cout << "Uploaded " << kQuadVertices.size()
        << " quad vertices (" << m_quadMesh.vertexBufferSize << " bytes).\n";
    std::cout << "Uploaded " << m_quadMesh.indexCount
        << " quad indices (" << m_quadMesh.indexBufferSize << " bytes).\n";
}

void GraphicsManager::destroyMesh(MeshGpuData& mesh)
{
    const VkDevice device = m_context.getDevice();

    if (mesh.indexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
        mesh.indexBuffer = VK_NULL_HANDLE;
    }

    if (mesh.indexMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, mesh.indexMemory, nullptr);
        mesh.indexMemory = VK_NULL_HANDLE;
    }

    if (mesh.vertexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, mesh.vertexBuffer, nullptr);
        mesh.vertexBuffer = VK_NULL_HANDLE;
    }

    if (mesh.vertexMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, mesh.vertexMemory, nullptr);
        mesh.vertexMemory = VK_NULL_HANDLE;
    }

    mesh.vertexBufferSize = 0;
    mesh.indexBufferSize = 0;
    mesh.indexCount = 0;
}

void GraphicsManager::createCommandPool()
{
    VkCommandPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = m_context.getGraphicsQueueFamily();

    const VkResult result =
        vkCreateCommandPool(m_context.getDevice(), &createInfo, nullptr, &m_commandPool);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan command pool");
    }

    std::cout << "Command pool created.\n";
}

void GraphicsManager::createCommandBuffers()
{
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocationInfo = {};
    allocationInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocationInfo.commandPool = m_commandPool;
    allocationInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocationInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

    const VkResult result =
        vkAllocateCommandBuffers(m_context.getDevice(), &allocationInfo, m_commandBuffers.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate Vulkan command buffers");
    }

    std::cout << "Allocated " << m_commandBuffers.size() << " command buffers.\n";
}

void GraphicsManager::createSyncObjects()
{
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
    m_renderFinishedSemaphores.resize(m_context.getSwapchainImageCount(), VK_NULL_HANDLE);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        const VkResult imageAvailableResult = vkCreateSemaphore(
            m_context.getDevice(),
            &semaphoreInfo,
            nullptr,
            &m_imageAvailableSemaphores[i]);
        const VkResult fenceResult =
            vkCreateFence(m_context.getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]);

        if (imageAvailableResult != VK_SUCCESS || fenceResult != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan frame synchronization objects");
        }
    }

    for (size_t i = 0; i < m_renderFinishedSemaphores.size(); ++i)
    {
        const VkResult result = vkCreateSemaphore(
            m_context.getDevice(),
            &semaphoreInfo,
            nullptr,
            &m_renderFinishedSemaphores[i]);

        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan presentation semaphore");
        }
    }

    std::cout << "Created frame synchronization objects.\n";
}

}
