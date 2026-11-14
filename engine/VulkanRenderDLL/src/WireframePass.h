#pragma once

#include "RenderPass.h"
#include "CameraPushConstants.h"

#include <vector>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

class VulkanContext;
struct DynamicMeshGpuData;

class WireframePass final : public RenderPass
{
public:
    // Conservative overlay bias; constructor arguments allow tuning without new dynamic state.
    WireframePass(VulkanContext& context, VkFormat depthFormat,
        float depthBiasConstantFactor = -1.0f, float depthBiasSlopeFactor = -1.0f);
    ~WireframePass() override;

    WireframePass(const WireframePass&) = delete;
    WireframePass& operator=(const WireframePass&) = delete;

    void record(
        VkCommandBuffer commandBuffer,
        const MeshGpuData& mesh,
        const CameraPushConstants& cameraData) override;

    void record(
        VkCommandBuffer commandBuffer,
        const DynamicMeshGpuData& mesh,
        uint32_t frameIndex,
        const CameraPushConstants& cameraData);

private:
    void recordMesh(
        VkCommandBuffer commandBuffer,
        VkBuffer vertexBuffer,
        VkBuffer indexBuffer,
        uint32_t indexCount,
        const CameraPushConstants& cameraData);
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char>& code) const;

    VulkanContext& m_context;
    VkFormat m_depthFormat;
    float m_depthBiasConstantFactor;
    float m_depthBiasSlopeFactor;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    uint32_t m_cameraPushConstantSize = 0;
};

}
