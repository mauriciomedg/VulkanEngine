#pragma once

#include <optional>
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
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, bool logQueues = false) const;
    bool isDeviceSuitable(VkPhysicalDevice device) const;

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    QueueFamilyIndices m_queueFamilyIndices;
};

}
