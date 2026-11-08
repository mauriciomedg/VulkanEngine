#pragma once

#include <array>
#include <cstdint>
#include <vulkan/vulkan.h>
#include <MeshFormat.h>

namespace VulkanRender
{

inline constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

struct MeshGpuData
{
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkDeviceSize vertexBufferSize = 0;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    VkDeviceSize indexBufferSize = 0;

    uint32_t indexCount = 0;
};

struct DynamicMeshGpuData
{
    std::array<VkBuffer, MAX_FRAMES_IN_FLIGHT> vertexBuffers = {};
    std::array<VkDeviceMemory, MAX_FRAMES_IN_FLIGHT> vertexMemories = {};
    std::array<void*, MAX_FRAMES_IN_FLIGHT> mappedVertexMemory = {};
    VkDeviceSize vertexBufferSize = 0;

    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    VkDeviceSize indexBufferSize = 0;

    uint32_t vertexCount = 0;
    uint32_t indexCount = 0;
};

}
