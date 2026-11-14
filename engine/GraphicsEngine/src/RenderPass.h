#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

struct MeshGpuData;
struct CameraPushConstants;

class RenderPass
{
public:
    virtual ~RenderPass() = default;

    virtual void record(
        VkCommandBuffer commandBuffer,
        const MeshGpuData& mesh,
        const CameraPushConstants& cameraData) = 0;
};

}
