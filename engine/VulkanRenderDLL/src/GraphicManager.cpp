#include "GraphicManager.h"

#include "VulkanContext.h"

#include <cstring>
#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace VulkanRender
{
GraphicManager::GraphicManager(VulkanContext& context)
    : m_context(context)
{
    try
    {
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
        createDepthResources();
    }
    catch (...)
    {
        destroyResources();
        throw;
    }
}

GraphicManager::~GraphicManager()
{
    vkDeviceWaitIdle(m_context.getDevice());
    destroyResources();
}

void GraphicManager::destroyResources()
{
    const VkDevice device = m_context.getDevice();
    destroyDepthResources();

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
}

uint32_t GraphicManager::getCurrentFrameIndex() const
{
    return m_currentFrame;
}

VkFormat GraphicManager::getDepthFormat() const
{
    return m_depthFormat;
}

VkImageView GraphicManager::getDepthImageView() const
{
    return m_depthImageView;
}

VkFormat GraphicManager::chooseDepthFormat() const
{
    constexpr std::array<VkFormat, 3> candidates = {
        VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT
    };
    for (VkFormat format : candidates)
    {
        VkFormatProperties properties{};
        vkGetPhysicalDeviceFormatProperties(m_context.getPhysicalDevice(), format, &properties);
        if (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            return format;
        }
    }
    throw std::runtime_error("No supported optimal-tiling depth attachment format");
}

void GraphicManager::createDepthResources()
{
    m_depthFormat = chooseDepthFormat();
    const VkExtent2D extent = m_context.getSwapchainExtent();
    const VkDevice device = m_context.getDevice();

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = m_depthFormat;
    imageInfo.extent = {extent.width, extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &imageInfo, nullptr, &m_depthImage) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create depth image");
    }

    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, m_depthImage, &requirements);
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = requirements.size;
    allocation.memoryTypeIndex = findMemoryType(requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(device, &allocation, nullptr, &m_depthMemory) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to allocate device-local depth memory");
    }
    if (vkBindImageMemory(device, m_depthImage, m_depthMemory, 0) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to bind depth image memory");
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create depth image view");
    }

    const char* formatName = m_depthFormat == VK_FORMAT_D32_SFLOAT ? "VK_FORMAT_D32_SFLOAT"
        : m_depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ? "VK_FORMAT_D32_SFLOAT_S8_UINT"
        : "VK_FORMAT_D24_UNORM_S8_UINT";
    std::cout << "Depth buffer:\n  format: " << formatName
        << "\n  extent: " << extent.width << " x " << extent.height
        << "\n  memory: DEVICE_LOCAL\n";
}

void GraphicManager::destroyDepthResources()
{
    const VkDevice device = m_context.getDevice();
    if (m_depthImageView != VK_NULL_HANDLE) vkDestroyImageView(device, m_depthImageView, nullptr);
    if (m_depthImage != VK_NULL_HANDLE) vkDestroyImage(device, m_depthImage, nullptr);
    if (m_depthMemory != VK_NULL_HANDLE) vkFreeMemory(device, m_depthMemory, nullptr);
    m_depthImageView = VK_NULL_HANDLE;
    m_depthImage = VK_NULL_HANDLE;
    m_depthMemory = VK_NULL_HANDLE;
    m_depthFormat = VK_FORMAT_UNDEFINED;
    m_depthInitialized = false;
}

void GraphicManager::prepareDepthAttachment(VkCommandBuffer commandBuffer)
{
    const VkPipelineStageFlags depthStages = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
        | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = m_depthInitialized ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT : 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
        | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = m_depthInitialized ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_UNDEFINED;
    // Combined layout works without enabling the separateDepthStencilLayouts feature.
    barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_depthImage;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    if (m_depthFormat != VK_FORMAT_D32_SFLOAT)
    {
        barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
    }
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer,
        m_depthInitialized ? depthStages : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        depthStages, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    m_depthInitialized = true;
}

VkCommandBuffer GraphicManager::getCurrentCommandBuffer() const
{
    return m_commandBuffers[m_currentFrame];
}

