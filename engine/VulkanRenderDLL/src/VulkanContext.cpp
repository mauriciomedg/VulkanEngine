#include "VulkanContext.h"

#include "Window.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace VulkanRender
{
namespace
{

constexpr std::array<const char*, 1> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

constexpr std::array<const char*, 1> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

#ifdef VULKAN_RENDER_ENABLE_VALIDATION_LAYERS
constexpr bool kEnableValidationLayers = true;
#else
constexpr bool kEnableValidationLayers = false;
#endif

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
    void* userData)
{
    (void)messageSeverity;
    (void)messageType;
    (void)userData;

    std::cerr << "[Vulkan Validation] " << callbackData->pMessage << '\n';
    return VK_FALSE;
}

std::string vulkanVersionToString(uint32_t version)
{
    return std::to_string(VK_VERSION_MAJOR(version)) + "." +
        std::to_string(VK_VERSION_MINOR(version)) + "." +
        std::to_string(VK_VERSION_PATCH(version));
}

const char* physicalDeviceTypeToString(VkPhysicalDeviceType type)
{
    switch (type)
    {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "Discrete GPU";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "Integrated GPU";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "Virtual GPU";
    case VK_PHYSICAL_DEVICE_TYPE_OTHER:
        return "Other";
    default:
        return "Unknown";
    }
}

const char* surfaceFormatToString(VkFormat format)
{
    switch (format)
    {
    case VK_FORMAT_B8G8R8A8_SRGB:
        return "VK_FORMAT_B8G8R8A8_SRGB";
    case VK_FORMAT_B8G8R8A8_UNORM:
        return "VK_FORMAT_B8G8R8A8_UNORM";
    case VK_FORMAT_R8G8B8A8_SRGB:
        return "VK_FORMAT_R8G8B8A8_SRGB";
    case VK_FORMAT_R8G8B8A8_UNORM:
        return "VK_FORMAT_R8G8B8A8_UNORM";
    default:
        return "VK_FORMAT_OTHER";
    }
}

const char* colorSpaceToString(VkColorSpaceKHR colorSpace)
{
    switch (colorSpace)
    {
    case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";
    default:
        return "VK_COLOR_SPACE_OTHER";
    }
}

const char* presentModeToString(VkPresentModeKHR presentMode)
{
    switch (presentMode)
    {
    case VK_PRESENT_MODE_FIFO_KHR:
        return "FIFO";
    case VK_PRESENT_MODE_MAILBOX_KHR:
        return "MAILBOX";
    case VK_PRESENT_MODE_IMMEDIATE_KHR:
        return "IMMEDIATE";
    default:
        return "OTHER";
    }
}

int physicalDevicePreference(VkPhysicalDeviceType type)
{
    switch (type)
    {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 3;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return 2;
    default:
        return 1;
    }
}

uint32_t chooseRequestedApiVersion(uint32_t runtimeVersion)
{
    if (runtimeVersion >= VK_API_VERSION_1_3)
    {
        return VK_API_VERSION_1_3;
    }

    if (runtimeVersion >= VK_API_VERSION_1_2)
    {
        return VK_API_VERSION_1_2;
    }

    if (runtimeVersion >= VK_API_VERSION_1_1)
    {
        return VK_API_VERSION_1_1;
    }

    return VK_API_VERSION_1_0;
}

uint32_t enumerateRuntimeVersion()
{
    uint32_t runtimeVersion = VK_API_VERSION_1_0;

    const auto enumerateInstanceVersion =
        reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
            vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));

    if (enumerateInstanceVersion)
    {
        const VkResult result = enumerateInstanceVersion(&runtimeVersion);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to enumerate Vulkan runtime version");
        }
    }

    return runtimeVersion;
}

bool validationLayersAvailable()
{
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    return std::all_of(
        kValidationLayers.begin(),
        kValidationLayers.end(),
        [&availableLayers](const char* requiredLayer)
        {
            return std::any_of(
                availableLayers.begin(),
                availableLayers.end(),
                [requiredLayer](const VkLayerProperties& layer)
                {
                    return std::strcmp(requiredLayer, layer.layerName) == 0;
                });
        });
}

