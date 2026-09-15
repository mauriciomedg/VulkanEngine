#include "VulkanContext.h"

#include "Window.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
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
}

VulkanContext::~VulkanContext()
{
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
        const bool suitable = isDeviceSuitable(devices[i]);
        QueueFamilyIndices indices = findQueueFamilies(devices[i], true);

        std::cout << "  required device extensions: "
            << (extensionsSupported ? "yes" : "no") << '\n';

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

    VkDeviceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
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
    return findQueueFamilies(device).isComplete() && checkDeviceExtensionSupport(device);
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

}
