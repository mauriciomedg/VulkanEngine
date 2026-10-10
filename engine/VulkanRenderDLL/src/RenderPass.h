#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

class RenderPass
{
public:
    virtual ~RenderPass() = default;

    virtual void record(VkCommandBuffer commandBuffer, uint32_t imageIndex) = 0;
};

}