void addUniqueExtension(std::vector<const char*>& extensions, const char* extension)
{
    const auto found = std::find_if(
        extensions.begin(),
        extensions.end(),
        [extension](const char* existing)
        {
            return std::strcmp(existing, extension) == 0;
        });

    if (found == extensions.end())
    {
        extensions.push_back(extension);
    }
}

std::vector<const char*> getRequiredExtensions()
{
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    if (!glfwExtensions || glfwExtensionCount == 0)
    {
        throw std::runtime_error("Failed to query GLFW Vulkan instance extensions");
    }

    std::cout << "Required Vulkan instance extensions:\n";

    std::vector<const char*> extensions;
    extensions.reserve(glfwExtensionCount + 1);
    for (uint32_t i = 0; i < glfwExtensionCount; ++i)
    {
        std::cout << "  " << glfwExtensions[i] << '\n';
        addUniqueExtension(extensions, glfwExtensions[i]);
    }

    if constexpr (kEnableValidationLayers)
    {
        addUniqueExtension(extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo)
{
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
}

VkResult createDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* createInfo,
    const VkAllocationCallbacks* allocator,
    VkDebugUtilsMessengerEXT* debugMessenger)
{
    const auto function =
        reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));

    if (!function)
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return function(instance, createInfo, allocator, debugMessenger);
}

void destroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* allocator)
{
    const auto function =
        reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));

    if (function)
    {
        function(instance, debugMessenger, allocator);
    }
}

}

VulkanContext::VulkanContext(const Window& window)
{
    std::cout << "Initializing Vulkan...\n";

    createInstance();
    createDebugMessenger();
    createSurface(window);
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapchain(window);
    createSwapchainImageViews();
}

VulkanContext::~VulkanContext()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(m_device);
    }

    for (VkImageView imageView : m_swapchainImageViews)
    {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    m_swapchainImageViews.clear();

    if (m_swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
        m_swapchainImages.clear();
        m_swapchainImageLayouts.clear();
        m_swapchainImageFormat = VK_FORMAT_UNDEFINED;
        m_swapchainExtent = {};
    }

    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
        m_graphicsQueue = VK_NULL_HANDLE;
        m_presentQueue = VK_NULL_HANDLE;
    }

    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }

    if (m_debugMessenger != VK_NULL_HANDLE)
    {
        destroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
}

VkPhysicalDevice VulkanContext::getPhysicalDevice() const
{
    return m_physicalDevice;
}

VkDevice VulkanContext::getDevice() const
{
    return m_device;
}

VkQueue VulkanContext::getGraphicsQueue() const
{
    return m_graphicsQueue;
}

VkQueue VulkanContext::getPresentQueue() const
{
    return m_presentQueue;
}

VkSwapchainKHR VulkanContext::getSwapchain() const
{
    return m_swapchain;
}

VkFormat VulkanContext::getSwapchainImageFormat() const
{
    return m_swapchainImageFormat;
}

VkExtent2D VulkanContext::getSwapchainExtent() const
{
    return m_swapchainExtent;
}

VkImage VulkanContext::getSwapchainImage(uint32_t imageIndex) const
{
    return m_swapchainImages.at(imageIndex);
}

VkImageView VulkanContext::getSwapchainImageView(uint32_t imageIndex) const
{
    return m_swapchainImageViews.at(imageIndex);
}

VkImageLayout VulkanContext::getSwapchainImageLayout(uint32_t imageIndex) const
{
    return m_swapchainImageLayouts.at(imageIndex);
}

void VulkanContext::setSwapchainImageLayout(uint32_t imageIndex, VkImageLayout layout)
{
    m_swapchainImageLayouts.at(imageIndex) = layout;
}

size_t VulkanContext::getSwapchainImageCount() const
{
    return m_swapchainImages.size();
}

uint32_t VulkanContext::getGraphicsQueueFamily() const
{
    return *m_queueFamilyIndices.graphicsFamily;
}

