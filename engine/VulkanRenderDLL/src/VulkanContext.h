#pragma once

#include <optional>
#include <vector>
#include <vulkan/vulkan.h>

namespace VulkanRender
{

class Window;

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct SwapchainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities = {};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanContext
{
public:
    explicit VulkanContext(const Window& window);
    ~VulkanContext();

    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

private:
    void createInstance();
    void createDebugMessenger();
    void createSurface(const Window& window);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain(const Window& window);
    void createSwapchainImageViews();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, bool logQueues = false) const;
    bool checkDeviceExtensionSupport(VkPhysicalDevice device) const;
    SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device) const;
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& formats) const;
    VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& presentModes) const;
    VkExtent2D chooseSwapExtent(
        const VkSurfaceCapabilitiesKHR& capabilities,
        const Window& window) const;
    VkCompositeAlphaFlagBitsKHR chooseCompositeAlpha(
        VkCompositeAlphaFlagsKHR supportedCompositeAlpha) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    QueueFamilyIndices m_queueFamilyIndices;
    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapchainImages;
    std::vector<VkImageView> m_swapchainImageViews;
    VkFormat m_swapchainImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_swapchainExtent = {};
};

}
