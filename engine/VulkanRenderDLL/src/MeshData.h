#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

struct Vertex
{
    float position[2];
    float color[3];
};

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

}