void VulkanContext::createInstance()
{
    const uint32_t runtimeVersion = enumerateRuntimeVersion();
    std::cout << "Vulkan runtime version: " << vulkanVersionToString(runtimeVersion) << '\n';

    if constexpr (kEnableValidationLayers)
    {
        if (!validationLayersAvailable())
        {
            throw std::runtime_error(
                "Validation layers requested, but VK_LAYER_KHRONOS_validation is unavailable");
        }

        std::cout << "Validation layers enabled.\n";
    }

    const std::vector<const char*> extensions = getRequiredExtensions();

    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName = "Vulkan Sandbox";
    applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.pEngineName = "VulkanEngine";
    applicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    applicationInfo.apiVersion = chooseRequestedApiVersion(runtimeVersion);

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &applicationInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    if constexpr (kEnableValidationLayers)
    {
        createInfo.enabledLayerCount = static_cast<uint32_t>(kValidationLayers.size());
        createInfo.ppEnabledLayerNames = kValidationLayers.data();

        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    }

    const VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    std::cout << "Vulkan instance created.\n";
}

void VulkanContext::createDebugMessenger()
{
    if constexpr (!kEnableValidationLayers)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    populateDebugMessengerCreateInfo(createInfo);

    const VkResult result =
        createDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan debug messenger");
    }

    std::cout << "Debug messenger created.\n";
}

void VulkanContext::createSurface(const Window& window)
{
    const VkResult result =
        glfwCreateWindowSurface(m_instance, window.getNativeHandle(), nullptr, &m_surface);

    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create GLFW Vulkan surface");
    }

    std::cout << "Vulkan surface created.\n";
}

void VulkanContext::pickPhysicalDevice()
{
    uint32_t deviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to enumerate Vulkan physical devices");
    }

    if (deviceCount == 0)
    {
        throw std::runtime_error("No Vulkan-capable physical devices found");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to enumerate Vulkan physical devices");
    }

    std::cout << "\nFound " << deviceCount << " Vulkan physical device(s).\n";

    int bestPreference = 0;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
    QueueFamilyIndices bestIndices;

    for (uint32_t i = 0; i < deviceCount; ++i)
    {
        VkPhysicalDeviceProperties properties = {};
        vkGetPhysicalDeviceProperties(devices[i], &properties);

        std::cout << "\nGPU " << i << ":\n";
        std::cout << "  " << properties.deviceName << '\n';
        std::cout << "  type: " << physicalDeviceTypeToString(properties.deviceType) << '\n';
        std::cout << "  Vulkan API: " << vulkanVersionToString(properties.apiVersion) << '\n';
        std::cout << "  driver version: " << properties.driverVersion << '\n';

        const bool extensionsSupported = checkDeviceExtensionSupport(devices[i]);
        const bool dynamicRenderingSupported = supportsDynamicRendering(devices[i]);
        const bool suitable = isDeviceSuitable(devices[i]);
        QueueFamilyIndices indices = findQueueFamilies(devices[i], true);

        std::cout << "  required device extensions: "
            << (extensionsSupported ? "yes" : "no") << '\n';
        std::cout << "  dynamic rendering: "
            << (dynamicRenderingSupported ? "yes" : "no") << '\n';

        if (suitable)
        {
            const int preference = physicalDevicePreference(properties.deviceType);
            if (preference > bestPreference)
            {
                bestPreference = preference;
                bestDevice = devices[i];
                bestIndices = indices;
            }
        }
    }

    if (bestDevice == VK_NULL_HANDLE)
    {
        throw std::runtime_error(
            "No suitable Vulkan physical device found with graphics, presentation, and swapchain support");
    }

    m_physicalDevice = bestDevice;
    m_queueFamilyIndices = bestIndices;

    VkPhysicalDeviceProperties selectedProperties = {};
    vkGetPhysicalDeviceProperties(m_physicalDevice, &selectedProperties);

    std::cout << "\nSelected physical device:\n";
    std::cout << "  " << selectedProperties.deviceName << '\n';
    std::cout << "Graphics queue family: " << *m_queueFamilyIndices.graphicsFamily << '\n';
    std::cout << "Present queue family: " << *m_queueFamilyIndices.presentFamily << '\n';

    const SwapchainSupportDetails support = querySwapchainSupport(m_physicalDevice);
    std::cout << "\nSelected device swapchain support:\n";
    std::cout << "  surface formats: " << support.formats.size() << '\n';
    std::cout << "  present modes: " << support.presentModes.size() << '\n';
    std::cout << "  minimum image count: " << support.capabilities.minImageCount << '\n';
    std::cout << "  maximum image count: " << support.capabilities.maxImageCount << '\n';
    std::cout << "  current extent: "
        << support.capabilities.currentExtent.width << " x "
        << support.capabilities.currentExtent.height << '\n';
    std::cout << "  minimum extent: "
        << support.capabilities.minImageExtent.width << " x "
        << support.capabilities.minImageExtent.height << '\n';
    std::cout << "  maximum extent: "
        << support.capabilities.maxImageExtent.width << " x "
        << support.capabilities.maxImageExtent.height << '\n';
    std::cout << "  supported present modes:\n";
    for (VkPresentModeKHR presentMode : support.presentModes)
    {
        std::cout << "    " << presentModeToString(presentMode) << '\n';
    }
}

