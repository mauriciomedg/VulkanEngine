#pragma once

#include "RenderPass.h"

#include <vector>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

class GraphicsManager;
class VulkanContext;

class OpaquePass final : public RenderPass
{
public:
    OpaquePass(VulkanContext& context, GraphicsManager& graphics);
    ~OpaquePass() override;

    OpaquePass(const OpaquePass&) = delete;
    OpaquePass& operator=(const OpaquePass&) = delete;

    void record(VkCommandBuffer commandBuffer, uint32_t imageIndex) override;

private:
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char>& code) const;

    VulkanContext& m_context;
    GraphicsManager& m_graphics;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

}
