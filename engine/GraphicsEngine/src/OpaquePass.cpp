#include "OpaquePass.h"

#include "MeshData.h"
#include "VulkanContext.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace VulkanRender
{
namespace
{

const std::filesystem::path kShaderDirectory = VULKAN_RENDER_SHADER_DIR;

std::vector<char> readFile(const std::filesystem::path& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open())
    {
        throw std::runtime_error("Failed to open file: " + filename.string());
    }

    const std::streamsize fileSize = file.tellg();
    if (fileSize <= 0)
    {
        throw std::runtime_error("File is empty: " + filename.string());
    }

    std::vector<char> buffer(static_cast<size_t>(fileSize));
    file.seekg(0);
    file.read(buffer.data(), fileSize);

    if (!file)
    {
        throw std::runtime_error("Failed to read file: " + filename.string());
    }

    return buffer;
}

}

OpaquePass::OpaquePass(VulkanContext& context, VkFormat depthFormat)
    : m_context(context)
    , m_depthFormat(depthFormat)
{
    createGraphicsPipeline();
}

OpaquePass::~OpaquePass()
{
    const VkDevice device = m_context.getDevice();

    if (m_pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, m_pipeline, nullptr);
        m_pipeline = VK_NULL_HANDLE;
    }

    if (m_pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, m_pipelineLayout, nullptr);
        m_pipelineLayout = VK_NULL_HANDLE;
    }
}

void OpaquePass::record(
    VkCommandBuffer commandBuffer,
    const MeshGpuData& mesh,
    const CameraPushConstants& cameraData)
{
    recordMesh(commandBuffer, mesh.vertexBuffer, mesh.indexBuffer, mesh.indexCount,
        cameraData);
}

void OpaquePass::record(
    VkCommandBuffer commandBuffer,
    const DynamicMeshGpuData& mesh,
    uint32_t frameIndex,
    const CameraPushConstants& cameraData)
{
    recordMesh(commandBuffer, mesh.vertexBuffers.at(frameIndex),
        mesh.indexBuffer, mesh.indexCount, cameraData);
}

void OpaquePass::recordMesh(
    VkCommandBuffer commandBuffer,
    VkBuffer vertexBuffer,
    VkBuffer indexBuffer,
    uint32_t indexCount,
    const CameraPushConstants& cameraData)
{
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

    if (m_cameraPushConstantSize == sizeof(CameraPushConstants))
    {
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
            0, m_cameraPushConstantSize, &cameraData);
    }
    else
    {
        // The reduced shader uses an identity model for this milestone.
        const CameraViewProjectionPushConstants reducedData{cameraData.view, cameraData.projection};
        vkCmdPushConstants(commandBuffer, m_pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
            0, m_cameraPushConstantSize, &reducedData);
    }

    VkBuffer vertexBuffers[] = {
        vertexBuffer
    };
    VkDeviceSize offsets[] = {
        0
    };
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
}

void OpaquePass::createGraphicsPipeline()
{
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_context.getPhysicalDevice(), &properties);
    const uint32_t limit = properties.limits.maxPushConstantsSize;
    if (limit < sizeof(CameraViewProjectionPushConstants))
    {
        throw std::runtime_error("Camera requires at least 128 bytes of push constants; device supports "
            + std::to_string(limit));
    }
    const bool includeModel = limit >= sizeof(CameraPushConstants);
    m_cameraPushConstantSize = includeModel ? sizeof(CameraPushConstants)
        : sizeof(CameraViewProjectionPushConstants);
    std::cout << "Camera push constants: " << m_cameraPushConstantSize
        << " bytes; device maxPushConstantsSize: " << limit << " bytes.\n";

    const std::vector<char> vertexShaderCode = readFile(kShaderDirectory /
        (includeModel ? "triangle.vert.spv" : "triangle.camera128.vert.spv"));
    const std::vector<char> fragmentShaderCode = readFile(kShaderDirectory / "triangle.frag.spv");

    VkShaderModule vertexShaderModule = createShaderModule(vertexShaderCode);
    VkShaderModule fragmentShaderModule = createShaderModule(fragmentShaderCode);

    VkPipelineShaderStageCreateInfo vertexShaderStageInfo = {};
    vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vertexShaderStageInfo.module = vertexShaderModule;
    vertexShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragmentShaderStageInfo = {};
    fragmentShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragmentShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragmentShaderStageInfo.module = fragmentShaderModule;
    fragmentShaderStageInfo.pName = "main";

    const std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {
        vertexShaderStageInfo,
        fragmentShaderStageInfo
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    VkVertexInputBindingDescription bindingDescription = {};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions = {};
    attributeDescriptions[0].binding = 0;
    attributeDescriptions[0].location = 0;
    attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[0].offset = offsetof(Vertex, position);

    attributeDescriptions[1].binding = 0;
    attributeDescriptions[1].location = 1;
    attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[1].offset = offsetof(Vertex, normal);

    attributeDescriptions[2].binding = 0;
    attributeDescriptions[2].location = 2;
    attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescriptions[2].offset = offsetof(Vertex, color);

    vertexInputInfo.vertexBindingDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    vertexInputInfo.vertexAttributeDescriptionCount =
        static_cast<uint32_t>(attributeDescriptions.size());
    vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.sampleShadingEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT |
        VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT |
        VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    const std::array<VkDynamicState, 2> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    VkPushConstantRange cameraRange{};
    cameraRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    cameraRange.offset = 0;
    cameraRange.size = m_cameraPushConstantSize;
    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &cameraRange;

    VkResult result =
        vkCreatePipelineLayout(m_context.getDevice(), &pipelineLayoutInfo, nullptr, &m_pipelineLayout);
    if (result != VK_SUCCESS)
    {
        vkDestroyShaderModule(m_context.getDevice(), fragmentShaderModule, nullptr);
        vkDestroyShaderModule(m_context.getDevice(), vertexShaderModule, nullptr);
        throw std::runtime_error("Failed to create Vulkan pipeline layout");
    }

    const VkFormat colorAttachmentFormat = m_context.getSwapchainImageFormat();
    VkPipelineRenderingCreateInfo renderingInfo = {};
    renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
    renderingInfo.colorAttachmentCount = 1;
    renderingInfo.pColorAttachmentFormats = &colorAttachmentFormat;
    renderingInfo.depthAttachmentFormat = m_depthFormat;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.pNext = &renderingInfo;
    pipelineInfo.stageCount = static_cast<uint32_t>(shaderStages.size());
    pipelineInfo.pStages = shaderStages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pDepthStencilState = &depthStencil;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;
    pipelineInfo.layout = m_pipelineLayout;
    pipelineInfo.renderPass = VK_NULL_HANDLE;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    result = vkCreateGraphicsPipelines(
        m_context.getDevice(),
        VK_NULL_HANDLE,
        1,
        &pipelineInfo,
        nullptr,
        &m_pipeline);

    vkDestroyShaderModule(m_context.getDevice(), fragmentShaderModule, nullptr);
    vkDestroyShaderModule(m_context.getDevice(), vertexShaderModule, nullptr);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan graphics pipeline");
    }

    std::cout << "OpaquePass graphics pipeline created.\n";
}

VkShaderModule OpaquePass::createShaderModule(const std::vector<char>& code) const
{
    if (code.size() % sizeof(uint32_t) != 0)
    {
        throw std::runtime_error("SPIR-V shader bytecode size is not 4-byte aligned");
    }

    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    const VkResult result =
        vkCreateShaderModule(m_context.getDevice(), &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan shader module");
    }

    return shaderModule;
}

}
