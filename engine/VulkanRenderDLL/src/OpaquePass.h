#pragma once

#include "RenderPass.h"
#include "Camera.h"

#include <cstddef>
#include <type_traits>

#include <vector>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

class VulkanContext;
struct DynamicMeshGpuData;

// GLSL column-major mat4 layout, with 16-byte column strides.
struct CameraPushConstants
{
    Mat4 model;
    Mat4 view;
    Mat4 projection;
};

struct CameraViewProjectionPushConstants
{
    Mat4 view;
    Mat4 projection;
};

static_assert(sizeof(Mat4) == 64);
static_assert(std::is_standard_layout_v<CameraPushConstants>);
static_assert(std::is_trivially_copyable_v<CameraPushConstants>);
static_assert(sizeof(CameraPushConstants) == 192);
static_assert(offsetof(CameraPushConstants, view) == 64);
static_assert(offsetof(CameraPushConstants, projection) == 128);
static_assert(sizeof(CameraViewProjectionPushConstants) == 128);
static_assert(offsetof(CameraViewProjectionPushConstants, projection) == 64);

class OpaquePass final : public RenderPass
{
public:
    OpaquePass(VulkanContext& context, VkFormat depthFormat);
    ~OpaquePass() override;

    OpaquePass(const OpaquePass&) = delete;
    OpaquePass& operator=(const OpaquePass&) = delete;

    void record(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        const MeshGpuData& mesh,
        const CameraPushConstants& cameraData,
        VkImageView depthImageView) override;

    void record(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        const DynamicMeshGpuData& mesh,
        uint32_t frameIndex,
        const CameraPushConstants& cameraData,
        VkImageView depthImageView);

private:
    void recordMesh(
        VkCommandBuffer commandBuffer,
        uint32_t imageIndex,
        VkBuffer vertexBuffer,
        VkBuffer indexBuffer,
        uint32_t indexCount,
        const CameraPushConstants& cameraData,
        VkImageView depthImageView);
    void createGraphicsPipeline();
    VkShaderModule createShaderModule(const std::vector<char>& code) const;

    VulkanContext& m_context;
    VkFormat m_depthFormat;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    uint32_t m_cameraPushConstantSize = 0;
};

}
