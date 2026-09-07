#include "VulkanContext.h"

#include "Window.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>
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
}

VulkanContext::~VulkanContext()
{
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

}