void VulkanContext::createLogicalDevice()
{
    if (!m_queueFamilyIndices.isComplete())
    {
        throw std::runtime_error("Cannot create Vulkan logical device without complete queue families");
    }

    if (!checkDeviceExtensionSupport(m_physicalDevice))
    {
        throw std::runtime_error("Selected Vulkan physical device does not support VK_KHR_swapchain");
    }

    if (!supportsDynamicRendering(m_physicalDevice))
    {
        throw std::runtime_error("Selected Vulkan physical device does not support dynamic rendering");
    }

    std::cout << "\nCreating logical device...\n";
    std::cout << "Enabled device extensions:\n";
    for (const char* extension : kDeviceExtensions)
    {
        std::cout << "  " << extension << '\n';
    }

    const std::set<uint32_t> uniqueQueueFamilies = {
        *m_queueFamilyIndices.graphicsFamily,
        *m_queueFamilyIndices.presentFamily
    };

    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());

    for (uint32_t queueFamilyIndex : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo = {};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {};
    dynamicRenderingFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
    dynamicRenderingFeatures.dynamicRendering = VK_TRUE;

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pNext = &dynamicRenderingFeatures;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(kDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = kDeviceExtensions.data();

    const VkResult result = vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan logical device");
    }

    std::cout << "Logical device created.\n";
    std::cout << "Dynamic rendering enabled.\n";

    vkGetDeviceQueue(m_device, *m_queueFamilyIndices.graphicsFamily, 0, &m_graphicsQueue);
    if (m_graphicsQueue == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to acquire Vulkan graphics queue");
    }

    std::cout << "Graphics queue acquired.\n";

    vkGetDeviceQueue(m_device, *m_queueFamilyIndices.presentFamily, 0, &m_presentQueue);
    if (m_presentQueue == VK_NULL_HANDLE)
    {
        throw std::runtime_error("Failed to acquire Vulkan present queue");
    }

    std::cout << "Present queue acquired.\n";

    if (m_queueFamilyIndices.graphicsFamily == m_queueFamilyIndices.presentFamily)
    {
        std::cout << "Graphics and present use the same queue family.\n";
    }
    else
    {
        std::cout << "Graphics and present use separate queue families.\n";
    }
}

void VulkanContext::createSwapchain(const Window& window)
{
    std::cout << "\nQuerying swapchain support...\n";

    const SwapchainSupportDetails support = querySwapchainSupport(m_physicalDevice);
    const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(support.formats);
    const VkPresentModeKHR presentMode = chooseSwapPresentMode(support.presentModes);
    const VkExtent2D extent = chooseSwapExtent(support.capabilities, window);

    uint32_t imageCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0 && imageCount > support.capabilities.maxImageCount)
    {
        imageCount = support.capabilities.maxImageCount;
    }

    std::cout << "Chosen surface format:\n";
    std::cout << "  " << surfaceFormatToString(surfaceFormat.format) << '\n';
    std::cout << "Chosen color space:\n";
    std::cout << "  " << colorSpaceToString(surfaceFormat.colorSpace) << '\n';
    std::cout << "Chosen present mode:\n";
    std::cout << "  " << presentModeToString(presentMode) << '\n';
    std::cout << "Chosen swapchain extent:\n";
    std::cout << "  " << extent.width << " x " << extent.height << '\n';
    std::cout << "Requested swapchain image count:\n";
    std::cout << "  " << imageCount << '\n';

    if ((support.capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0)
    {
        throw std::runtime_error(
            "Selected Vulkan surface does not support color-attachment swapchain images");
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    const std::array<uint32_t, 2> queueFamilyIndices = {
        *m_queueFamilyIndices.graphicsFamily,
        *m_queueFamilyIndices.presentFamily
    };

    if (m_queueFamilyIndices.graphicsFamily != m_queueFamilyIndices.presentFamily)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = static_cast<uint32_t>(queueFamilyIndices.size());
        createInfo.pQueueFamilyIndices = queueFamilyIndices.data();
        std::cout << "Swapchain sharing mode: CONCURRENT\n";
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        std::cout << "Swapchain sharing mode: EXCLUSIVE\n";
    }

    createInfo.preTransform = support.capabilities.currentTransform;
    createInfo.compositeAlpha = chooseCompositeAlpha(support.capabilities.supportedCompositeAlpha);
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    const VkResult result = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapchain);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to create Vulkan swapchain");
    }

    std::cout << "Swapchain created.\n";

    VkResult imageResult = vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr);
    if (imageResult != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query Vulkan swapchain image count");
    }

    m_swapchainImages.resize(imageCount);
    imageResult = vkGetSwapchainImagesKHR(
        m_device,
        m_swapchain,
        &imageCount,
        m_swapchainImages.data());
    if (imageResult != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to retrieve Vulkan swapchain images");
    }

    m_swapchainImageFormat = surfaceFormat.format;
    m_swapchainExtent = extent;
    m_swapchainImageLayouts.assign(m_swapchainImages.size(), VK_IMAGE_LAYOUT_UNDEFINED);

    std::cout << "Retrieved " << m_swapchainImages.size() << " swapchain images.\n";
    std::cout << "Swapchain image usage: COLOR_ATTACHMENT.\n";
}