VkSemaphore GraphicManager::getImageAvailableSemaphore() const
{
    return m_imageAvailableSemaphores[m_currentFrame];
}

VkSemaphore GraphicManager::getRenderFinishedSemaphore(uint32_t imageIndex) const
{
    return m_renderFinishedSemaphores.at(imageIndex);
}

VkFence GraphicManager::getCurrentInFlightFence() const
{
    return m_inFlightFences[m_currentFrame];
}

void GraphicManager::waitForCurrentFrame() const
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

void GraphicManager::resetCurrentFrameFence() const
{
    const VkResult result =
        vkResetFences(m_context.getDevice(), 1, &m_inFlightFences[m_currentFrame]);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to reset Vulkan in-flight frame fence");
    }
}

void GraphicManager::resetCurrentCommandBuffer() const
{
    const VkResult result = vkResetCommandBuffer(m_commandBuffers[m_currentFrame], 0);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to reset Vulkan command buffer");
    }
}

void GraphicManager::submitCurrentFrame(uint32_t imageIndex) const
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

void GraphicManager::advanceFrame()
{
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

uint32_t GraphicManager::findMemoryType(
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

void GraphicManager::createBuffer(
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

MeshGpuData GraphicManager::createMesh(
    const Vertex* vertices,
    size_t vertexCount,
    const uint32_t* indices,
    size_t indexCount)
{
    if (vertices == nullptr || indices == nullptr || vertexCount == 0 || indexCount == 0)
    {
        throw std::invalid_argument("Mesh requires non-empty vertex and index data");
    }

    if (vertexCount > (std::numeric_limits<size_t>::max)() / sizeof(Vertex) ||
        indexCount > (std::numeric_limits<size_t>::max)() / sizeof(uint32_t) ||
        indexCount > (std::numeric_limits<uint32_t>::max)())
    {
        throw std::invalid_argument("Mesh counts exceed supported buffer or draw sizes");
    }

    MeshGpuData mesh;
    try
    {
        mesh.vertexBufferSize = sizeof(Vertex) * vertexCount;
        createBuffer(
            mesh.vertexBufferSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            mesh.vertexBuffer,
            mesh.vertexMemory);

        void* vertexData = nullptr;
        VkResult result = vkMapMemory(
            m_context.getDevice(),
            mesh.vertexMemory,
            0,
            mesh.vertexBufferSize,
            0,
            &vertexData);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map Vulkan vertex buffer memory");
        }

        std::memcpy(vertexData, vertices, static_cast<size_t>(mesh.vertexBufferSize));
        vkUnmapMemory(m_context.getDevice(), mesh.vertexMemory);

        mesh.indexBufferSize = sizeof(uint32_t) * indexCount;
        createBuffer(
            mesh.indexBufferSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            mesh.indexBuffer,
            mesh.indexMemory);

        void* indexData = nullptr;
        result = vkMapMemory(
            m_context.getDevice(),
            mesh.indexMemory,
            0,
            mesh.indexBufferSize,
            0,
            &indexData);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map Vulkan index buffer memory");
        }

        std::memcpy(indexData, indices, static_cast<size_t>(mesh.indexBufferSize));
        vkUnmapMemory(m_context.getDevice(), mesh.indexMemory);

        mesh.indexCount = static_cast<uint32_t>(indexCount);
    }
    catch (...)
    {
        destroyMesh(mesh);
        throw;
    }

    std::cout << "Uploaded " << vertexCount
        << " mesh vertices (" << mesh.vertexBufferSize << " bytes).\n";
    std::cout << "Uploaded " << mesh.indexCount
        << " mesh indices (" << mesh.indexBufferSize << " bytes).\n";
    return mesh;
}

void GraphicManager::destroyMesh(MeshGpuData& mesh)
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

DynamicMeshGpuData GraphicManager::createDynamicMesh(
    const Vertex* vertices,
    size_t vertexCount,
    const uint32_t* indices,
    size_t indexCount)
{
    if (vertices == nullptr || indices == nullptr || vertexCount == 0 || indexCount == 0 ||
        vertexCount > (std::numeric_limits<size_t>::max)() / sizeof(Vertex) ||
        indexCount > (std::numeric_limits<size_t>::max)() / sizeof(uint32_t) ||
        vertexCount > (std::numeric_limits<uint32_t>::max)() ||
        indexCount > (std::numeric_limits<uint32_t>::max)())
    {
        throw std::invalid_argument("Dynamic mesh requires valid, non-empty vertex and index data");
    }

    DynamicMeshGpuData mesh;
    mesh.vertexBufferSize = sizeof(Vertex) * vertexCount;
    mesh.indexBufferSize = sizeof(uint32_t) * indexCount;
    mesh.vertexCount = static_cast<uint32_t>(vertexCount);
    mesh.indexCount = static_cast<uint32_t>(indexCount);
    const VkDevice device = m_context.getDevice();
    const VkMemoryPropertyFlags properties =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    try
    {
        for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
        {
            createBuffer(mesh.vertexBufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, properties,
                mesh.vertexBuffers[frame], mesh.vertexMemories[frame]);
            void* mapped = nullptr;
            if (vkMapMemory(device, mesh.vertexMemories[frame], 0,
                mesh.vertexBufferSize, 0, &mapped) != VK_SUCCESS)
            {
                throw std::runtime_error("Failed to persistently map dynamic vertex memory");
            }
            mesh.mappedVertexMemory[frame] = mapped;
            std::memcpy(mapped, vertices, static_cast<size_t>(mesh.vertexBufferSize));
        }

        createBuffer(mesh.indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, properties,
            mesh.indexBuffer, mesh.indexMemory);
        void* mappedIndices = nullptr;
        if (vkMapMemory(device, mesh.indexMemory, 0,
            mesh.indexBufferSize, 0, &mappedIndices) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to map dynamic mesh index memory");
        }
        std::memcpy(mappedIndices, indices, static_cast<size_t>(mesh.indexBufferSize));
        vkUnmapMemory(device, mesh.indexMemory);
    }
    catch (...)
    {
        destroyDynamicMesh(mesh);
        throw;
    }

    std::cout << "Uploaded " << mesh.vertexCount << " dynamic vertices ("
        << mesh.vertexBufferSize << " bytes per slot) to " << MAX_FRAMES_IN_FLIGHT
        << " persistently mapped frame buffers; " << mesh.indexCount
        << " shared indices (" << mesh.indexBufferSize << " bytes).\n";
    return mesh;
}

void GraphicManager::updateDynamicMesh(
    DynamicMeshGpuData& mesh,
    uint32_t frameIndex,
    const Vertex* vertices,
    size_t vertexCount)
{
    if (frameIndex >= MAX_FRAMES_IN_FLIGHT || vertices == nullptr ||
        vertexCount == 0 || vertexCount != mesh.vertexCount ||
        mesh.mappedVertexMemory[frameIndex] == nullptr)
    {
        throw std::invalid_argument("Invalid dynamic mesh frame slot or vertex data");
    }

    std::memcpy(mesh.mappedVertexMemory[frameIndex], vertices,
        static_cast<size_t>(mesh.vertexBufferSize));
}

void GraphicManager::destroyDynamicMesh(DynamicMeshGpuData& mesh)
{
    const VkDevice device = m_context.getDevice();
    for (uint32_t frame = 0; frame < MAX_FRAMES_IN_FLIGHT; ++frame)
    {
        if (mesh.mappedVertexMemory[frame] != nullptr)
        {
            vkUnmapMemory(device, mesh.vertexMemories[frame]);
        }
        if (mesh.vertexBuffers[frame] != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(device, mesh.vertexBuffers[frame], nullptr);
        }
        if (mesh.vertexMemories[frame] != VK_NULL_HANDLE)
        {
            vkFreeMemory(device, mesh.vertexMemories[frame], nullptr);
        }
    }
    if (mesh.indexBuffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, mesh.indexBuffer, nullptr);
    }
    if (mesh.indexMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, mesh.indexMemory, nullptr);
    }
    mesh = {};
}

void GraphicManager::createCommandPool()
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

void GraphicManager::createCommandBuffers()
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

void GraphicManager::createSyncObjects()
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
