#pragma once

#include "RenderPass.h"

#include <vector>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

class VulkanContext;
struct DynamicMeshGpuData;

class OpaquePass final : public RenderPass
{
public:
    explicit OpaquePass(VulkanContext& context);
    ~OpaquePass() override;

    OpaquePass(const OpaquePass&) = delete;
    OpaquePass& operator=(const OpaquePass&) = delete;

    void record(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        const MeshGpuData& mesh) override;

    void record(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        const DynamicMeshGpuData& mesh,
        uint32_t frameIndex);

private:
    void recordMesh(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        VkBuffer vertexBuffer,
        VkBuffer indexBuffer,
        uint32_t indexCount);
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char>& code) const;

    VulkanContext& m_context;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}