void VulkanContext::createSwapchainImageViews()
{
    m_swapchainImageViews.resize(m_swapchainImages.size());

    for (size_t i = 0; i < m_swapchainImages.size(); ++i)
    {
        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_swapchainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_swapchainImageFormat;
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        const VkResult result =
            vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create Vulkan swapchain image view");
        }
    }

    std::cout << "Created " << m_swapchainImageViews.size() << " swapchain image views.\n";
}

QueueFamilyIndices VulkanContext::findQueueFamilies(VkPhysicalDevice device, bool logQueues) const
{
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; ++i)
    {
        const VkQueueFamilyProperties& queueFamily = queueFamilies[i];

        VkBool32 presentSupport = VK_FALSE;
        const VkResult result =
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to query Vulkan physical device presentation support");
        }

        const bool graphicsSupport = (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
        const bool computeSupport = (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT) != 0;
        const bool transferSupport = (queueFamily.queueFlags & VK_QUEUE_TRANSFER_BIT) != 0;

        if (logQueues)
        {
            std::cout << "\n  queue family " << i << ":\n";
            std::cout << "    count: " << queueFamily.queueCount << '\n';
            std::cout << "    graphics: " << (graphicsSupport ? "yes" : "no") << '\n';
            std::cout << "    compute: " << (computeSupport ? "yes" : "no") << '\n';
            std::cout << "    transfer: " << (transferSupport ? "yes" : "no") << '\n';
            std::cout << "    present: " << (presentSupport == VK_TRUE ? "yes" : "no") << '\n';
        }

        if (graphicsSupport && !indices.graphicsFamily.has_value())
        {
            indices.graphicsFamily = i;
        }

        if (presentSupport == VK_TRUE && !indices.presentFamily.has_value())
        {
            indices.presentFamily = i;
        }
    }

    return indices;
}

bool VulkanContext::isDeviceSuitable(VkPhysicalDevice device) const
{
    const bool extensionsSupported = checkDeviceExtensionSupport(device);
    const bool dynamicRenderingSupported = supportsDynamicRendering(device);
    bool swapchainAdequate = false;

    if (extensionsSupported)
    {
        const SwapchainSupportDetails support = querySwapchainSupport(device);
        swapchainAdequate = !support.formats.empty() && !support.presentModes.empty();
    }

    return findQueueFamilies(device).isComplete() &&
        extensionsSupported &&
        dynamicRenderingSupported &&
        swapchainAdequate;
}

bool VulkanContext::checkDeviceExtensionSupport(VkPhysicalDevice device) const
{
    uint32_t extensionCount = 0;
    VkResult result = vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to enumerate Vulkan device extensions");
    }

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    result = vkEnumerateDeviceExtensionProperties(
        device,
        nullptr,
        &extensionCount,
        availableExtensions.data());
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to enumerate Vulkan device extensions");
    }

    return std::all_of(
        kDeviceExtensions.begin(),
        kDeviceExtensions.end(),
        [&availableExtensions](const char* requiredExtension)
        {
            return std::any_of(
                availableExtensions.begin(),
                availableExtensions.end(),
                [requiredExtension](const VkExtensionProperties& extension)
                {
                    return std::strcmp(requiredExtension, extension.extensionName) == 0;
                });
        });
}

bool VulkanContext::supportsDynamicRendering(VkPhysicalDevice device) const
{
    VkPhysicalDeviceProperties properties = {};
    vkGetPhysicalDeviceProperties(device, &properties);
    if (properties.apiVersion < VK_API_VERSION_1_3)
    {
        return false;
    }

    VkPhysicalDeviceDynamicRenderingFeatures dynamicRenderingFeatures = {};
    dynamicRenderingFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;

    VkPhysicalDeviceFeatures2 features = {};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &dynamicRenderingFeatures;

    vkGetPhysicalDeviceFeatures2(device, &features);

    return dynamicRenderingFeatures.dynamicRendering == VK_TRUE;
}

SwapchainSupportDetails VulkanContext::querySwapchainSupport(VkPhysicalDevice device) const
{
    SwapchainSupportDetails details;

    VkResult result =
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query Vulkan surface capabilities");
    }

    uint32_t formatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query Vulkan surface format count");
    }

    if (formatCount > 0)
    {
        details.formats.resize(formatCount);
        result = vkGetPhysicalDeviceSurfaceFormatsKHR(
            device,
            m_surface,
            &formatCount,
            details.formats.data());
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to query Vulkan surface formats");
        }
    }

    uint32_t presentModeCount = 0;
    result = vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (result != VK_SUCCESS)
    {
        throw std::runtime_error("Failed to query Vulkan surface present mode count");
    }

    if (presentModeCount > 0)
    {
        details.presentModes.resize(presentModeCount);
        result = vkGetPhysicalDeviceSurfacePresentModesKHR(
            device,
            m_surface,
            &presentModeCount,
            details.presentModes.data());
        if (result != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to query Vulkan surface present modes");
        }
    }

    return details;
}

VkSurfaceFormatKHR VulkanContext::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& formats) const
{
    if (formats.empty())
    {
        throw std::runtime_error("Cannot choose Vulkan swapchain surface format from an empty list");
    }

    const auto preferred = std::find_if(
        formats.begin(),
        formats.end(),
        [](const VkSurfaceFormatKHR& format)
        {
            return format.format == VK_FORMAT_B8G8R8A8_SRGB &&
                format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });

    if (preferred != formats.end())
    {
        return *preferred;
    }

    return formats.front();
}

VkPresentModeKHR VulkanContext::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& presentModes) const
{
    const auto preferred = std::find(
        presentModes.begin(),
        presentModes.end(),
        VK_PRESENT_MODE_MAILBOX_KHR);

    if (preferred != presentModes.end())
    {
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanContext::chooseSwapExtent(
    const VkSurfaceCapabilitiesKHR& capabilities,
    const Window& window) const
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    const auto [framebufferWidth, framebufferHeight] = window.getFramebufferSize();

    VkExtent2D actualExtent = {
        static_cast<uint32_t>(std::max(framebufferWidth, 0)),
        static_cast<uint32_t>(std::max(framebufferHeight, 0))
    };

    actualExtent.width = std::clamp(
        actualExtent.width,
        capabilities.minImageExtent.width,
        capabilities.maxImageExtent.width);
    actualExtent.height = std::clamp(
        actualExtent.height,
        capabilities.minImageExtent.height,
        capabilities.maxImageExtent.height);

    return actualExtent;
}

VkCompositeAlphaFlagBitsKHR VulkanContext::chooseCompositeAlpha(
    VkCompositeAlphaFlagsKHR supportedCompositeAlpha) const
{
    constexpr std::array<VkCompositeAlphaFlagBitsKHR, 4> kPreferredCompositeAlpha = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };

    for (VkCompositeAlphaFlagBitsKHR compositeAlpha : kPreferredCompositeAlpha)
    {
        if ((supportedCompositeAlpha & compositeAlpha) != 0)
        {
            return compositeAlpha;
        }
    }

    throw std::runtime_error("No supported Vulkan swapchain composite alpha mode found");
}

}
