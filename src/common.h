
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_LEFT_HANDED
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <array>
#include <vector>
#include <string>
#include <queue>
#include <tuple>
#include <any>
#include <mutex>
#include <random>
#include <execution>
#include <map>
#include <set>
#include <unordered_set>
#include <limits>
#include <algorithm>
#include <optional>
#include <filesystem>
#include <memory>

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_MODULE_H
#include FT_OTSVG_H
#include <plutosvg/plutosvg-ft.h>

#include <raqm.h>
#include "stb/stb_image.h"
#include "stb/stb_rect_pack.h"

double millis();
std::string timestamp();

#define LOG_FATAL(message) throw std::runtime_error("\033[1;95m[" + timestamp() + "][Runtime Error]: " + (std::string)message + "\033[0m");

#define LOG_ERROR(message) std::cout << "\033[1;31m[" + timestamp() + "][Error]: " << message << "\033[0m" << std::endl;

#define LOG_WARN(message) std::cout << "\033[1;33m[" + timestamp() + "][Warn]: " << message << "\033[0m" << std::endl;

#define LOG_DEBUG(message) std::cout << "\033[1;32m[" + timestamp() + "][Debug]: " << message << "\033[0m" << std::endl;

#define LOG_INFO(message) std::cout << "\033[1;34m[" + timestamp() + "][Info]: " << message << "\033[0m" << std::endl;

#define LOG_CIRC(message) std::cout << "[" + timestamp() + "]: " << message << std::endl;

#define MEASURETIME(message, func)\
{\
    uint64_t t0 = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();\
    func;\
    uint64_t t1 = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();\
    LOG_INFO("\n# " << message << "\n# TIME: " << t1 - t0 << "ns\n");\
}

namespace settings
{
    const uint32_t API_VERSION = VK_API_VERSION_1_4;

    const std::vector<const char *> validationLayers = {
        "VK_LAYER_KHRONOS_validation",
    };

    const std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
        VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME,
        VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME,
        VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME,
        VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
        VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME
    };

    const std::vector<const char *> instanceExtensions = {
        VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME,
        VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME,
        VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME
    };

    const std::vector<VkPresentModeKHR> possible_pModes = {
        VK_PRESENT_MODE_IMMEDIATE_KHR,
        VK_PRESENT_MODE_MAILBOX_KHR,
        VK_PRESENT_MODE_FIFO_KHR
    };

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData)
    {
        if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
        {
            LOG_ERROR("validation layer: " << pCallbackData->pMessage);
        }
        else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
        {
            LOG_WARN("validation layer: " << pCallbackData->pMessage);
        }
        else if (messageSeverity == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT && messageType != VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
        {
            LOG_DEBUG("validation layer: " << pCallbackData->pMessage);
        }
        
        return VK_FALSE;
    }

    const bool multisampling = true;
    const bool validationLayer = true;
    const bool debugLayer = true;
    const unsigned int maxFramesInFlight = 3;
    const float antialiasing = 0.25f; // 25%

    const unsigned int width = 1440;
    const unsigned int height = 810;

    const unsigned int atlas_imageWidth = 4096;
    const unsigned int atlas_imageHeight = 4096;
    const unsigned int atlas_texture_Padding = 8;
    const int fontSizes[] = { 16, 24, 32, 48, 64, 128, -1 };
};

// Map a float value from one range to another
float mapRange(float value, float inMin, float inMax, float outMin, float outMax);

std::vector<char> readFile(const std::string &filename);

std::u32string trim(const std::u32string& str);
std::u32string trim(std::u32string_view str);
std::vector<std::u32string> split_on_ascii(std::u32string_view str);
std::vector<std::u32string> split_u32string_on_newline_andtrim(const std::u32string& str);

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo, const VkAllocationCallbacks *pAllocator, VkDebugUtilsMessengerEXT *pDebugMessenger);
void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks *pAllocator);
void CmdSetPolygonModeEXT(VkInstance instance, VkCommandBuffer commandBuffer, VkPolygonMode polygonMode);

VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool commandPool);

void endSingleTimeCommands(VkDevice device, VkQueue graphicsQueue, VkCommandBuffer commandBuffer, VkCommandPool commandPool);

void transitionImageLayout(uint32_t baseMip, uint32_t mipCount, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout, VkDevice device, VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuffer = VK_NULL_HANDLE);

void copy_image_to_image(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize, VkFilter filter);

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, VkDevice device, VkQueue queue, VkCommandPool commandPool, VkCommandBuffer commandBuff = VK_NULL_HANDLE);

// not thread safe
extern class _random 
{
    std::random_device  rand_dev;
    std::mt19937        generator;

public:
    _random() : generator(rand_dev()) {}

    int getRangeInt(int start, int end)
    {
        return std::uniform_int_distribution<int>(start, end)(generator);
    }

    float getRangeFloat(float start, float end)
    {
        return std::uniform_real_distribution<float>(start, end)(generator);
    }

    template <glm::length_t L> glm::vec<L, float> getRangeVec(glm::vec<L, float> start, glm::vec<L, float> end)
    {
        glm::vec<L, float> ret;

        for (int i = 0; i < L; i++)
        {
            ret[i] = getRangeFloat(start[i], end[i]);
        }

        return ret;
    }

} RandomGenerator;

struct GlyphDescriptor
{
    glm::vec2 minUV;
    glm::vec2 maxUV;
    glm::ivec2 size;
    glm::ivec2 bearing;
    int BitmapIndex;
};

struct Texture
{
    uint32_t mipLevels;
    VkImage image;
    VmaAllocation ImageMemory;
    VkImageLayout layout;
    VkImageView ImgView;
    VkSampler sampler;
    VkExtent2D size;
    VkFormat format;
    std::string path = ""; // only for debug
};

struct Font
{
    std::unordered_map<char32_t, GlyphDescriptor> glyphDescriptions;
    std::string name;
    FT_Face fontFace;
    uint32_t fontSize;
};

struct TextureAtlas
{
    Texture texture;
    bool full = false;
    u_char * buffer;
    stbrp_context packCtx;
    std::vector<stbrp_node> nodes;
};

struct VertexInputBindingelement
{
    VkFormat type;
    uint32_t count;
    uint32_t sizeInBytes;
};

struct UniformLayoutBindingelement
{
    uint32_t descriptorCount;
    VkDescriptorType type;
    VkShaderStageFlags stage;
};


class VulkanHandler
{
    public:
        VkInstance instance                                 = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger             = VK_NULL_HANDLE;

        VkPhysicalDevice physicalDevice                     = VK_NULL_HANDLE;
        VkDevice device                                     = VK_NULL_HANDLE;

        VkSampleCountFlagBits msaaSamples                   = VK_SAMPLE_COUNT_1_BIT;

        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;
        std::optional<uint32_t> computeFamily;

        VmaAllocator allocator;

        void init()
        {
            createInstance();
            setupDebugMessenger();
            LOG_DEBUG("VK INITIALIZED");
            pickPhysicalDevice();
            createLogicalDevice();
            initVMA();
        }

        void getQueues(VkQueue * graphics, VkQueue * present, VkQueue * compute)
        {
            if (graphics) vkGetDeviceQueue(device, graphicsFamily.value(), queueIdx, graphics);
            if (present) vkGetDeviceQueue(device, presentFamily.value(), queueIdx, present);

            // so that compute is diffrent than graphics and present
            if (compute) vkGetDeviceQueue(device, computeFamily.value(), (queueIdx + 1) % queueCount, compute);

            //if (queueIdx + 1 < queueCount)
            //    queueIdx++;
            //else
            //    LOG_ERROR("No more Queue to give");
        }

        void destroy()
        {
            vmaDestroyAllocator(allocator);
            
            if (settings::validationLayer)
                DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            
            vkDestroyDevice(device, nullptr);
            vkDestroyInstance(instance, nullptr);
        }
    private:
        int queueIdx = 0;
        int queueCount = -1;

        void createInstance()
        {
            // check Validation layer support
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            for (const char *layerName : settings::validationLayers)
            {
                bool layerFound = false;

                for (const auto &layerProperties : availableLayers)
                {
                    if (strcmp(layerName, layerProperties.layerName) == 0)
                    {
                        layerFound = true;
                        break;
                    }
                }

                if (!layerFound)
                {
                    LOG_FATAL("validation layers requested, but not available!");
                }
            }

            VkApplicationInfo appInfo{};
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = NULL;
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = NULL;
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = settings::API_VERSION;

            std::vector<const char *> extensions = {};

            {
                uint32_t ExtensionCount = 0;
                const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&ExtensionCount);
                extensions.resize(ExtensionCount);
                memcpy(extensions.data(), glfwExtensions, sizeof(const char *) * ExtensionCount);

                if (settings::validationLayer)
                {
                    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                    extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
                }

                for (const char *extName : settings::instanceExtensions)
                    extensions.push_back(extName);
            }

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
            VkValidationFeatureEnableEXT enables[] = {
                VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT,
                VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT, 
                VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT
            };
            
            VkValidationFeaturesEXT features = {};
            
            if (settings::validationLayer)
            {
                createInfo.enabledLayerCount = static_cast<uint32_t>(settings::validationLayers.size());
                createInfo.ppEnabledLayerNames = settings::validationLayers.data();


                features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
                features.enabledValidationFeatureCount = sizeof(enables) / sizeof(VkValidationFeatureEnableEXT);
                features.pEnabledValidationFeatures = enables;

                debugCreateInfo = {};
                debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
                debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
                debugCreateInfo.pfnUserCallback = settings::debugCallback;
                debugCreateInfo.pNext = &features;

                createInfo.pNext = &debugCreateInfo;
            }
            else
            {
                createInfo.enabledLayerCount = 0;
                createInfo.pNext = nullptr;
            }


            if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
                LOG_FATAL("failed to create instance!");
        }

        void setupDebugMessenger()
        {
            if (!settings::validationLayer)
                return;

            VkDebugUtilsMessengerCreateInfoEXT createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = settings::debugCallback;

            if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS)
                LOG_FATAL("failed to set up debug messenger!");
        }

        void pickPhysicalDevice()
        {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

            if (deviceCount == 0)
                LOG_FATAL("No graphics device found !");

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

            // Use an ordered map to automatically sort candidates by increasing score
            std::multimap<int, VkPhysicalDevice> candidates;

            for (const auto &device : devices)
            {
                int score = _rateDeviceSuitability(device);
                candidates.insert(std::make_pair(score, device));
            }

            // Check if the best candidate is suitable at all
            if (candidates.rbegin()->first > 0)
            {
                VkPhysicalDeviceProperties deviceProperties;
                vkGetPhysicalDeviceProperties(candidates.rbegin()->second, &deviceProperties);

                LOG_DEBUG("PICKED DEVICE: " << deviceProperties.deviceName);
                physicalDevice = candidates.rbegin()->second;
                msaaSamples = (settings::multisampling) ? _getMaxUsableSampleCount(physicalDevice) : VK_SAMPLE_COUNT_1_BIT;
            }
            else
            {
                LOG_FATAL("failed to find a suitable GPU!");
            }
        }

        void createLogicalDevice()
        {
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

            int i = 0;

            for (const auto &queueFamily : queueFamilies)
            {
                if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    graphicsFamily = i;

                    if (queueCount < 0)
                        queueCount = queueFamily.queueCount;
                    else if (queueFamily.queueCount < queueCount)
                        queueCount = queueFamily.queueCount;    
                }

                if (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)
                {
                    computeFamily = i;

                    if (queueCount < 0)
                        queueCount = queueFamily.queueCount;
                    else if (queueFamily.queueCount < queueCount)
                        queueCount = queueFamily.queueCount;    
                }

                VkBool32 presentSupport = glfwGetPhysicalDevicePresentationSupport(instance, physicalDevice, i);
                
                if (presentSupport)
                {
                    presentFamily = i;
                    if (queueCount < 0)
                        queueCount = queueFamily.queueCount;
                    else if (queueFamily.queueCount < queueCount)
                        queueCount = queueFamily.queueCount;
                }

                if (graphicsFamily.has_value() && presentFamily.has_value() && computeFamily.has_value())
                    break;

                i++;
            }

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::set<uint32_t> uniqueQueueFamilies = {graphicsFamily.value(), presentFamily.value(), computeFamily.value()};

            std::vector<float> queuePriority;
            queuePriority.resize(queueCount, 1.f);

            for (uint32_t queueFamily : uniqueQueueFamilies)
            {
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = queueFamily;
                queueCreateInfo.queueCount = queueCount;
                queueCreateInfo.pQueuePriorities = queuePriority.data();
                queueCreateInfos.push_back(queueCreateInfo);
            }

            VkPhysicalDeviceFeatures deviceFeatures{};
            deviceFeatures.fillModeNonSolid = true;
            deviceFeatures.samplerAnisotropy = true;
            deviceFeatures.sampleRateShading = VK_TRUE; 
            deviceFeatures.logicOp = VK_TRUE;
            deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
            
            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

            createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            createInfo.pQueueCreateInfos = queueCreateInfos.data();
            createInfo.pEnabledFeatures = &deviceFeatures;

            createInfo.enabledExtensionCount = static_cast<uint32_t>(settings::deviceExtensions.size());
            createInfo.ppEnabledExtensionNames = settings::deviceExtensions.data();

            VkPhysicalDeviceVulkan13Features v13 {};
            v13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
            v13.dynamicRendering = VK_TRUE;
            v13.synchronization2 = VK_TRUE;

            VkPhysicalDeviceBufferDeviceAddressFeatures ba{};
            ba.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            ba.bufferDeviceAddress = VK_TRUE;
            ba.pNext = &v13;
        
            VkPhysicalDeviceSwapchainMaintenance1FeaturesKHR dd{};
            dd.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_KHR;
            dd.swapchainMaintenance1 = VK_TRUE;
            dd.pNext = &ba;

            VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
            indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
            indexingFeatures.runtimeDescriptorArray = VK_TRUE;
            indexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
            indexingFeatures.pNext = &dd;
            
            createInfo.pNext = &indexingFeatures;
            createInfo.enabledLayerCount = 0;

            if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
                LOG_FATAL("failed to create logical device!");
        }

        void initVMA()
        {
            VmaAllocatorCreateInfo cI{};
            cI.device = device;
            cI.instance = instance;
            cI.pAllocationCallbacks = nullptr;
            cI.pDeviceMemoryCallbacks = nullptr;
            cI.pHeapSizeLimit = nullptr;
            cI.physicalDevice = physicalDevice;
            cI.preferredLargeHeapBlockSize = 0;
            cI.pTypeExternalMemoryHandleTypes = nullptr;
            cI.pVulkanFunctions = nullptr;
            cI.vulkanApiVersion = settings::API_VERSION;
            cI.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

            vmaCreateAllocator(&cI, &allocator);
        }

        int _rateDeviceSuitability(VkPhysicalDevice device)
        {
            VkPhysicalDeviceProperties deviceProperties;
            VkPhysicalDeviceFeatures deviceFeatures;
            vkGetPhysicalDeviceProperties(device, &deviceProperties);
            vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
            int score = 0;

            // Discrete GPUs have a significant performance advantage
            if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                score += 10000;
            }

            // Maximum possible size of textures affects graphics quality
            score += deviceProperties.limits.maxImageDimension2D;

            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(settings::deviceExtensions.begin(), settings::deviceExtensions.end());

            for (const auto &extension : availableExtensions)
            {
                requiredExtensions.erase(extension.extensionName);
            }

            bool extensionSupport = requiredExtensions.empty();
            bool swapChainAdequate = true;

            if (extensionSupport)
            {
                VkSurfaceKHR surface;
                glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
                GLFWwindow * temp_win = glfwCreateWindow(1, 1, "", NULL, NULL);
                glfwCreateWindowSurface(instance, temp_win, NULL, &surface); 

                VkSurfaceCapabilitiesKHR capabilities;
                std::vector<VkSurfaceFormatKHR> formats;
                 std::vector<VkPresentModeKHR> presentModes;

                vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &capabilities);
                uint32_t formatCount;
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

                if (formatCount != 0)
                {
                    formats.resize(formatCount);
                    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, formats.data());
                }

                uint32_t presentModeCount;
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

                if (presentModeCount != 0)
                {
                    presentModes.resize(presentModeCount);
                    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, presentModes.data());
                }

                swapChainAdequate = !formats.empty() && !presentModes.empty();
                vkDestroySurfaceKHR(instance, surface, nullptr);
                glfwDestroyWindow(temp_win);
            }

            VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures{};
            indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

            VkPhysicalDeviceFeatures2 features2{};
            features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            features2.pNext = &indexingFeatures;

            vkGetPhysicalDeviceFeatures2(device, &features2);

            if (!deviceFeatures.geometryShader || !extensionSupport || !swapChainAdequate ||
                !deviceFeatures.samplerAnisotropy || !indexingFeatures.runtimeDescriptorArray || 
                !indexingFeatures.shaderSampledImageArrayNonUniformIndexing)
            {
                return 0;
            }

            bool presentSupport = false;
            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            for (std::size_t i = 0; i < queueFamilies.size(); i++)
            {
                if (!presentSupport) presentSupport = glfwGetPhysicalDevicePresentationSupport(instance, device, i);                 
            }

            return presentSupport ? score : 0;
        }

        VkSampleCountFlagBits _getMaxUsableSampleCount(VkPhysicalDevice physicalDevice)
        {
            VkPhysicalDeviceProperties physicalDeviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);

            VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
            if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
            if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
            if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
            if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
            if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
            if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
            return VK_SAMPLE_COUNT_1_BIT;
        }
};

struct Window
{
    GLFWwindow * winHandle                              = nullptr;
    VkSwapchainKHR swapchain                            = VK_NULL_HANDLE;
    VkSurfaceKHR surface                                = VK_NULL_HANDLE;

    std::vector<VkImage> swapchainImages                = {};
    VkFormat swapchainImageFormat                       = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent                          = {UINT32_MAX, UINT32_MAX};
    std::vector<VkImageView> swapchainImageViews        = {};

    VkImage depthImage                                  = VK_NULL_HANDLE;
    VmaAllocation depthImageMemory                      = VK_NULL_HANDLE;
    VkImageView depthImageView                          = VK_NULL_HANDLE;

    VkImage colorImage                                  = VK_NULL_HANDLE;
    VmaAllocation colorImageMemory                      = VK_NULL_HANDLE;
    VkImageView colorImageView                          = VK_NULL_HANDLE;
    
    VkCommandPool commandPool                           = VK_NULL_HANDLE;
    VkQueue graphicsQueue                               = VK_NULL_HANDLE;
    VkQueue presentQueue                                = VK_NULL_HANDLE;
    VkQueue computeQueue                                = VK_NULL_HANDLE;
    VulkanHandler * Vk                                  = nullptr;
    int iw, ih                                          = 0;
    int currWinW, currWinH                              = 0;
    bool _quit                                          = false;
    VkFormat depthFormat                                = VK_FORMAT_UNDEFINED;

    std::vector<VkSemaphore> imageAvailableSemaphores   = {}; 
    std::vector<VkSemaphore> renderFinishedSemaphores   = {};
    std::vector<VkFence> inFlightFences                 = {};
    std::vector<VkCommandBuffer> commandBuffers         = {};
    std::vector<VkFence> imagesInFlight                 = {};

    VkImage FB_images           [settings::maxFramesInFlight]   = {};
    VmaAllocation FB_ImageMems  [settings::maxFramesInFlight]   = {};
    VkImageView FB_ImgViews     [settings::maxFramesInFlight]   = {};
    VkSampler FB_sampler                                = VK_NULL_HANDLE;

    struct MouseState
    {
        double x;
        double y;
        std::map<int, int> btnState;
    };

    std::map<int, int> keyboardState                    = {};
    MouseState mouseState                               = {};
    bool quitesc                                        = false;
    int currentFrameIndex                               = 0;
    uint32_t imageIndex                                 = UINT32_MAX;
    uint32_t pModeIdx                                   = UINT32_MAX;
    float frameTime                                     = -1;
    bool recreate_swapchain                             = false;
    float lastFrameTimestamp = 0.0;

    void create(int width, int height, const char* title, bool resizeable, VulkanHandler * Vk, uint32_t pModeIdx, std::string icon = "", bool quitesc = true, int minwidth = GLFW_DONT_CARE, int minheight = GLFW_DONT_CARE, int maxwidth = GLFW_DONT_CARE, int maxheight = GLFW_DONT_CARE)
    {
        this->Vk = Vk; this->quitesc = quitesc; this->pModeIdx = pModeIdx;
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, (resizeable) ? GLFW_TRUE : GLFW_FALSE);
        winHandle = glfwCreateWindow(width, height, title, NULL, NULL);
        glfwCreateWindowSurface(Vk->instance, winHandle, NULL, &surface);
        glfwSetWindowUserPointer(winHandle, this);
        glfwSetWindowSizeLimits(winHandle, minwidth, minheight, maxwidth, maxheight);
        
        GLFWimage icn;
        icn.pixels = stbi_load(icon.c_str(), &icn.width, &icn.height, nullptr, 4);
        glfwSetWindowIcon(winHandle, 1, &icn);
        stbi_image_free(icn.pixels);

        glfwSetKeyCallback(winHandle, [](GLFWwindow* window, int key, int scancode, int action, int mods){
            ((Window*)glfwGetWindowUserPointer(window))->keyboardState[key] = action;
            if (key == GLFW_KEY_ESCAPE && ((Window*)glfwGetWindowUserPointer(window))->quitesc)
                ((Window*)glfwGetWindowUserPointer(window))->quit();
        });

        glfwSetMouseButtonCallback(winHandle, [](GLFWwindow* window, int button, int action, int mods){
            ((Window*)glfwGetWindowUserPointer(window))->mouseState.btnState[button] = action;
        });

        glfwSetFramebufferSizeCallback(winHandle, [](GLFWwindow * window, int w, int h){
            ((Window*)glfwGetWindowUserPointer(window))->currWinW = w;
            ((Window*)glfwGetWindowUserPointer(window))->currWinH = h;
            ((Window*)glfwGetWindowUserPointer(window))->recreate_swapchain = true;
        });

        glfwSetCursorPosCallback(winHandle, [](GLFWwindow* window, double xpos, double ypos){
            ((Window*)glfwGetWindowUserPointer(window))->mouseState.x = xpos;
            ((Window*)glfwGetWindowUserPointer(window))->mouseState.y = ypos;
        });

        iw = width; ih = height;
        currWinW = width; currWinH = height;
        depthFormat = VK_FORMAT_UNDEFINED;

        for (VkFormat format : {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D32_SFLOAT, VK_FORMAT_D24_UNORM_S8_UINT}) {
            VkFormatProperties props;
            vkGetPhysicalDeviceFormatProperties(Vk->physicalDevice, format, &props);

            if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
                depthFormat = format;
            }
        }

        if (depthFormat == VK_FORMAT_UNDEFINED)
            LOG_FATAL("failed to find Depth format!");

        createObjects();    
        LOG_DEBUG("WINDOW CREATED");
    }

    static void resize(GLFWwindow* window, int w, int h)
    {
        ((Window*)glfwGetWindowUserPointer(window))->recreateSwap(w, h);
        LOG_WARN(w << "  " << h);
    }

    void recreateSwap(int w, int h)
    {
        vkDeviceWaitIdle(Vk->device);

        for (int i = 0; i < settings::maxFramesInFlight; i++)
        {
            vkDestroyImageView(Vk->device, FB_ImgViews[i], nullptr);
            vmaDestroyImage(Vk->allocator, FB_images[i], FB_ImageMems[i]);
        }

        for (VkImageView image_view : swapchainImageViews)
        {
            vkDestroyImageView(Vk->device, image_view, nullptr);
        }

        vkDestroyImageView(Vk->device, depthImageView, nullptr);
        vmaDestroyImage(Vk->allocator, depthImage, depthImageMemory);

        vkDestroyImageView(Vk->device, colorImageView, nullptr);
        vmaDestroyImage(Vk->allocator, colorImage, colorImageMemory);

        swapchain = createSwapChain(Vk->physicalDevice, Vk->device, surface, swapchainImages, swapchainImageFormat, swapchainExtent, w, h, swapchain, settings::possible_pModes[pModeIdx]);

        createImageViews();
        createColorResources();
        createDepthResources();
        createFB();
    }

    // only GLFWHandler should call this or else potential double free
    void destroy()
    {
        for (int i = 0; i < settings::maxFramesInFlight; i++)
        {
            vkDestroyImageView(Vk->device, FB_ImgViews[i], nullptr);
            vmaDestroyImage(Vk->allocator, FB_images[i], FB_ImageMems[i]);
        }

        vkDestroySampler(Vk->device, FB_sampler, nullptr);
        vkDestroyCommandPool(Vk->device, commandPool, nullptr);

        for (size_t i = 0; i < renderFinishedSemaphores.size(); i++)
        {
            vkDestroySemaphore(Vk->device, renderFinishedSemaphores[i], nullptr);
        }

        for (size_t i = 0; i < settings::maxFramesInFlight; i++)
        {
            vkDestroySemaphore(Vk->device, imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(Vk->device, inFlightFences[i], nullptr);
        }

        for (VkImageView image_view : swapchainImageViews)
        {
            vkDestroyImageView(Vk->device, image_view, nullptr);
        }

        vkDestroyImageView(Vk->device, depthImageView, nullptr);
        vmaDestroyImage(Vk->allocator, depthImage, depthImageMemory);

        vkDestroyImageView(Vk->device, colorImageView, nullptr);
        vmaDestroyImage(Vk->allocator, colorImage, colorImageMemory);

        vkDestroySwapchainKHR(Vk->device, swapchain, nullptr);
        vkDestroySurfaceKHR(Vk->instance, surface, nullptr);
        LOG_DEBUG("WINDOW DESTROYED");
    }

    void createCommandPool()
    {
        std::optional<uint32_t> graphicsFamily;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(Vk->physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(Vk->physicalDevice, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                graphicsFamily = i;
                break;
            }

            i++;
        }

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsFamily.value();

        if (vkCreateCommandPool(Vk->device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
            LOG_FATAL("failed to create command pool!");
    }

    VkSwapchainKHR createSwapChain(VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface, std::vector<VkImage>& swapChainImages, VkFormat& swapChainImageFormat, VkExtent2D& swapChainExtent, int width, int height, VkSwapchainKHR oldSwapChain, VkPresentModeKHR presentMode)
    {
        struct SwapChainSupportDetails
        {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        } swapChainSupport;
        
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &swapChainSupport.capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

        if (formatCount != 0)
        {
            swapChainSupport.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, swapChainSupport.formats.data());
        }

        uint32_t presentModeCount;
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

        if (presentModeCount != 0)
        {
            swapChainSupport.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, swapChainSupport.presentModes.data());
        }

        VkSurfaceFormatKHR surfaceFormat = swapChainSupport.formats[0];

        for (const auto &availableFormat : swapChainSupport.formats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                surfaceFormat = availableFormat;
            }
        }

        VkPresentModeKHR preMode = presentMode;
        for (const auto &availablePresentMode : swapChainSupport.presentModes)
        {
            if (availablePresentMode == presentMode)
            {
                preMode = availablePresentMode;
            }
        }

        VkExtent2D extent;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &swapChainSupport.capabilities);

        if (swapChainSupport.capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
            extent = swapChainSupport.capabilities.currentExtent;
        }
        else
        {
            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)};

            actualExtent.width = std::clamp(actualExtent.width, swapChainSupport.capabilities.minImageExtent.width, swapChainSupport.capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, swapChainSupport.capabilities.minImageExtent.height, swapChainSupport.capabilities.maxImageExtent.height);

            extent = actualExtent;
        }

        uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

        if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount)
        {
            imageCount = swapChainSupport.capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto &queueFamily : queueFamilies)
        {
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphicsFamily = i;
            
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

            if (presentSupport)
                presentFamily = i;
            
            if (graphicsFamily.has_value() && presentFamily.has_value())
                break;

            i++;
        }

        uint32_t queueFamilyIndices[] = {graphicsFamily.value(), presentFamily.value()};

        if (graphicsFamily != presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            createInfo.queueFamilyIndexCount = 0;     // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = preMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = oldSwapChain;

        VkSwapchainPresentModesCreateInfoKHR pModes{};
        pModes.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_MODES_CREATE_INFO_KHR;
        pModes.presentModeCount = settings::possible_pModes.size();
        pModes.pPresentModes = settings::possible_pModes.data();
        createInfo.pNext = &pModes;

        VkSwapchainKHR retSwap;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &retSwap) != VK_SUCCESS)
        {
            LOG_FATAL("failed to create swap chain!");
        }

        if (oldSwapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(device, oldSwapChain, nullptr);
        }

        vkGetSwapchainImagesKHR(device, retSwap, &imageCount, nullptr);
        swapChainImages.resize(imageCount);

        vkGetSwapchainImagesKHR(device, retSwap, &imageCount, swapChainImages.data());
        swapChainImageFormat = surfaceFormat.format;
        swapChainExtent = extent;

        return retSwap;
    }

    void quit()
    {
        _quit = true;
    }

    bool wantsToQuit()
    {
        return _quit || glfwWindowShouldClose(winHandle);
    }
    
private:

    void createObjects()
    {
        Vk->getQueues(&graphicsQueue, &presentQueue, &computeQueue);
        createCommandPool();
        swapchain = createSwapChain(Vk->physicalDevice, Vk->device, surface, swapchainImages, swapchainImageFormat, swapchainExtent, iw, ih, VK_NULL_HANDLE, settings::possible_pModes[pModeIdx]);
        
        createImageViews();
        createColorResources();
        createDepthResources();
        createSyncObjects();
        createCommandBuffer();
        createFB();
    }

    void createSyncObjects()
    {
        uint32_t imageCount = swapchainImages.size();

        imageAvailableSemaphores.resize(settings::maxFramesInFlight);
        renderFinishedSemaphores.resize(settings::maxFramesInFlight * imageCount);
        inFlightFences.resize(settings::maxFramesInFlight);
        imagesInFlight.resize(imageCount, VK_NULL_HANDLE);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < settings::maxFramesInFlight; i++)
        {
            for (uint32_t j = 0; j < imageCount; j++)
            {
                vkCreateSemaphore(Vk->device, &semaphoreInfo, nullptr, &renderFinishedSemaphores[i*imageCount + j]);
            }

            vkCreateSemaphore(Vk->device, &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]);
            vkCreateFence(Vk->device, &fenceInfo, nullptr, &inFlightFences[i]);
        }
    }

    void createFB()
    {
        if (!FB_sampler)
        {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = VK_FILTER_LINEAR;
            samplerInfo.minFilter = VK_FILTER_LINEAR;
            samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
            samplerInfo.anisotropyEnable = VK_TRUE;

            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(Vk->physicalDevice, &properties);

            samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
            
            if (vkCreateSampler(Vk->device, &samplerInfo, nullptr, &FB_sampler) != VK_SUCCESS)
                LOG_FATAL("failed to create texture sampler!");
        }

        int texWidth = swapchainExtent.width;
        int texHeight = swapchainExtent.height;
        VkDeviceSize imageSize = texWidth * texHeight * 4;
        
        
        for (int i = 0; i < settings::maxFramesInFlight; i++)
        {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = texWidth;
            imageInfo.extent.height = texHeight;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.format = swapchainImageFormat;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = swapchainImageFormat;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            VmaAllocationCreateInfo cI{};
            cI.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

            VkClearColorValue ccol = {1,1,1,1};

            vmaCreateImage(Vk->allocator, &imageInfo, &cI, &FB_images[i], &FB_ImageMems[i], nullptr);
            viewInfo.image = FB_images[i];

            transitionImageLayout(0, 1, FB_images[i], swapchainImageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL, Vk->device, graphicsQueue, commandPool);

            VkCommandBuffer commandBuffer = beginSingleTimeCommands(Vk->device, commandPool);
            vkCmdClearColorImage(commandBuffer, FB_images[i], VK_IMAGE_LAYOUT_GENERAL, &ccol, 1, &viewInfo.subresourceRange);
            endSingleTimeCommands(Vk->device, graphicsQueue, commandBuffer, commandPool);
            

            if (vkCreateImageView(Vk->device, &viewInfo, nullptr, &FB_ImgViews[i]) != VK_SUCCESS) {
                LOG_FATAL("failed to create texture image view!");
            }
        }
    }

    void createImageViews()
    {
        swapchainImageViews.resize(swapchainImages.size());
        for (size_t i = 0; i < swapchainImages.size(); i++)
        {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = swapchainImageFormat;
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(Vk->device, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS)
            {
                LOG_FATAL("failed to create image views!");
            }
        }
    }

    void createColorResources()
    {
    
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapchainExtent.width;
        imageInfo.extent.height = swapchainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = swapchainImageFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        imageInfo.samples = Vk->msaaSamples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo cI{};
        cI.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vmaCreateImage(Vk->allocator, &imageInfo, &cI, &colorImage, &colorImageMemory, nullptr);

        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = colorImage;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapchainImageFormat;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(Vk->device, &createInfo, nullptr, &colorImageView) != VK_SUCCESS)
        {
            LOG_FATAL("failed to create image views!");
        }

        transitionImageLayout(0, 1, colorImage, swapchainImageFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, Vk->device, graphicsQueue, commandPool);
    }

    void createDepthResources()
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = swapchainExtent.width;
        imageInfo.extent.height = swapchainExtent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = depthFormat;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        imageInfo.samples = Vk->msaaSamples;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo cI{};
        cI.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        vmaCreateImage(Vk->allocator, &imageInfo, &cI, &depthImage, &depthImageMemory, nullptr);
        
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = depthImage;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = depthFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(Vk->device, &createInfo, nullptr, &depthImageView) != VK_SUCCESS)
        {
            LOG_FATAL("failed to create image views!");
        }

        //transitionDepthImageLayout
        VkPipelineStageFlags sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags destinationStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = depthImage;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_NONE;
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        if (depthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT || depthFormat == VK_FORMAT_D24_UNORM_S8_UINT) {
            barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    
        VkCommandBuffer commandBuffer = beginSingleTimeCommands(Vk->device, commandPool);

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        endSingleTimeCommands(Vk->device, graphicsQueue, commandBuffer, commandPool);
    }

    void createCommandBuffer()
    {
        commandBuffers.resize(settings::maxFramesInFlight);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

        if (vkAllocateCommandBuffers(Vk->device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
            LOG_FATAL("failed to allocate command buffers!");
    }
};

void EndRenderPass(Window * win);
void BeginRenderPass(Window * win, VkAttachmentLoadOp load, VkAttachmentStoreOp store, std::array<VkClearValue, 2> clearValues, VkRect2D scissor, VkViewport viewport);

class GPUBuffer
{
    VkBuffer Staging_Buff           = VK_NULL_HANDLE;
    VkBuffer Buff                   = VK_NULL_HANDLE;

    VmaAllocation Staging_allocation = VK_NULL_HANDLE;
    VmaAllocation allocation         = VK_NULL_HANDLE;

    uint32_t BufsizeBytes           = -1;
    void *dataAcces                 = nullptr;
    
    VkQueue queue = VK_NULL_HANDLE;
    VulkanHandler * VKH = nullptr;

    VkCommandBuffer commandBuffer   = VK_NULL_HANDLE;
    VkCommandPool commandPool       = VK_NULL_HANDLE;
    VkBufferUsageFlagBits bufferUsage;
    bool staging = true;


    public:
        void createBuffer(VkBufferUsageFlagBits bufferUsage, uint32_t initial_sizeBytes, VkQueue queue, VulkanHandler * VKH, VkCommandPool commandPool, bool staging = true)
        {
            this->BufsizeBytes = initial_sizeBytes; this->queue = queue; this->VKH = VKH; this->commandPool = commandPool; this->bufferUsage = bufferUsage; this->staging = staging;

            if (BufsizeBytes == 0)
                return;

            if (staging)
            {
                {
                    VkBufferCreateInfo bufferInfo = {};
                    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                    bufferInfo.size = initial_sizeBytes;
                    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
                    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                    VmaAllocationCreateInfo allocInfo = {};
                    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                    allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

                    vmaCreateBuffer(VKH->allocator, &bufferInfo, &allocInfo, &Staging_Buff, &Staging_allocation, nullptr);
                }
                {
                    VkBufferCreateInfo bufferInfo = {};
                    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                    bufferInfo.size = initial_sizeBytes;
                    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | bufferUsage;
                    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                    VmaAllocationCreateInfo allocInfo = {};
                    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

                    vmaCreateBuffer(VKH->allocator, &bufferInfo, &allocInfo, &Buff, &allocation, nullptr);
                }
            } else
            {
                VkBufferCreateInfo bufferInfo = {};
                bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
                bufferInfo.size = initial_sizeBytes;
                bufferInfo.usage = bufferUsage;
                bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

                VmaAllocationCreateInfo allocInfo = {};
                allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
                allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;

                vmaCreateBuffer(VKH->allocator, &bufferInfo, &allocInfo, &Staging_Buff, &Staging_allocation, nullptr);
            }

            vmaMapMemory(VKH->allocator, Staging_allocation, &dataAcces);

            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool = commandPool;
            allocInfo.commandBufferCount = 1;

            vkAllocateCommandBuffers(VKH->device, &allocInfo, &commandBuffer);
        }

        void destroyBuffer()
        {
            if (BufsizeBytes == 0)
                return;
            
            vmaUnmapMemory(VKH->allocator, Staging_allocation);
            vkFreeCommandBuffers(VKH->device, commandPool, 1, &commandBuffer);
            vmaDestroyBuffer(VKH->allocator, Staging_Buff, Staging_allocation);
            
            if (staging)
            {
                vmaDestroyBuffer(VKH->allocator, Buff, allocation);
            }
        }

        void resizeBuffer(uint32_t sizeBytes, bool waitQueue = true)
        {
            if (sizeBytes == 0 || sizeBytes == BufsizeBytes)
                return;

            if (waitQueue)
                vkQueueWaitIdle(queue);

            destroyBuffer();
            createBuffer(bufferUsage, sizeBytes, queue, VKH, commandPool, staging);
        }

        void writeToBuffer(void *BufferData, uint32_t sizeBytes, VkCommandBuffer cmdBfr = VK_NULL_HANDLE)
        {
            if (sizeBytes == 0)
                return;

            // write after write
            if (sizeBytes > this->BufsizeBytes)
            {
                resizeBuffer(sizeBytes, cmdBfr == VK_NULL_HANDLE);
            }

            memcpy(reinterpret_cast<uint8_t*>(dataAcces), BufferData, sizeBytes);

            if (!staging)
                return;

            auto recordCopy = [&](VkCommandBuffer cb)
            {
                // ---- Barrier: ensure previous transfer writes finished ----
                VkBufferMemoryBarrier2 barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
                barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
                barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                barrier.buffer = Buff;
                barrier.offset = 0;
                barrier.size = sizeBytes;

                VkDependencyInfo depInfo{};
                depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                depInfo.bufferMemoryBarrierCount = 1;
                depInfo.pBufferMemoryBarriers = &barrier;

                vkCmdPipelineBarrier2(cb, &depInfo);

                // ---- Copy ----
                VkBufferCopy copyRegion{};
                copyRegion.srcOffset = 0;
                copyRegion.dstOffset = 0;
                copyRegion.size = sizeBytes;

                vkCmdCopyBuffer(cb, Staging_Buff, Buff, 1, &copyRegion);

                VkBufferMemoryBarrier2 postBarrier{};
                postBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
                postBarrier.srcStageMask  = VK_PIPELINE_STAGE_2_COPY_BIT;
                postBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                postBarrier.dstStageMask  = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                postBarrier.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
                postBarrier.buffer = Buff;
                postBarrier.offset = 0;
                postBarrier.size   = sizeBytes;

                VkDependencyInfo depPost{};
                depPost.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
                depPost.bufferMemoryBarrierCount = 1;
                depPost.pBufferMemoryBarriers = &postBarrier;

                vkCmdPipelineBarrier2(cb, &depPost);
            };

            if (cmdBfr == VK_NULL_HANDLE)
            {
                VkCommandBufferBeginInfo beginInfo{};
                beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
                beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

                vkResetCommandBuffer(commandBuffer, 0);
                vkBeginCommandBuffer(commandBuffer, &beginInfo);

                recordCopy(commandBuffer);

                vkEndCommandBuffer(commandBuffer);

                VkSubmitInfo submitInfo{};
                submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                submitInfo.commandBufferCount = 1;
                submitInfo.pCommandBuffers = &commandBuffer;

                vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
                vkQueueWaitIdle(queue);
            }
            else
            {
                recordCopy(cmdBfr);
            }
        }

        void * getdataAccess() { return dataAcces; }

        VkBuffer getHandle() { return (staging) ? Buff : Staging_Buff; }
        uint32_t getByteSize() { return BufsizeBytes; }
        
};

// Vk, graphicsqueue, commandPool

class AssetManager
{
    private:
        std::unordered_map<std::string, Texture> assetMap = {};
        std::unordered_map<std::string, Font> FontMap     = {};
        std::vector<TextureAtlas> Bitmaps       = {};
        VulkanHandler * Vk                = nullptr;
        VkQueue graphicsQueue               = VK_NULL_HANDLE;
        VkCommandPool commandPool   = VK_NULL_HANDLE;
        VkSampler textureSampler    = VK_NULL_HANDLE;
        VkSampler fontSampler       = VK_NULL_HANDLE;
        FT_Library lib              = nullptr;

        void createImage(uint32_t mipLevels, uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VmaAllocation& imageAllocation)
        {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width = width;
            imageInfo.extent.height = height;
            imageInfo.extent.depth = 1;
            imageInfo.mipLevels = mipLevels;
            imageInfo.arrayLayers = 1;
            imageInfo.format = format;
            imageInfo.tiling = tiling;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage = usage;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

            VmaAllocationCreateInfo cI{};
            cI.requiredFlags = properties;

            vmaCreateImage(Vk->allocator, &imageInfo, &cI, &image, &imageAllocation, nullptr);
        }

        void generateMipmaps(VkImage image, VkFormat imageFormat, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, VkCommandBuffer commandBuffer = VK_NULL_HANDLE)
        {
            VkFormatProperties formatProperties;
            vkGetPhysicalDeviceFormatProperties(Vk->physicalDevice, imageFormat, &formatProperties);

            if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
                LOG_FATAL("texture image format does not support linear blitting!");
            }

            bool cmdBuf = commandBuffer == VK_NULL_HANDLE;
            if (cmdBuf) commandBuffer = beginSingleTimeCommands(Vk->device, commandPool);

            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.image = image;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.subresourceRange.levelCount = 1;

            int32_t mipWidth = texWidth;
            int32_t mipHeight = texHeight;

            for (uint32_t i = 1; i < mipLevels; i++) {
                barrier.subresourceRange.baseMipLevel = i - 1;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

                vkCmdPipelineBarrier(commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier
                );

                VkImageBlit blit{};
                blit.srcOffsets[0] = { 0, 0, 0 };
                blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.srcSubresource.mipLevel = i - 1;
                blit.srcSubresource.baseArrayLayer = 0;
                blit.srcSubresource.layerCount = 1;
                blit.dstOffsets[0] = { 0, 0, 0 };
                blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                blit.dstSubresource.mipLevel = i;
                blit.dstSubresource.baseArrayLayer = 0;
                blit.dstSubresource.layerCount = 1;

                vkCmdBlitImage(commandBuffer,
                    image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                    image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    1, &blit,
                    VK_FILTER_LINEAR
                );

                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vkCmdPipelineBarrier(commandBuffer,
                    VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                    0, nullptr,
                    0, nullptr,
                    1, &barrier
                );

                if (mipWidth > 1) mipWidth /= 2;
                if (mipHeight > 1) mipHeight /= 2;
            }
            
            barrier.subresourceRange.baseMipLevel = mipLevels - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            if (cmdBuf) endSingleTimeCommands(Vk->device, graphicsQueue, commandBuffer, commandPool);
        }

        FT_Face createFontFace(std::string fontFilename, int& fontSize)
        {
            FT_Error error;
            FT_Face face;

            // load font
            error = FT_New_Face( lib , fontFilename.c_str() , 0 , &face );
            if ( error )
            {
                std::cout << "BitmapFontGenerator > ERROR: failed to open file \"" << fontFilename << "\", error code: " << FT_Error_String( error ) << std::endl;    
            }

            FT_Select_Charmap(face, FT_ENCODING_UNICODE);
            error = FT_Set_Pixel_Sizes(face, 0, fontSize);

            if (error)
                LOG_ERROR("PBLM")
            
            bool hasColor = FT_HAS_COLOR(face);

            if (face->num_fixed_sizes > 0)
            {
                int best_match = 0;
                int diff = std::abs(fontSize - face->available_sizes[0].width);
                for (int i = 1; i < face->num_fixed_sizes; ++i)\
                {
                    int ndiff = std::abs(fontSize - face->available_sizes[i].width);
                    if (ndiff < diff)
                    {
                        best_match = i;
                        diff = ndiff;
                    }
                }
                
                error = FT_Select_Size(face, best_match);
                
                if ( error )
                    std::cout << "BitmapFontGenerator > failed to set font size, error code: " << FT_Error_String( error ) << std::endl;
            }

            return face;
        }

    public:

        void addGlyphsToAtlas(uint32_t * Glyphs, int GlyphCount, std::unordered_map<char32_t, GlyphDescriptor>& glyphdescs, Font font, VkCommandBuffer commandBuffer = VK_NULL_HANDLE, bool gi = false)
        {
            FT_Error error;
            FT_Set_Pixel_Sizes(font.fontFace, 0, font.fontSize);
            int AtlasIdx = -1;

            for (int i = 0; i < Bitmaps.size(); i++)
            {
                if (!Bitmaps[i].full)
                {
                    AtlasIdx = i;
                    break;
                }
            }

            std::u32string remaining = U"";
             
            if (AtlasIdx < 0)
            {
                AtlasIdx = Bitmaps.size();
                Bitmaps.push_back(TextureAtlas());
                Bitmaps[AtlasIdx].buffer = new u_char[settings::atlas_imageWidth*settings::atlas_imageHeight*4];
                memset( Bitmaps[AtlasIdx].buffer, 0 , settings::atlas_imageWidth*settings::atlas_imageHeight*4 );
                Bitmaps[AtlasIdx].nodes.resize(settings::atlas_imageWidth);

                // initialize packer
                stbrp_init_target(
                    &Bitmaps[AtlasIdx].packCtx,
                    settings::atlas_imageWidth,
                    settings::atlas_imageHeight,
                    Bitmaps[AtlasIdx].nodes.data(),
                    settings::atlas_imageWidth
                );
            }

            // draw all characters
            for (int ia = 0; ia < GlyphCount; ia++)
            {
                char32_t charcode = Glyphs[ia];
                uint32_t glyphIndex = gi ? charcode : FT_Get_Char_Index(font.fontFace, charcode);

                if (glyphdescs.contains(glyphIndex) || !glyphIndex)
                    continue;

                if(!FT_HAS_COLOR(font.fontFace))
                {
                    error = FT_Load_Glyph(font.fontFace, glyphIndex, FT_LOAD_DEFAULT | FT_LOAD_RENDER );
                    error = FT_Render_Glyph(font.fontFace->glyph, FT_RENDER_MODE_NORMAL);
                } else
                {
                    error = FT_Load_Glyph(font.fontFace, glyphIndex, FT_LOAD_COLOR | FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL);
                    //error = FT_Render_Glyph(font.fontFace->glyph, FT_RENDER_MODE_NORMAL);
                }

                if (error)
                    continue;
                
                if (font.fontFace->glyph->bitmap.buffer == nullptr || font.fontFace->glyph->bitmap.width == 0 || font.fontFace->glyph->bitmap.rows == 0)
                    continue;
            

                int gw = font.fontFace->glyph->bitmap.width + 2 * settings::atlas_texture_Padding;
                int gh = font.fontFace->glyph->bitmap.rows  + 2 * settings::atlas_texture_Padding;

                stbrp_rect rect;
                rect.id = 0;
                rect.w  = gw;
                rect.h  = gh;

                if (!stbrp_pack_rects(&Bitmaps[AtlasIdx].packCtx, &rect, 1))
                {
                    remaining = std::u32string(reinterpret_cast<char32_t*>(Glyphs + ia), (std::size_t)GlyphCount - ia);
                    Bitmaps[AtlasIdx].full = true;
                    break;
                }

                // save the character width

                // find the tile position where we have to draw the character
                int x = rect.x + settings::atlas_texture_Padding;
                int y = rect.y + settings::atlas_texture_Padding;

                float uvx0 = (float)(x) / (float)(settings::atlas_imageWidth); 
                float uvy0 = (float)(y) / (float)(settings::atlas_imageHeight); 
                float uvx1 = (float)(x + font.fontFace->glyph->bitmap.width) / (float)(settings::atlas_imageWidth); 
                float uvy1 = (float)(y + font.fontFace->glyph->bitmap.rows) / (float)(settings::atlas_imageHeight); 
                
                glyphdescs[glyphIndex] = {
                    {uvx0, uvy0},
                    {uvx1, uvy1},
                    {font.fontFace->glyph->metrics.width / 64.f, font.fontFace->glyph->metrics.height / 64.f},
                    {font.fontFace->glyph->metrics.horiBearingX / 64.f, -font.fontFace->glyph->metrics.horiBearingY / 64.f},
                    AtlasIdx
                };

                // draw the character
                const FT_Bitmap& bitmap = font.fontFace->glyph->bitmap;

                for ( int xx = 0 ; xx < bitmap.width ; ++xx )
                {
                    for ( int yy = 0 ; yy < bitmap.rows ; ++yy )
                    {
                        int dstX = x + xx;
                        int dstY = y + yy;
                        
                        if (bitmap.pixel_mode == FT_PIXEL_MODE_BGRA)
                        {
                            int src = (yy * bitmap.pitch) + (xx * 4);

                            Bitmaps[AtlasIdx].buffer[(dstY * settings::atlas_imageWidth + dstX) * 4 + 0] = bitmap.buffer[src + 2]; // R
                            Bitmaps[AtlasIdx].buffer[(dstY * settings::atlas_imageWidth + dstX) * 4 + 1] = bitmap.buffer[src + 1]; // G
                            Bitmaps[AtlasIdx].buffer[(dstY * settings::atlas_imageWidth + dstX) * 4 + 2] = bitmap.buffer[src + 0]; // B
                            Bitmaps[AtlasIdx].buffer[(dstY * settings::atlas_imageWidth + dstX) * 4 + 3] = bitmap.buffer[src + 3]; // A
                        }
                        else // grayscale
                        {
                            u_char r = bitmap.buffer[yy * bitmap.pitch + xx];
                            Bitmaps[AtlasIdx].buffer[(dstY * settings::atlas_imageWidth + dstX) * 4 + 0] = 255;
                            Bitmaps[AtlasIdx].buffer[(dstY * settings::atlas_imageWidth + dstX) * 4 + 1] = 255;
                            Bitmaps[AtlasIdx].buffer[(dstY * settings::atlas_imageWidth + dstX) * 4 + 2] = 255;
                            Bitmaps[AtlasIdx].buffer[(dstY * settings::atlas_imageWidth + dstX) * 4 + 3] = r;
                        }
                    }
                }
            }

            if (Bitmaps[AtlasIdx].texture.image == VK_NULL_HANDLE)
            {
                Bitmaps[AtlasIdx].texture = loadGeneral(settings::atlas_imageWidth, settings::atlas_imageHeight, 4, Bitmaps[AtlasIdx].buffer, ("atlas" + std::to_string(AtlasIdx)).c_str(), fontSampler);
            } else
            {            
                updateTexture(Bitmaps[AtlasIdx].texture, Bitmaps[AtlasIdx].buffer, settings::atlas_imageWidth, settings::atlas_imageHeight, 0,0, commandBuffer);
            }
        
            if (!remaining.empty())
            {
                int runs = std::floor(remaining.length() / 10);
                
                for (int i = 0; i < runs; i++)
                {
                    auto b = remaining.substr(10*i, 10);
                    addGlyphsToAtlas(reinterpret_cast<uint32_t*>(b.data()), b.length(), glyphdescs, font, commandBuffer, gi);
                }

                auto b = remaining.substr(10*runs, remaining.length() % 10);
                addGlyphsToAtlas(reinterpret_cast<uint32_t*>(b.data()), b.length(), glyphdescs, font, commandBuffer, gi);   
            }
        }
    
        void init(VulkanHandler * Vk, VkQueue gqueue, VkCommandPool cmdPool)
        {
            this->Vk = Vk; this->graphicsQueue = gqueue; this->commandPool = cmdPool;
            // init freetype
            FT_Error error = FT_Init_FreeType( &lib );

            error = FT_Property_Set(lib, "ot-svg", "svg-hooks", &plutosvg_ft_hooks);
            
            if ( error != FT_Err_Ok )
            {
                std::cout << "BitmapFontGenerator > ERROR: FT_Init_FreeType failed, error code: " << FT_Error_String(error) << std::endl;
            }
        }

        void setupTextureSampler(VkFilter ScalingFilter = VK_FILTER_LINEAR, VkSamplerAddressMode adressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT, bool anisotropic = true)
        {
            VkSamplerCreateInfo samplerInfo{};
            samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
            samplerInfo.magFilter = ScalingFilter;
            samplerInfo.minFilter = ScalingFilter;
            samplerInfo.addressModeU = adressMode;
            samplerInfo.addressModeV = adressMode;
            samplerInfo.addressModeW = adressMode;
            samplerInfo.anisotropyEnable = anisotropic;

            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(Vk->physicalDevice, &properties);

            samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
            samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
            samplerInfo.unnormalizedCoordinates = VK_FALSE;
            samplerInfo.compareEnable = VK_FALSE;
            samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
            samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
            samplerInfo.mipLodBias = 0.0f;
            samplerInfo.minLod = 0.0f;
            samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
            
            if (vkCreateSampler(Vk->device, &samplerInfo, nullptr, &textureSampler) != VK_SUCCESS) {
                LOG_FATAL("failed to create texture sampler!");
            }
        }

        void loadAssetDirectory(std::string directory)
        {
            for (const auto & entry : std::filesystem::directory_iterator(directory))
            {
                std::string file = entry.path();
                int indexDot = file.find_last_of('.');
                if (indexDot > 0 && indexDot < file.size())
                {
                    if (file.substr(indexDot) == ".png" || file.substr(indexDot) == ".ttf")
                        loadAssetFile(entry.path());
                }
            }
        }

        void loadAssetDirectoryRecursive(std::string directory)
        {
            for (const auto & entry : std::filesystem::recursive_directory_iterator(directory))
            {
                std::string file = entry.path();
                int indexDot = file.find_last_of('.');
                if (indexDot > 0 && indexDot < file.size())
                {
                    if (file.substr(indexDot) == ".png" || file.substr(indexDot) == ".ttf")
                        loadAssetFile(entry.path());
                }
            }
        }
   
        int loadAssetFile(std::string filePath)
        {
            if (filePath.substr(filePath.size() - 3) != "ttf")
            {
                std::string key = filePath.substr(filePath.find_last_of('/')+1);
                assetMap[key] = loadGeneral(filePath, textureSampler);
            } else 
            {
                if (fontSampler == VK_NULL_HANDLE)
                {
                    VkSamplerCreateInfo samplerInfo{};
                    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                    samplerInfo.magFilter = VK_FILTER_LINEAR;
                    samplerInfo.minFilter = VK_FILTER_LINEAR;
                    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                    samplerInfo.anisotropyEnable = VK_TRUE;

                    VkPhysicalDeviceProperties properties{};
                    vkGetPhysicalDeviceProperties(Vk->physicalDevice, &properties);

                    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
                    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
                    samplerInfo.unnormalizedCoordinates = VK_FALSE;
                    samplerInfo.compareEnable = VK_FALSE;
                    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
                    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
                    samplerInfo.mipLodBias = 0.0f;
                    samplerInfo.minLod = 0.0f;
                    samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
                    
                    if (vkCreateSampler(Vk->device, &samplerInfo, nullptr, &fontSampler) != VK_SUCCESS)
                        LOG_FATAL("failed to create texture sampler!");
                }

                int i = 0;
                while (settings::fontSizes[i] != -1)
                {
                    int size_request = settings::fontSizes[i];
                    std::string fn = filePath.substr(filePath.find_last_of('/')+1) + std::to_string(size_request);

                    FT_Face fc = createFontFace(filePath, size_request);

                    FontMap[fn] = {{}, fn, fc, (uint32_t)size_request};
                    
                    std::vector<uint32_t> initGlyphs = {U'…', U'-'};
                    for (uint32_t i = 32; i < 127; i++)
                        initGlyphs.push_back(i);

                    addGlyphsToAtlas(initGlyphs.data(), 96, FontMap[fn].glyphDescriptions, FontMap[fn]);
                    FontMap[fn].fontSize = size_request;
                    i++;
                }
            }

            return 0;
        }

        Texture loadGeneral(std::string filePath, VkSampler sampler)
        {
            VkImage textureImage;
            VmaAllocation textureImageMemory;

            int texWidth, texHeight, texChannels;
            stbi_uc* pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
            VkDeviceSize imageSize = texWidth * texHeight * 4;

            uint32_t mipLevels = (uint32_t)(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

            if (!pixels) LOG_FATAL("failed to load texture image!");

            GPUBuffer buffer;
            buffer.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageSize, graphicsQueue, Vk, commandPool, false);
            buffer.writeToBuffer(pixels, imageSize);
            stbi_image_free(pixels);

            createImage(mipLevels, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

            transitionImageLayout(0, mipLevels, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Vk->device, graphicsQueue, commandPool);
            copyBufferToImage(buffer.getHandle(), textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), Vk->device, graphicsQueue, commandPool);
            generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

            VkImageView vkImgView;
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = textureImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = mipLevels;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(Vk->device, &viewInfo, nullptr, &vkImgView) != VK_SUCCESS) {
                LOG_FATAL("failed to create texture image view!");
            }

            buffer.destroyBuffer();
            return (Texture){mipLevels, textureImage, textureImageMemory, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vkImgView, sampler, VkExtent2D{(uint)texWidth, (uint)texHeight}, VK_FORMAT_R8G8B8A8_SRGB, filePath};
        }

        Texture loadGeneral(int width, int height, int channels, u_char * buffer, std::string name, VkSampler sampler)
        {
            VkImage textureImage;
            VmaAllocation textureImageMemory;

            int texWidth = width;
            int texHeight = height;
            int texChannels = channels;
            stbi_uc* pixels = buffer;
            VkDeviceSize imageSize = texWidth * texHeight * 4;

            uint32_t mipLevels = (uint32_t)(std::floor(std::log2(std::max(texWidth, texHeight)))) + 1;

            if (!pixels) LOG_FATAL("failed to load texture image!");

            GPUBuffer buff;
            buff.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageSize, graphicsQueue, Vk, commandPool, false);
            buff.writeToBuffer(pixels, imageSize); 

            createImage(mipLevels, texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

            transitionImageLayout(0, mipLevels, textureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Vk->device, graphicsQueue, commandPool);
            copyBufferToImage(buff.getHandle(), textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), Vk->device, graphicsQueue, commandPool);
            generateMipmaps(textureImage, VK_FORMAT_R8G8B8A8_SRGB, texWidth, texHeight, mipLevels);

            VkImageView vkImgView;
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = textureImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = mipLevels;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(Vk->device, &viewInfo, nullptr, &vkImgView) != VK_SUCCESS) {
                LOG_FATAL("failed to create texture image view!");
            }

            buff.destroyBuffer();
            return (Texture){mipLevels, textureImage, textureImageMemory, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vkImgView, sampler, VkExtent2D{(uint)texWidth, (uint)texHeight}, VK_FORMAT_R8G8B8A8_SRGB, name};
        }

        Texture createStorageImage(int width, int height, VkFormat format, void * buffer, int bufferSizeBytes)
        {
            VkImage textureImage;
            VmaAllocation textureImageMemory;

            int texWidth = width;
            int texHeight = height;

            createImage(1, texWidth, texHeight, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, textureImage, textureImageMemory);

            if (bufferSizeBytes > 0)
            {
                GPUBuffer buff;
                buff.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, bufferSizeBytes, graphicsQueue, Vk, commandPool, false);
                buff.writeToBuffer(buffer, bufferSizeBytes); 
                transitionImageLayout(0, 1, textureImage, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, Vk->device, graphicsQueue, commandPool);          
                copyBufferToImage(buff.getHandle(), textureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), Vk->device, graphicsQueue, commandPool);
                buff.destroyBuffer();
            
                transitionImageLayout(0,1, textureImage, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL, Vk->device, graphicsQueue, commandPool);
            }

            VkImageView vkImgView;
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = textureImage;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            if (vkCreateImageView(Vk->device, &viewInfo, nullptr, &vkImgView) != VK_SUCCESS) {
                LOG_FATAL("failed to create texture image view!");
            }

            return (Texture){1, textureImage, textureImageMemory, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, vkImgView, textureSampler, VkExtent2D{(uint)texWidth, (uint)texHeight}, format, "storage" + std::to_string(assetMap.size())};            
        }

        void destroyTexture(Texture& tex)
        {
            vmaDestroyImage(Vk->allocator, tex.image, tex.ImageMemory);
            vkDestroyImageView(Vk->device, tex.ImgView, nullptr);
        }

        GPUBuffer buff;
        GPUBuffer buf2;

        GPUBuffer mapImageToBuffer(Texture& context, int sizeBytes, VkCommandBuffer commandBuffer = VK_NULL_HANDLE)
        {
            VkDeviceSize imageSize = sizeBytes;

            if (buf2.getHandle() == VK_NULL_HANDLE) buf2.createBuffer(VkBufferUsageFlagBits(VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT), imageSize, graphicsQueue, Vk, commandPool, false);
            else buf2.resizeBuffer(sizeBytes);

            bool cmdBuf = (commandBuffer == VK_NULL_HANDLE);

            if (cmdBuf) commandBuffer = beginSingleTimeCommands(Vk->device, commandPool);
        

            transitionImageLayout(
                0,
                context.mipLevels,
                context.image,
                context.format,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                Vk->device, graphicsQueue,
                commandPool,
                commandBuffer
            );
        
            VkBufferImageCopy copyRegion{};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageOffset = {0,0,0};
            copyRegion.imageExtent = {context.size.width, context.size.height, 1};

            vkCmdCopyImageToBuffer(commandBuffer, context.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, buf2.getHandle(), 1, &copyRegion);


            transitionImageLayout(
                0,
                context.mipLevels,
                context.image,
                context.format,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                Vk->device, graphicsQueue,
                commandPool,
                commandBuffer
            );        
 
            if (cmdBuf) endSingleTimeCommands(Vk->device, graphicsQueue, commandBuffer, commandPool);

            return buf2;
        }

        void unmapImageToBuffer(Texture& context, VkCommandBuffer commandBuffer = VK_NULL_HANDLE)
        {
            bool cmdBuf = (commandBuffer == VK_NULL_HANDLE);

            if (cmdBuf) commandBuffer = beginSingleTimeCommands(Vk->device, commandPool);
            VkBufferImageCopy copyRegion{};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;
            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageOffset = {0,0,0};
            copyRegion.imageExtent = {context.size.width, context.size.height, 1};

            transitionImageLayout(
                0,
                context.mipLevels,
                context.image,
                context.format,
                VK_IMAGE_LAYOUT_GENERAL,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                Vk->device, graphicsQueue,
                commandPool,
                commandBuffer
            );        

            vkCmdCopyBufferToImage(commandBuffer, buf2.getHandle(), context.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

            transitionImageLayout(
                0,
                context.mipLevels,
                context.image,
                context.format,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                VK_IMAGE_LAYOUT_GENERAL,
                Vk->device, graphicsQueue,
                commandPool,
                commandBuffer
            );        

            if (cmdBuf) endSingleTimeCommands(Vk->device, graphicsQueue, commandBuffer, commandPool);
        }

        void updateTexture(Texture& context, void* buffer, uint32_t width, uint32_t height, uint32_t offsetX, uint32_t offsetY, VkCommandBuffer commandBuffer = VK_NULL_HANDLE)
        {
            VkDeviceSize imageSize = width * height * 4;

            if (buff.getHandle() == VK_NULL_HANDLE) buff.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, imageSize, graphicsQueue, Vk, commandPool, false);
            buff.writeToBuffer(buffer, imageSize, commandBuffer);

            bool cmdBuf = (commandBuffer == VK_NULL_HANDLE);

            if (cmdBuf) commandBuffer = beginSingleTimeCommands(Vk->device, commandPool);

            transitionImageLayout(
                0,
                context.mipLevels,
                context.image,
                VK_FORMAT_R8G8B8A8_SRGB,
                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                Vk->device, graphicsQueue,
                commandPool,
                commandBuffer
            );
        
            VkBufferImageCopy region{};
            region.bufferOffset = 0;
            region.bufferRowLength = 0;
            region.bufferImageHeight = 0;
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.mipLevel = 0;
            region.imageSubresource.baseArrayLayer = 0;
            region.imageSubresource.layerCount = 1;
            region.imageOffset = { (int32_t)offsetX, (int32_t)offsetY, 0 };
            region.imageExtent = { width, height, 1 };

            vkCmdCopyBufferToImage(commandBuffer, buff.getHandle(), context.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
        
            generateMipmaps(context.image, VK_FORMAT_R8G8B8A8_SRGB, width, height, context.mipLevels, commandBuffer);

            if (cmdBuf) endSingleTimeCommands(Vk->device, graphicsQueue, commandBuffer, commandPool);
        }
        
        void loadGlyphsIndices(uint32_t * glyphs, int glyphCount, std::string font, VkCommandBuffer commandBuffer = VK_NULL_HANDLE)
        {
            addGlyphsToAtlas(glyphs, glyphCount, FontMap[font].glyphDescriptions, FontMap[font], commandBuffer, true);
        }

        std::vector<TextureAtlas>& getBitmaps()
        {
            return Bitmaps;
        }

        void destroy()
        {
            // buf2 crashing
            if (buf2.getHandle() != VK_NULL_HANDLE) buf2.destroyBuffer();
            if (buff.getHandle() != VK_NULL_HANDLE) buff.destroyBuffer();

            for (auto& ac : assetMap)
            {
                vmaDestroyImage(Vk->allocator, ac.second.image, ac.second.ImageMemory);
                vkDestroyImageView(Vk->device, ac.second.ImgView, nullptr);
            }

            for (auto& atl : Bitmaps)
            {
                delete[] atl.buffer;
                vmaDestroyImage(Vk->allocator, atl.texture.image, atl.texture.ImageMemory);
                vkDestroyImageView(Vk->device, atl.texture.ImgView, nullptr);
            }

            vkDestroySampler(Vk->device, textureSampler, nullptr);
            vkDestroySampler(Vk->device, fontSampler, nullptr);
        }

        Texture& getAssetContext(std::string filename)
        {
            return assetMap[filename];
        }

        std::vector <Texture> getAllAssets()
        {
            std::vector<Texture> res;
            for (auto& [name, tex] : assetMap)
                res.emplace_back(tex);
            return res;
        }

        std::vector <Font> getAllFonts()
        {
            std::vector <Font> Assets = {};
            for (auto pair : FontMap)
            {
                Assets.emplace_back(pair.second);
            }

            return Assets;
        }

        Font& getFont(std::string font, int size)
        {
            return FontMap[getFontKey(font, size)];
        }

        Font& getFontDirect(std::string font_exact)
        {
            return FontMap[font_exact];
        }

        std::string getFontKey(std::string font, int size)
        {
            return font + std::to_string(getClosestFontSize(size));
        }

        int getClosestFontSize(int size)
        {
            //int i = 0;
            //while (settings::fontSizes[i] != -1)
            //{
            //    if (settings::fontSizes[i] > size)
            //        return settings::fontSizes[glm::max(i-1,0)];
            //    i++;
            //}

            //return settings::fontSizes[i-1];

            int i = 0;
            int delta = INT32_MAX;
            int idx = -1;

            while (settings::fontSizes[i] != -1)
            {
                if (glm::abs(settings::fontSizes[i] - size) < delta)
                {    
                    delta = glm::abs(settings::fontSizes[i] - size);
                    idx = i;
                }

                i++;
            }

            return settings::fontSizes[idx];
        }

        VkSampler getTextureSampler() { return textureSampler; }
};

class Shader
{
    VkPipeline pipeline                                                 = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout                                             = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout                                   = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool                                             = VK_NULL_HANDLE;
    
    std::vector<VertexInputBindingelement> VertexInputBindings                  = {};
    std::vector<UniformLayoutBindingelement> UniformLayoutBindings              = {};
    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings       = {};
    std::vector<VkDescriptorSet> descriptorSets                                 = {};
    std::vector<VkVertexInputAttributeDescription> VertexAttributeDescriptions  = {};

    VulkanHandler * VKH     = nullptr;
    Window * win            = nullptr;

    public:
        void init(VulkanHandler * VKH, Window * win, std::vector<VkDescriptorPoolSize> descriptorPoolLayout = {})
        {
            this->VKH = VKH; this->win = win;
            
            uint32_t maxTextures = 1;
            VkPhysicalDeviceProperties pp;
            vkGetPhysicalDeviceProperties(VKH->physicalDevice, &pp);

            maxTextures = pp.limits.maxDescriptorSetSampledImages;

            if (descriptorPoolLayout.size() == 0)
            {
                descriptorPoolLayout.emplace_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, settings::maxFramesInFlight*4});
                descriptorPoolLayout.emplace_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, maxTextures});
                descriptorPoolLayout.emplace_back(VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, settings::maxFramesInFlight*4});
            }

            VkDescriptorPoolCreateInfo poolInfo{};
            poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            poolInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolLayout.size());
            poolInfo.pPoolSizes = descriptorPoolLayout.data();
            poolInfo.maxSets = static_cast<uint32_t>(settings::maxFramesInFlight);
            poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;

            if (vkCreateDescriptorPool(VKH->device, &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
                LOG_FATAL("failed to create descriptor pool!");
        }

        void pushInputLayoutBinding(VertexInputBindingelement binding)
        {
            VertexInputBindings.push_back(binding);
        }

        void pushUnfiormLayoutBinding(UniformLayoutBindingelement binding)
        {
            UniformLayoutBindings.push_back(binding);

            VkDescriptorSetLayoutBinding dslb;
            dslb.binding = UniformLayoutBindings.size() - 1;
            dslb.descriptorCount = binding.descriptorCount;
            dslb.descriptorType = binding.type;
            dslb.stageFlags = binding.stage;
            dslb.pImmutableSamplers = nullptr; // Optional

            descriptorSetLayoutBindings.push_back(dslb);
        }
        
        void setupInputLayout()
        {
            std::vector<VkVertexInputAttributeDescription> descriptions = {};
            int offset = 0;
            int layoutNum = 0;

            for (std::size_t i = 0; i < VertexInputBindings.size(); i++)
            {
                VertexInputBindingelement &e = VertexInputBindings[i];

                for (uint32_t j = 0; j < e.count; j++)
                {
                    VkVertexInputAttributeDescription desc = {};
                    desc.binding = 0;
                    desc.location = layoutNum;
                    desc.format = e.type;
                    desc.offset = offset;

                    offset += e.sizeInBytes;
                    descriptions.push_back(desc);

                    layoutNum++;
                }
            }

            VertexAttributeDescriptions = descriptions;
        }
        
        void setupUnfiormLayout()
        {
            VkDescriptorSetLayoutCreateInfo layoutInfo{};
            layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            layoutInfo.bindingCount = descriptorSetLayoutBindings.size();
            layoutInfo.pBindings = descriptorSetLayoutBindings.data();

            if (vkCreateDescriptorSetLayout(VKH->device, &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
                LOG_FATAL("failed to create descriptor set layout!");

            std::vector<VkDescriptorSetLayout> layouts(settings::maxFramesInFlight, descriptorSetLayout);
            VkDescriptorSetAllocateInfo allocInfo{};
            allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            allocInfo.descriptorPool = descriptorPool;
            allocInfo.descriptorSetCount = static_cast<uint32_t>(settings::maxFramesInFlight);
            allocInfo.pSetLayouts = layouts.data();

            descriptorSets.resize(settings::maxFramesInFlight);
            if (vkAllocateDescriptorSets(VKH->device, &allocInfo, descriptorSets.data()) != VK_SUCCESS)
                LOG_FATAL("FAILED TO ALLOCATE Descriptor sets");
        }
        
        void updateUniformUBOs(GPUBuffer * UBOs, VkDeviceSize sizeBytes, int count, int binding = -1)
        {
            for (int i = 0; i < count; i++)
            {
                VkDescriptorBufferInfo bi;
                bi.buffer = UBOs[i].getHandle();
                bi.offset = 0;
                bi.range = sizeBytes;

                VkWriteDescriptorSet ds;
                ds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                ds.descriptorCount = 1;
                ds.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
                ds.pBufferInfo = &bi;
                ds.dstSet = descriptorSets[i];
                ds.dstArrayElement = 0;
                ds.pNext = nullptr;

                if (binding == -1)
                {
                    for (std::size_t i = 0; i < descriptorSetLayoutBindings.size(); i++)
                    {
                        if (descriptorSetLayoutBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
                            ds.dstBinding = descriptorSetLayoutBindings[i].binding;
                    }
                } else
                {
                    ds.dstBinding = binding;
                }

                updateUniform(ds);
            }
        }
        
        void updateSSBOs(GPUBuffer *SSBOs, VkDeviceSize sizeBytes, int count, int binding = -1)
        {
            for (int i = 0; i < count; i++)
            {
                VkDescriptorBufferInfo bi;
                bi.buffer = SSBOs[i].getHandle();
                bi.offset = 0;
                bi.range = sizeBytes;

                VkWriteDescriptorSet ds;
                ds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                ds.descriptorCount = 1;
                ds.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
                ds.pBufferInfo = &bi;
                ds.dstSet = descriptorSets[i];
                ds.dstArrayElement = 0;
                ds.pNext = nullptr;

                if (binding == -1)
                {
                    for (std::size_t i = 0; i < descriptorSetLayoutBindings.size(); i++)
                    {
                        if (descriptorSetLayoutBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                            ds.dstBinding = descriptorSetLayoutBindings[i].binding;
                    }
                } else
                {
                    ds.dstBinding = binding;
                }

                updateUniform(ds);
            }
        }
        
        void updateUniformTexture(VkImageView imgView, VkImageLayout layout, VkSampler sampler, uint32_t arrayElement, int count, int binding = -1)
        {
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = layout;
            imageInfo.imageView = imgView;
            imageInfo.sampler = sampler;

            for (int i = 0; i < count; i++)
            {
                VkWriteDescriptorSet ds;
                ds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                ds.descriptorCount = 1;
                ds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                ds.pImageInfo = &imageInfo;
                ds.dstSet = descriptorSets[i];
                ds.dstArrayElement = arrayElement;
                ds.pNext = nullptr;

                if (binding == -1)
                {
                    for (std::size_t i = 0; i < descriptorSetLayoutBindings.size(); i++)
                    {
                        if (descriptorSetLayoutBindings[i].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
                            ds.dstBinding = descriptorSetLayoutBindings[i].binding;
                    }
                } else
                {
                    ds.dstBinding = binding;
                }

                updateUniform(ds);
            }
        }
        
        void updateUniform(VkWriteDescriptorSet info)
        {
            vkUpdateDescriptorSets(VKH->device, 1, &info, 0, nullptr);
        }

        void createComputePipeline(std::string computeShader)
        {
            VkShaderModule computeShaderModule = createShaderModule(VKH->device, readFile(computeShader));

            VkPipelineShaderStageCreateInfo computeShaderStageInfo{};
            computeShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            computeShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            computeShaderStageInfo.module = computeShaderModule;
            computeShaderStageInfo.pName = "main";

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

            if (vkCreatePipelineLayout(VKH->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
                LOG_FATAL("failed to create pipeline layout!");

            VkComputePipelineCreateInfo pipelineInfo{};

            pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
            pipelineInfo.layout = pipelineLayout;
            pipelineInfo.stage = computeShaderStageInfo;

            if (vkCreateComputePipelines(VKH->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
                LOG_FATAL("failed to create compute pipeline!");

            vkDestroyShaderModule(VKH->device, computeShaderModule, nullptr);
        }

        void createGraphicsPipeline(std::string vert, std::string frag)
        {
            VkVertexInputBindingDescription bindDesc = VertexInputBinding_getBindingDescription(false);
            std::vector<VkVertexInputAttributeDescription> attDesc = getVertexAttributeDescriptions();
            
            VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 1;
            vertexInputInfo.pVertexBindingDescriptions = &bindDesc;
            vertexInputInfo.vertexAttributeDescriptionCount = VertexInputBinding_getAttDescSize();
            vertexInputInfo.pVertexAttributeDescriptions = attDesc.data();

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = win->currWinW;
            viewport.height = win->currWinH;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = {(uint)win->currWinW, (uint)win->currWinH};

            std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };

            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;

            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

            rasterizer.lineWidth = 1.0f;

            rasterizer.cullMode = VK_CULL_MODE_NONE;
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0.0f; // Optional
            rasterizer.depthBiasClamp = 0.0f;          // Optional
            rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional

            VkPipelineMultisampleStateCreateInfo multisampling{};

            if (settings::multisampling)
            {
                multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                multisampling.rasterizationSamples = VKH->msaaSamples;
                multisampling.sampleShadingEnable = VK_TRUE; // enable sample shading in the pipeline
                multisampling.minSampleShading = settings::antialiasing;           // min fraction for sample shading; closer to one is smoother        // Optional
                multisampling.pSampleMask = nullptr;            // Optional
                multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
                multisampling.alphaToOneEnable = VK_FALSE;      // Optional
            }
            else
            {
                multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
                multisampling.sampleShadingEnable = VK_FALSE;   // enable sample shading in the pipeline
                multisampling.minSampleShading = 0.f;           // min fraction for sample shading; closer to one is smoother        // Optional
                multisampling.pSampleMask = nullptr;            // Optional
                multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
                multisampling.alphaToOneEnable = VK_FALSE;      // Optional
            }

            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0.0f; // Optional
            colorBlending.blendConstants[1] = 0.0f; // Optional
            colorBlending.blendConstants[2] = 0.0f; // Optional
            colorBlending.blendConstants[3] = 0.0f; // Optional

            VkPipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.minDepthBounds = 0.0f; // Optional
            depthStencil.maxDepthBounds = 1.0f; // Optional
            depthStencil.stencilTestEnable = VK_FALSE;
            depthStencil.front = {}; // Optional
            depthStencil.back = {}; // Optional

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
            pipelineLayoutInfo.pushConstantRangeCount = 0;    // Optional
            pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

            _createGraphicsPipeline(vert, frag, vertexInputInfo, inputAssembly, viewport, scissor, dynamicStates, rasterizer, multisampling, colorBlendAttachment, colorBlending, pipelineLayoutInfo, depthStencil);
        }
      
        void createGraphicsPipeline_customVertexLayout(std::string vert, std::string frag, std::vector<VkVertexInputBindingDescription> vertexBindingDescs, std::vector<VkVertexInputAttributeDescription> vertexAttrDescs, VkPushConstantRange * push = VK_NULL_HANDLE)
        {
            VkPipelineVertexInputStateCreateInfo vertexInputState{};
            vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputState.vertexBindingDescriptionCount = vertexBindingDescs.size();
            vertexInputState.pVertexBindingDescriptions = vertexBindingDescs.data();
            vertexInputState.vertexAttributeDescriptionCount = vertexAttrDescs.size();
            vertexInputState.pVertexAttributeDescriptions = vertexAttrDescs.data();

            VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
            inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            inputAssembly.primitiveRestartEnable = VK_FALSE;

            VkViewport viewport{};
            viewport.x = 0.0f;
            viewport.y = 0.0f;
            viewport.width = win->currWinW;
            viewport.height = win->currWinH;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = {(uint)win->currWinW, (uint)win->currWinH};

            std::vector<VkDynamicState> dynamicStates = {
                VK_DYNAMIC_STATE_VIEWPORT,
                VK_DYNAMIC_STATE_SCISSOR
            };

            VkPipelineRasterizationStateCreateInfo rasterizer{};
            rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            rasterizer.depthClampEnable = VK_FALSE;

            rasterizer.rasterizerDiscardEnable = VK_FALSE;
            rasterizer.polygonMode = VK_POLYGON_MODE_FILL;

            rasterizer.lineWidth = 1.0f;

            rasterizer.cullMode = VK_CULL_MODE_NONE;
            rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

            rasterizer.depthBiasEnable = VK_FALSE;
            rasterizer.depthBiasConstantFactor = 0.0f; // Optional
            rasterizer.depthBiasClamp = 0.0f;          // Optional
            rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional

            VkPipelineMultisampleStateCreateInfo multisampling{};

            if (settings::multisampling)
            {
                multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                multisampling.rasterizationSamples = win->Vk->msaaSamples;
                multisampling.sampleShadingEnable = VK_TRUE; // enable sample shading in the pipeline
                multisampling.minSampleShading = settings::antialiasing;           // min fraction for sample shading; closer to one is smoother        // Optional
                multisampling.pSampleMask = nullptr;            // Optional
                multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
                multisampling.alphaToOneEnable = VK_FALSE;      // Optional
            }
            else
            {
                multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
                multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
                multisampling.sampleShadingEnable = VK_FALSE;   // enable sample shading in the pipeline
                multisampling.minSampleShading = 0.f;           // min fraction for sample shading; closer to one is smoother        // Optional
                multisampling.pSampleMask = nullptr;            // Optional
                multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
                multisampling.alphaToOneEnable = VK_FALSE;      // Optional
            }

            VkPipelineColorBlendAttachmentState colorBlendAttachment{};
            colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            colorBlendAttachment.blendEnable = VK_TRUE;
            colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
            colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

            VkPipelineColorBlendStateCreateInfo colorBlending{};
            colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            colorBlending.logicOpEnable = VK_FALSE;
            colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
            colorBlending.attachmentCount = 1;
            colorBlending.pAttachments = &colorBlendAttachment;
            colorBlending.blendConstants[0] = 0.0f; // Optional
            colorBlending.blendConstants[1] = 0.0f; // Optional
            colorBlending.blendConstants[2] = 0.0f; // Optional
            colorBlending.blendConstants[3] = 0.0f; // Optional

            VkPipelineDepthStencilStateCreateInfo depthStencil{};
            depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            depthStencil.depthTestEnable = VK_TRUE;
            depthStencil.depthWriteEnable = VK_TRUE;
            depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
            depthStencil.depthBoundsTestEnable = VK_FALSE;
            depthStencil.minDepthBounds = 0.0f; // Optional
            depthStencil.maxDepthBounds = 1.0f; // Optional
            depthStencil.stencilTestEnable = VK_FALSE;
            depthStencil.front = {}; // Optional
            depthStencil.back = {}; // Optional

            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount = 1;
            pipelineLayoutInfo.pSetLayouts = getDescriptorSetLayout();
            pipelineLayoutInfo.pushConstantRangeCount = 0;    // Optional
            pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

            if (push)
            {
                pipelineLayoutInfo.pushConstantRangeCount = 1;
                pipelineLayoutInfo.pPushConstantRanges = push; 
            }

            _createGraphicsPipeline( vert, frag, vertexInputState, inputAssembly, viewport, 
                                            scissor, dynamicStates, rasterizer, multisampling, colorBlendAttachment,
                                            colorBlending, pipelineLayoutInfo, depthStencil);
        }

        void destroy()
        {
            vkDestroyPipeline(VKH->device, pipeline, nullptr);
            vkDestroyPipelineLayout(VKH->device, pipelineLayout, nullptr);
            vkDestroyDescriptorSetLayout(VKH->device, descriptorSetLayout, nullptr);
            vkDestroyDescriptorPool(VKH->device, descriptorPool, nullptr);
        }

        VkPipeline getPipeline() { return pipeline; }
        VkPipelineLayout getPipelineLayout() { return pipelineLayout; }
        VkDescriptorSet * getDescriptorSets() { return descriptorSets.data(); }
        std::vector<VkDescriptorSetLayoutBinding> getLayoutBindings() { return descriptorSetLayoutBindings; }
        VkDescriptorSetLayout * getDescriptorSetLayout() { return &descriptorSetLayout; }

        std::vector<VkVertexInputAttributeDescription> getVertexAttributeDescriptions()
        {
            if (VertexAttributeDescriptions.size() == 0)
                LOG_ERROR("Vertex Attribute Description vector size is 0");
            return VertexAttributeDescriptions;
        }

        VkVertexInputBindingDescription VertexInputBinding_getBindingDescription(bool instanced)
        {
            uint32_t stride = 0;

            for (VertexInputBindingelement &e : VertexInputBindings)
                stride += e.sizeInBytes * e.count;
            
            VkVertexInputBindingDescription desc = {};
            desc.binding = 0;
            desc.inputRate = (instanced) ? VK_VERTEX_INPUT_RATE_INSTANCE : VK_VERTEX_INPUT_RATE_VERTEX;
            desc.stride = stride;
            return desc;
        }
        
        int VertexInputBinding_getAttDescSize()
        {
            int size = 0;
            for (VertexInputBindingelement &e : VertexInputBindings)
                size += e.count;
            
            return size;
        }

        VkShaderModule createShaderModule(VkDevice device, const std::vector<char> &code)
        {
            VkShaderModuleCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            createInfo.codeSize = code.size();
            createInfo.pCode = reinterpret_cast<const uint32_t *>(code.data());

            VkShaderModule shaderModule;
            if (vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
            {
                LOG_FATAL("failed to create shader module!");
            }

            return shaderModule;
        }

        void _createGraphicsPipeline( const std::string &vertPath, const std::string &fragPath, VkPipelineVertexInputStateCreateInfo &vertexInputInfo,
        VkPipelineInputAssemblyStateCreateInfo &inputAssembly, VkViewport &viewport, VkRect2D &scissor,
        std::vector<VkDynamicState> &dynamicStates, VkPipelineRasterizationStateCreateInfo &rasterizer,
        VkPipelineMultisampleStateCreateInfo &multisampling, VkPipelineColorBlendAttachmentState &colorBlendAttachment,
        VkPipelineColorBlendStateCreateInfo &colorBlending, VkPipelineLayoutCreateInfo &pipelineLayoutInfo,
        VkPipelineDepthStencilStateCreateInfo& depthStencil)
        {
            auto vertShaderCode = readFile(vertPath);
            auto fragShaderCode = readFile(fragPath);

            VkShaderModule vertShaderModule = createShaderModule(VKH->device, vertShaderCode);
            VkShaderModule fragShaderModule = createShaderModule(VKH->device, fragShaderCode);

            VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
            vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            vertShaderStageInfo.module = vertShaderModule;
            vertShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
            fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            fragShaderStageInfo.module = fragShaderModule;
            fragShaderStageInfo.pName = "main";

            VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

            VkPipelineDynamicStateCreateInfo dynamicState{};
            dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
            dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
            dynamicState.pDynamicStates = dynamicStates.data();

            VkPipelineViewportStateCreateInfo viewportState{};
            viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            viewportState.viewportCount = 1;
            viewportState.scissorCount = 1;

            if (vkCreatePipelineLayout(VKH->device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
                LOG_FATAL("failed to create pipeline layout!");
            //LOG_DEBUG("Created Pipeline Layout");

            VkGraphicsPipelineCreateInfo pipelineInfo{};
            pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            pipelineInfo.stageCount = 2;
            pipelineInfo.pStages = shaderStages;
            pipelineInfo.pVertexInputState = &vertexInputInfo;
            pipelineInfo.pInputAssemblyState = &inputAssembly;
            pipelineInfo.pViewportState = &viewportState;
            pipelineInfo.pRasterizationState = &rasterizer;
            pipelineInfo.pMultisampleState = &multisampling;
            pipelineInfo.pDepthStencilState = nullptr; // Optional
            pipelineInfo.pColorBlendState = &colorBlending;
            pipelineInfo.pDynamicState = &dynamicState;
            pipelineInfo.layout = pipelineLayout;

            pipelineInfo.renderPass = VK_NULL_HANDLE;

            VkPipelineRenderingCreateInfo pipelineRendering{};
            pipelineRendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            pipelineRendering.colorAttachmentCount = 1;
            pipelineRendering.pColorAttachmentFormats = &win->swapchainImageFormat;
            pipelineRendering.depthAttachmentFormat = win->depthFormat;
            pipelineRendering.stencilAttachmentFormat = win->depthFormat;
            pipelineInfo.pNext = &pipelineRendering;

            pipelineInfo.subpass = 0;

            pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
            pipelineInfo.basePipelineIndex = -1;              // Optional
            pipelineInfo.pDepthStencilState = &depthStencil;

            if (vkCreateGraphicsPipelines(VKH->device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline) != VK_SUCCESS)
                LOG_FATAL("failed to create graphics pipeline!");
            //LOG_DEBUG("Created Graphics Pipeline");

            vkDestroyShaderModule(VKH->device, fragShaderModule, nullptr);
            vkDestroyShaderModule(VKH->device, vertShaderModule, nullptr);
        }
};

enum EllipsizeMode
{
    ELLIPSE,
    LASTWORD,
    ELLIPSIZEMODEMAX
};

enum AlignMode
{
    LEFT,
    CENTER,
    RIGHT,
    AUTO,
    ALIGNMODEMAX
};


struct LineMetric
{
    float x, y;         // top-left of the line box, relative to the paragraph/line origin
    float w, h;
    float baseline_y;
    float center_y;      // visual vertical center (ascender/descender midpoint)
    std::u32string text; // actual glyph-string drawn on this line (post wrap/ellipsize)
};

struct TextParagraphData
{
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;   // 0 = grow to fit (single measured line width)
    float h = 0.f;   // 0 = grow to fit (no vertical clipping)
    EllipsizeMode ellipse = ELLIPSE;
    AlignMode align = LEFT;
    int fontSize = 16;
    float r = 1, g = 1, b = 1, a = 1;
    std::u32string text;
    std::string font;

    bool operator==(const TextParagraphData& o) const
    {
        return o.x == x && o.y == y && o.w == w && o.h == h &&
               o.ellipse == ellipse && o.align == align && o.fontSize == fontSize &&
               o.r == r && o.g == g && o.b == b && o.a == a &&
               o.text == text && o.font == font;
    }
};

struct TextLineData
{
    float x = 0.f;
    float y = 0.f;
    int fontSize = 16;
    float r = 1, g = 1, b = 1, a = 1;
    std::u32string text;
    std::string font;
    EllipsizeMode ellipse = ELLIPSE;
    AlignMode align = LEFT;  // only used if maxWidth > 0
    float maxWidth = 0.f;

    bool operator==(const TextLineData& o) const
    {
        return o.x == x && o.y == y && o.fontSize == fontSize &&
               o.r == r && o.g == g && o.b == b && o.a == a &&
               o.text == text && o.font == font &&
               o.align == align && o.maxWidth == maxWidth;
    }

    // shouldnt be accessed by outside of text drawer
    struct Vertex
    {
        glm::vec2 pos;
        glm::vec2 size;
        glm::vec4 uvBBox;
        int texIdx;
        int transformIdx;
    };

    std::vector<Vertex> shapingV = {};
    int transformIndex = -1;
};


class TextDrawer
{
    Shader shader;
    GPUBuffer VBOs[settings::maxFramesInFlight];
    GPUBuffer IBOs[settings::maxFramesInFlight];

    GPUBuffer InstVBOs[settings::maxFramesInFlight];
    GPUBuffer TransformBOs[settings::maxFramesInFlight];
    std::array<std::string,2> used_shaders;

    struct transform { float r,g,b,a,x,y; };

    struct Uniform { glm::mat4 proj; } uniform;

    struct RAQMfontfallback { Font font; int start; int len; };

    struct GlyphKey
    {
        std::string fontName;
        char32_t glyphIndex;
        bool operator==(const GlyphKey&) const = default;
    };

    struct GlyphKeyHash
    {
        size_t operator()(GlyphKey const& k) const noexcept
        {
            return std::hash<std::string>()(k.fontName) ^ (std::hash<uint32_t>()(k.glyphIndex) << 1);
        }
    };

    struct TupleHash
    {
        size_t operator()(const std::tuple<std::u32string,std::string,int>& k) const noexcept
        {
            size_t h1 = std::hash<std::u32string>()(std::get<0>(k));
            size_t h2 = std::hash<std::string>()(std::get<1>(k));
            size_t h3 = std::hash<int>()(std::get<2>(k));
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };

    struct PairHash
    {
        size_t operator()(const std::pair<std::string,std::u32string>& k) const noexcept
        {
            return std::hash<std::string>()(k.first) ^ (std::hash<std::u32string>()(k.second) << 1);
        }
    };

    struct ShapedText
    {
        std::vector<raqm_glyph_t> glyphs;   // copied out of raqm's internal buffer
    //    std::vector<const Font*> clusterFonts;
    };

    struct WrapKey
    {
        std::u32string text;
        std::string font;
        int fontSize;
        float w, h;
        EllipsizeMode ellipse;
        AlignMode align;
        bool operator==(const WrapKey&) const = default;
    };

    struct WrapKeyHash
    {
        size_t operator()(const WrapKey& k) const noexcept
        {
            size_t h1 = std::hash<std::u32string>()(k.text);
            size_t h2 = std::hash<std::string>()(k.font);
            size_t h3 = std::hash<int>()(k.fontSize);
            size_t h4 = std::hash<float>()(k.w) ^ (std::hash<float>()(k.h) << 1);
            size_t h5 = std::hash<int>()((int)k.ellipse) ^ (std::hash<int>()((int)k.align) << 1);
            return h1 ^ (h2 << 1) ^ (h3 << 2) ^ (h4 << 3) ^ (h5 << 4);
        }
    };

    Window * win;
    AssetManager * am;

    std::vector<std::string> font_order;
    int instance_size = 0;

    std::deque<TextLineData> pendingLines;
    std::vector<transform> Ts;
    std::vector<TextLineData::Vertex> vertices = {};

    raqm_t * instance;

    static std::mutex cacheMutex;
    static std::unordered_map<std::pair<std::string, std::u32string>, bool, PairHash> loaded;
    static std::unordered_set<GlyphKey, GlyphKeyHash> atlasGlyphCache;
    static std::unordered_map<std::tuple<std::u32string, std::string, int>, std::vector<float>, TupleHash> measureTextWidthCumulativeCache;
    static std::unordered_map<int, std::vector<Font*>> fallbackFontCache;
    static std::unordered_map<std::tuple<std::u32string,std::string,int>, ShapedText, /*hash*/ TupleHash> shapeCache;
    static std::unordered_map<std::tuple<std::u32string,std::string,int>, std::vector<TextLineData::Vertex>, TupleHash> vertexCache;
    static std::unordered_map<WrapKey, std::vector<TextLineData>, WrapKeyHash> wrapCache;

    raqm_direction_t detectParagraphDirection(const std::u32string& text);
    
    std::vector<RAQMfontfallback> getFontFallbackRuns(std::u32string_view str, const Font& primary);
    
    std::vector<float> measureTextWidthCumulative(const std::u32string& text, const Font& primary_font, int fontSize);
    
    float measureTextWidth(const std::u32string& text, const Font& primary_font, int fontSize);

    void ensureGlyphsInAtlasFor(const std::u32string& text, const std::string& fontName, int fontSize, VkCommandBuffer cmd = VK_NULL_HANDLE);

    void recreatePipelineIfNeeded();

    void shapeAndEmit(const std::u32string& text, const Font& primary,
                       size_t subStart, size_t subEnd,
                       glm::vec2 cursor, float scale, int tIdx,
                       std::vector<TextLineData::Vertex>& out);

    void addGlyphVertex(std::vector<TextLineData::Vertex>& vertices, const Font& font,
                         char32_t glyphIndex, int x_offset, int y_offset,
                         glm::vec2& cursor, float scale, int tIdx);

    int allocateTransform(float r, float g, float b, float a, float x, float y);

    const ShapedText& shapeText(const std::u32string& text, const Font& primary_font, int fontSize, std::vector<const Font*> clusterFonts);

    std::vector<TextLineData> WrapParagraph(const TextParagraphData& tod);
public:
    void init(Window * win, AssetManager * am, std::array<std::string,2> shaders);
    void setFallbackFonts(std::vector<std::string> ss);

    // optional preload to prevent frametime spike on first frame
    void preLoadText(TextLineData& data);
    void preLoadText(TextParagraphData& data);

    std::vector<TextLineData*> addParagraph(const TextParagraphData& data, VkCommandBuffer cmd = VK_NULL_HANDLE);
    TextLineData*       addLine(const TextLineData& data, VkCommandBuffer cmd = VK_NULL_HANDLE);

    // Pure queries -- no GPU state touched, safe to call any time.
    std::vector<LineMetric> getTextMetrics(const TextParagraphData& data);
    LineMetric getTextMetrics(const TextLineData& data);

    void writeToGPU(VkCommandBuffer commandBuffer = VK_NULL_HANDLE, int currFrameIndex = -1);
    void DrawCallInRenderPass(int currentFrameIndex);
    void destroy();
};

struct WindowRescources
{
    AssetManager assets;
    //TextDrawer text_drawer;
};

enum LayerEventType
{
    TOGGLE_DRAW,
    TOGGLE_UPDATE,
    MOVE_FORWARD, // move forward in draw order meaning it gets drawn earlier
    MOVE_FRONT,
    MOVE_BACKWARD,
    MOVE_BACK,
    SWITCH_TO_NEXT_STACK,
    ADD_SIGNAL_SEMAPHORE,
    ADD_WAIT_SEMAPHORE,
    INTERLAYER_EVENT
};

struct LayerEvent
{
    LayerEventType Type;
    void * eventOriginlayer;
    int eventDestinationlayer; // (index of origin layer + eventDestinationLayer) % LayerCount 
    std::any data;
    int data_size = -1;
};

struct LayerEventHandler
{
    std::queue<LayerEvent> events;
};

// represents a render pass
struct Layer
{
    Window * win = nullptr;
    VulkanHandler * VKH = nullptr;

    // updates per sec (= 1 / dt) if 0 then as fast as possible
    int updateFrequency = 0;

    virtual void init(VulkanHandler* VKH, Window * w, LayerEventHandler * EH, WindowRescources * res) = 0;
    virtual void destroy() = 0;
    virtual void setUpdateFrequency(int freq) = 0;
    virtual void onrecreate_swapchain() = 0;
    virtual void handle_event(LayerEvent ev) = 0;
    virtual void draw(uint32_t imageIndex, int currentFrameIndex) = 0;
    virtual void update(LayerEventHandler * EH, float t, float dt) = 0;
    virtual ~Layer() = default;
};

struct LayerStack
{
    struct DefaultLayer : Layer
    {
        float r;

        void init(VulkanHandler* VKH, Window * w, LayerEventHandler * EH, WindowRescources * res) override
        {
            this->VKH = VKH; win = w; this->updateFrequency = updateFrequency;
            r = 0;
        }
        
        void destroy() override {}

        void handle_event(LayerEvent ev) override {}

        void onrecreate_swapchain() override {}

        void setUpdateFrequency(int freq) override { updateFrequency = freq; }

        void draw(uint32_t imageIndex, int currentFrameIndex) override
        {
            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color = {{r, 0.01f, 0.05f, 1.0f}};
            clearValues[1].depthStencil = {1.0f, 0};

            VkViewport viewport{};
            viewport.x = 0;
            viewport.y = 0;
            viewport.width = win->swapchainExtent.width;
            viewport.height = win->swapchainExtent.height;
            viewport.minDepth = 0.0f;
            viewport.maxDepth = 1.0f;

            VkRect2D scissor{};
            scissor.offset = {0, 0};
            scissor.extent = {win->swapchainExtent.width, win->swapchainExtent.height};

            BeginRenderPass(win, VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_STORE_OP_STORE, clearValues, scissor, viewport);
            vkCmdSetViewport(win->commandBuffers[currentFrameIndex], 0, 1, &viewport);
            vkCmdSetScissor(win->commandBuffers[currentFrameIndex], 0, 1, &scissor);
            EndRenderPass(win);
        }

        void update(LayerEventHandler * EH, float t, float dt) override { r = (glm::sin(t) + 1) / 2; }
    };

    std::vector<Layer *> layers;
    std::vector<bool> wants_draw;
    std::vector<bool> wants_update;

    std::vector<int> order;

    std::vector<double> layerupdateTimings  = {};
    LayerEventHandler EH;
    bool wantsToSwitchToNext = false;

    std::vector<VkSemaphore> extra_signal = {};
    std::vector<VkSemaphore> extra_wait = {};

    void addLayer(Layer * l)
    {
        layers.emplace_back(l);
        layerupdateTimings.emplace_back(glfwGetTime());
    }

    void init(VulkanHandler * VKH, Window * win, WindowRescources * res)
    {
        for (int i = 0; i < layers.size(); i++)
        {
            order.push_back(i);
            wants_draw.push_back(true);
            wants_update.push_back(true);
        }

        if (layers.size() == 0)
        {
            addLayer(new DefaultLayer());
            order = {0};
            wants_draw = {true};
            wants_update = {true};
        }

        for (Layer* l : layers)
            l->init(VKH, win, &EH, res);
    }

    void destroy()
    {
        for (Layer* l : layers)
        {
            l->destroy();
            delete l;
        }
    }

    int getLayerIndexFromPointer(Layer* l)
    {
        for (int i = 0; i < layers.size(); i++)
            if (layers[i] == l)
                return i;
        return -1;
    }

    void handleEvents()
    {
        while (!EH.events.empty())
        {
            LayerEvent& ev = EH.events.front();

            switch (ev.Type) {
                case TOGGLE_DRAW:
                    wants_draw[getLayerIndexFromPointer((Layer*)ev.eventOriginlayer)] = !wants_draw[getLayerIndexFromPointer((Layer*)ev.eventOriginlayer)];
                    break;
                case TOGGLE_UPDATE:
                    wants_update[getLayerIndexFromPointer((Layer*)ev.eventOriginlayer)] = !wants_update[getLayerIndexFromPointer((Layer*)ev.eventOriginlayer)];
                    break;
                case MOVE_FORWARD:
                    for (int j = 0; j < order.size(); j++)
                    {
                        if (order[j] == getLayerIndexFromPointer((Layer*)ev.eventOriginlayer))
                        {
                            if (j == 0)
                                break;
                            std::swap(order[j], order[j-1]);
                            break;
                        }
                    }
                    break;

                case MOVE_FRONT:
                    for (int j = 0; j < order.size(); j++)
                    {
                        if (order[j] == getLayerIndexFromPointer((Layer*)ev.eventOriginlayer))
                        {
                            if (j == 0)
                                break;

                            std::swap(order[0], order[j]);
                            break;
                        }
                    }
                    break;

                case MOVE_BACKWARD:
                    for (int j = 0; j < order.size(); j++)
                    {
                        if (order[j] == getLayerIndexFromPointer((Layer*)ev.eventOriginlayer))
                        {
                            if (j == order.size() - 1)
                                break;

                            std::swap(order[order.size() - 1], order[j]);
                            break;
                        }
                    }   
                    break;
                case MOVE_BACK:
                    for (int j = 0; j < order.size(); j++)
                    {
                        if (order[j] == getLayerIndexFromPointer((Layer*)ev.eventOriginlayer))
                        {
                            if (j == order.size() - 1)
                                break;

                            std::swap(order[j], order[j+1]);
                            break;
                        }
                    }
                    break;
                case SWITCH_TO_NEXT_STACK:
                    wantsToSwitchToNext = true;
                    break;
                case ADD_SIGNAL_SEMAPHORE:
                    extra_signal.push_back(std::any_cast<VkSemaphore>(ev.data));
                    break;
                case ADD_WAIT_SEMAPHORE:
                    extra_wait.push_back(std::any_cast<VkSemaphore>(ev.data));
                    break;
                case INTERLAYER_EVENT:
                    layers[(getLayerIndexFromPointer((Layer*)ev.eventOriginlayer) + ev.eventDestinationlayer) % layers.size()]->handle_event(ev);
                    break;
            }

            EH.events.pop();
        }
    }

    void draw(int currentFrameIndex, uint32_t imageIndex)
    {
        for (uint32_t i : order)
        {
            layers[i]->draw(imageIndex, currentFrameIndex);
        }
    }

    void update(float dt)
    {
        for (uint32_t i : order)
        {
            Layer * l = layers[i];
            double now = glfwGetTime();

            if (l->updateFrequency == 0)
                l->update(&EH, now, dt * 0.001);
            else
            {
                if (now - layerupdateTimings[i] >= (1.0 / l->updateFrequency))
                {
                    l->update(&EH, now, (1.0 / l->updateFrequency));
                    layerupdateTimings[i] = layerupdateTimings[i] + (1.0 / l->updateFrequency); // hack to make timings more stable by accounting for lag tolerance ca. +- frametime
                }
            }
        }


        handleEvents();
    }
};


// handels the window loop
class LayerHandler
{
private:
    struct DebugLayer : Layer
    {
        TextDrawer text_drawer;
        Shader shader;
        GPUBuffer VBOs[settings::maxFramesInFlight];
        GPUBuffer UBOs[settings::maxFramesInFlight];
        GPUBuffer IBOs[settings::maxFramesInFlight];

        struct Vertex
        {
            float x;
            float y;
            float z;
            glm::vec4 col;
            glm::vec2 uv;
            int tex_idx;
        };

        struct Uniform
        {
            glm::mat4 proj;
            int window_width;
            int window_height;
            float show;
            float time;
        } u;

        TextParagraphData h;
        WindowRescources * res;

        void init(VulkanHandler * vk, Window * w, LayerEventHandler * EH, WindowRescources * res) override
        {
            win = w; this->updateFrequency = updateFrequency; VKH=vk; this->res = res;
            text_drawer.init(win, &res->assets, {"shaders/text_vert.spv", "shaders/text_frag.spv"});
            setUpdateFrequency(0);

            shader.init(VKH, win);

            shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32B32_SFLOAT, 1, sizeof(float)*3});
            shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32B32A32_SFLOAT, 1, sizeof(float)*4});
            shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32G32_SFLOAT, 1, sizeof(float)*2});
            shader.pushInputLayoutBinding(VertexInputBindingelement{VK_FORMAT_R32_SINT, 1, sizeof(int)});
            shader.setupInputLayout();

            shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT});
            shader.pushUnfiormLayoutBinding(UniformLayoutBindingelement{1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT});
            shader.setupUnfiormLayout();
            shader.createGraphicsPipeline("shaders/debug_vert.spv", "shaders/debug_frag.spv");
        

            u = Uniform{ glm::mat4(1), win->currWinW, win->currWinH, this->t, 0};
    
            Vertex v[4] = {
                {-1,-1,0, glm::vec4(1), glm::vec2(-1, -1), 0},
                {-1,+1,0, glm::vec4(1), glm::vec2(-1, +1), 0},
                {+1,+1,0, glm::vec4(1), glm::vec2(+1, +1), 0},
                {+1,-1,0, glm::vec4(1), glm::vec2(+1, -1), 0}
            };

            uint32_t ind[6] = { 0,1,2, 2,3,0 };
            
            h = TextParagraphData{50, 40, (float)win->currWinW - 100, (float)win->currWinH - 100, ELLIPSE, LEFT, 48, 0.225, 1, 0, u.show, U"FPS: --", "CascadiaCode.ttf"};

            for (uint i = 0; i < settings::maxFramesInFlight; i++)
            {
                VBOs[i].createBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, sizeof(v), win->graphicsQueue, win->Vk, win->commandPool);
                IBOs[i].createBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, sizeof(ind), win->graphicsQueue, win->Vk, win->commandPool);
                UBOs[i].createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, sizeof(Uniform), win->graphicsQueue, win->Vk, win->commandPool);

                VBOs[i].writeToBuffer(v, sizeof(v));
                IBOs[i].writeToBuffer(ind, sizeof(ind));
            }

            shader.updateUniformUBOs(UBOs, sizeof(Uniform), settings::maxFramesInFlight);
        
            init_end = glfwGetTime();
        }

        void handle_event(LayerEvent ev) override {}

        void destroy() override
        {
            text_drawer.destroy();

            for (uint i = 0; i < settings::maxFramesInFlight; i++)
            {
                VBOs[i].destroyBuffer();
                IBOs[i].destroyBuffer();
                UBOs[i].destroyBuffer();
            }

            shader.destroy();
        }

        void onrecreate_swapchain() override {}
        
        void draw(uint32_t imageIndex, int currentFrameIndex) override
        {
            if (u.show != 0)
            {
                UBOs[currentFrameIndex].writeToBuffer(&u, sizeof(Uniform), win->commandBuffers[currentFrameIndex]);
                text_drawer.addParagraph(h, win->commandBuffers[currentFrameIndex]);
                text_drawer.writeToGPU(win->commandBuffers[currentFrameIndex], currentFrameIndex);

                VkDescriptorImageInfo imageInfo{};
                imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                imageInfo.imageView = win->FB_ImgViews[win->currentFrameIndex];
                imageInfo.sampler = win->FB_sampler;

                VkWriteDescriptorSet ds;
                ds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                ds.descriptorCount = 1;
                ds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                ds.pImageInfo = &imageInfo;
                ds.dstSet = shader.getDescriptorSets()[win->currentFrameIndex];
                ds.dstArrayElement = 0;
                ds.pNext = nullptr;
                ds.dstBinding = 1;

                shader.updateUniform(ds);
            
                std::array<VkClearValue, 2> clearValues{};
                clearValues[0].color = {{0.05, 0.05f, 0.05f, 1.0f}};
                clearValues[1].depthStencil = {1.0f, 0};

                VkViewport viewport{};
                viewport.x = 0;
                viewport.y = 0;
                viewport.width = win->swapchainExtent.width;
                viewport.height = win->swapchainExtent.height;
                viewport.minDepth = 0.0f;
                viewport.maxDepth = 1.0f;

                VkRect2D scissor{};
                scissor.offset = {0, 0};
                scissor.extent = {win->swapchainExtent.width, win->swapchainExtent.height};

                VkBuffer vbo[] = {VBOs[currentFrameIndex].getHandle()};
                VkDeviceSize offsets[] = {0};

                BeginRenderPass(win, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_STORE_OP_STORE, clearValues, scissor, viewport);
                vkCmdSetViewport(win->commandBuffers[currentFrameIndex], 0, 1, &viewport);
                vkCmdSetScissor(win->commandBuffers[currentFrameIndex], 0, 1, &scissor);
                
                vkCmdBindIndexBuffer(win->commandBuffers[currentFrameIndex], IBOs[currentFrameIndex].getHandle(), 0, VK_INDEX_TYPE_UINT32);
                vkCmdBindVertexBuffers(win->commandBuffers[currentFrameIndex], 0, 1, vbo, offsets);
                vkCmdBindPipeline(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipeline());
                vkCmdBindDescriptorSets(win->commandBuffers[currentFrameIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, shader.getPipelineLayout(), 0, 1, &shader.getDescriptorSets()[currentFrameIndex], 0, nullptr);
        
                vkCmdDrawIndexed(win->commandBuffers[currentFrameIndex], 6, 1, 0, 0, 0);
                text_drawer.DrawCallInRenderPass(currentFrameIndex);
                EndRenderPass(win);
            }
            
            ft += win->frameTime;
            num_ft++;
        }

        void setUpdateFrequency(int freq) override { updateFrequency = freq; }

        float ft = 0.f;
        int num_ft = 0;

        float gt = 0.f;
        int num_gt = 0;

        float init_end = 0;
        int state = 0;
        float t = 0.0;
        int prevState = GLFW_RELEASE;

        bool fpsprint[2] = {false, false};

        void update(LayerEventHandler * EH, float t, float dt) override
        {
            auto glob_time = std::to_string((1000.f * (gt / num_gt)));
            auto fps = std::to_string((int)(1000.f / (ft / num_ft)));
            auto frameTime = std::to_string(ft / num_ft);
            auto glob_fps = std::to_string((int)(1.f / (gt / num_gt)));

            if (t - init_end >= 0.25)
            {
                h.text = (  U"RENDER: " + std::u32string(fps.begin(), fps.end()) + U" FPS - " + std::u32string(frameTime.begin(), frameTime.end()).substr(0,5) + U"ms\n" + 
                            U"MAINLP: " + std::u32string(glob_fps.begin(), glob_fps.end()) + U" FPS - " + std::u32string(glob_time.begin(), glob_time.end()).substr(0,5) + U"ms");
                ft = 0.f;
                num_ft = 0;
                gt = 0.f;
                num_gt = 0;
                init_end = t;
            }
        
            gt += dt;
            num_gt++;

            if (win->keyboardState[GLFW_KEY_GRAVE_ACCENT] == GLFW_PRESS && prevState == GLFW_RELEASE && state == 0)
                state = (u.show == 1) * -1 + (u.show == 0) * 1;
            
            prevState = win->keyboardState[GLFW_KEY_GRAVE_ACCENT];
            
            fpsprint[0] = win->keyboardState[GLFW_KEY_HOME]; 
            if (fpsprint[0] == GLFW_PRESS && fpsprint[1] == GLFW_RELEASE)
                LOG_DEBUG("\nRENDER: " + fps+ " FPS - " + frameTime.substr(0,5) + "ms\n" + "MAINLP: " + glob_fps + " FPS - " + glob_time.substr(0,5) + "ms\n-----------------------------------\n");
            fpsprint[1] = fpsprint[0];

            float anim_duration = 0.1; // s
            
            this->t += state * dt * (1.f / anim_duration);

            if (this->t > 1)
            {
                this->t = 1;
                state = 0;
            } else if (this->t < 0)
            {
                this->t = 0;
                state = 0;
            }

            u = Uniform{ glm::mat4(1), win->currWinW, win->currWinH, this->t, t};
            h.a = u.show;
            h.w = (float)win->currWinW - 100;
            h.h = (float)win->currWinH - 100;
        }
    } _DebugLayer;

    WindowRescources res;
public:
    std::vector<LayerStack> stacks;
    int currentStack = 0;
    VulkanHandler * VKH                     = nullptr;
    Window * win                            = nullptr; // does not own
    
    // only GLFWHAndler should call this
    void setHandlers(VulkanHandler * VKH, Window * w)
    {
        win = w; this->VKH = VKH;
    }

    void addStack(LayerStack& l)
    {
        stacks.emplace_back(l);
    }

    void initStacks()
    {
        res.assets.init(win->Vk, win->graphicsQueue, win->commandPool);
        res.assets.setupTextureSampler();
        res.assets.loadAssetDirectory("fonts");
        res.assets.loadAssetDirectoryRecursive("imgs");
        
        for (auto& stack : stacks)
        {
            stack.init(VKH, win, &res);
            stack.handleEvents();
        }

        if (settings::debugLayer)
        {
            _DebugLayer.init(VKH, win, nullptr, &res);
        }
        
    }
    
    void destroy()
    {
        for (auto& stack : stacks)
        {
            stack.destroy();
        }

        if (settings::debugLayer)
        {
            _DebugLayer.destroy();
        }

        res.assets.destroy();
    }

    void handleStacks(int currentFrameIndex, uint32_t imageIndex)
    {
        for (auto& stack : stacks)
        {
            if (stack.wantsToSwitchToNext)
            {
                currentStack = (currentStack + 1) % stacks.size();
                stack.wantsToSwitchToNext = false;
            }
        }
        
        stacks[currentStack].draw(currentFrameIndex, imageIndex);

        if (settings::debugLayer) _DebugLayer.draw(imageIndex, currentFrameIndex);
    }

    void updateStacks(float dt)
    {
        stacks[currentStack].update(dt);
        if (settings::debugLayer) _DebugLayer.update(nullptr, glfwGetTime(), dt * 0.001);
    }
};

// handels Windows and most glfwCalls except for temp window in vulkanhandler
class GLFWHandler
{
    public:
        void init()
        {
            glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
            glfwInit();
            glfwSetTime(0);

            if (!glfwVulkanSupported()) {
                LOG_FATAL("GLFW was built without Vulkan support!");
            }
        }

        bool shouldClose()
        {            
            for (std::size_t i = 0; i < windows.size(); i++)
            {
                auto& w = windows[i].first;

                if (w->wantsToQuit() && windows.size() > 1)
                {
                    vkDeviceWaitIdle(w->Vk->device);

                    windows[i].second.destroy();
                    w->destroy();
                    glfwDestroyWindow(w->winHandle);


                    windows.erase(windows.begin() + i);       
                }
                else if (w->wantsToQuit() && windows.size() == 1)
                {
                    return true;
                }
            }

            return false;
        }

        void destroy()
        {
            for (auto& w : windows)
            {
                w.second.destroy();
                w.first->destroy();
            }    
            glfwTerminate();
        }

        std::pair<std::unique_ptr<Window>, LayerHandler>* getNewWindowUnit(VulkanHandler& VKH)
        {
            windows.emplace_back(std::make_unique<Window>(), LayerHandler{});
            windows.back().second.setHandlers(&VKH, windows.back().first.get());
            return &windows.back();
        }

        void prepareForMainLoop(VulkanHandler& VKH)
        {
            for (auto& w : windows)
            {
                w.second.initStacks();
            }
        }

        static void recordCmdWindow(std::pair<std::unique_ptr<Window>, LayerHandler>& w)
        {
            auto& win = w.first;
            auto& layerHandler = w.second;

            // Reset and record command buffer for this window
            vkResetCommandBuffer(win->commandBuffers[win->currentFrameIndex], 0);
            
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            
            if (vkBeginCommandBuffer(win->commandBuffers[win->currentFrameIndex], &beginInfo) == VK_SUCCESS)
            {
                layerHandler.handleStacks(win->currentFrameIndex, win->imageIndex);

                VkImageMemoryBarrier2 barriers[2] = {};
                barriers[0]                  = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
                barriers[0].srcStageMask     = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
                barriers[0].srcAccessMask    = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
                barriers[0].dstStageMask     = VK_PIPELINE_STAGE_2_COPY_BIT;
                barriers[0].dstAccessMask    = VK_ACCESS_2_TRANSFER_READ_BIT;
                barriers[0].oldLayout        = VK_IMAGE_LAYOUT_GENERAL;
                barriers[0].newLayout        = VK_IMAGE_LAYOUT_GENERAL;
                barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barriers[0].image            = win->FB_images[win->currentFrameIndex];
                barriers[0].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

                barriers[1]                   = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
                barriers[1].srcStageMask     = VK_PIPELINE_STAGE_2_NONE;
                barriers[1].srcAccessMask    = VK_ACCESS_NONE;
                barriers[1].dstStageMask     = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
                barriers[1].dstAccessMask    = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                barriers[1].oldLayout        = VK_IMAGE_LAYOUT_UNDEFINED;
                barriers[1].newLayout        = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; 
                barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barriers[1].image            = win->swapchainImages[win->imageIndex];
                barriers[1].subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

                VkDependencyInfo dep {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
                dep.imageMemoryBarrierCount  = 2;
                dep.pImageMemoryBarriers     = barriers;

                vkCmdPipelineBarrier2(win->commandBuffers[win->currentFrameIndex], &dep);
                        

                VkImageCopy region {};
                region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.srcSubresource.baseArrayLayer = 0;
                region.srcSubresource.layerCount = 1;
                region.srcSubresource.mipLevel = 0;
                region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                region.dstSubresource.baseArrayLayer = 0;
                region.dstSubresource.layerCount = 1;
                region.dstSubresource.mipLevel = 0;
                region.extent = VkExtent3D {win->swapchainExtent.width, win->swapchainExtent.height, 1}; 
                
                vkCmdCopyImage(win->commandBuffers[win->currentFrameIndex], win->FB_images[win->currentFrameIndex], VK_IMAGE_LAYOUT_GENERAL, win->swapchainImages[win->imageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);


                VkImageMemoryBarrier2 post{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2 };
                post.srcStageMask        = VK_PIPELINE_STAGE_2_COPY_BIT;
                post.srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT;
                post.dstStageMask        = VK_PIPELINE_STAGE_2_NONE;
                post.dstAccessMask       = VK_ACCESS_2_NONE;
                post.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
                post.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
                post.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                post.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                post.image               = win->swapchainImages[win->imageIndex];
                post.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

                VkDependencyInfo postDep { VK_STRUCTURE_TYPE_DEPENDENCY_INFO };
                postDep.imageMemoryBarrierCount = 1;
                postDep.pImageMemoryBarriers    = &post;
                vkCmdPipelineBarrier2(win->commandBuffers[win->currentFrameIndex], &postDep);

                vkEndCommandBuffer(win->commandBuffers[win->currentFrameIndex]);
            }
        }

        static bool callCmdsWindow(GLFWHandler* WH, VulkanHandler& VKH, std::pair<std::unique_ptr<Window>, LayerHandler>& w)
        {
            auto& win = w.first;
            auto& layerHandler = w.second;

            vkWaitForFences(VKH.device, 1, &win->inFlightFences[win->currentFrameIndex], VK_TRUE, UINT64_MAX);
            
            VkAcquireNextImageInfoKHR acquireInfo {};
            acquireInfo.sType = VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR;
            acquireInfo.swapchain = win->swapchain;
            acquireInfo.timeout = UINT64_MAX;
            acquireInfo.semaphore = win->imageAvailableSemaphores[win->currentFrameIndex];
            acquireInfo.fence = VK_NULL_HANDLE;
            acquireInfo.deviceMask = 1;

            VkResult result = vkAcquireNextImage2KHR(VKH.device, &acquireInfo, &win->imageIndex);

            if (win->imagesInFlight[win->imageIndex] != VK_NULL_HANDLE)
                vkWaitForFences(VKH.device, 1, &win->imagesInFlight[win->imageIndex], VK_TRUE, UINT64_MAX);
            

            win->imagesInFlight[win->imageIndex] = win->inFlightFences[win->currentFrameIndex];
            vkResetFences(VKH.device, 1, &win->inFlightFences[win->currentFrameIndex]);

            if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
            {   
                recordCmdWindow(w);
                return true;
            }

            win->recreate_swapchain = true;
            return false;
        }

        static bool handleWindow(GLFWHandler* WH, VulkanHandler& VKH, std::pair<std::unique_ptr<Window>, LayerHandler>& w)
        {
            {
                double t0 = millis();
                auto& win = w.first;
                auto& layerHandler = w.second;

                bool recreate = !callCmdsWindow(WH, VKH, w);

                if (!recreate)
                {
                    // Submit with window-specific semaphores
                    std::vector<VkSemaphoreSubmitInfo> waits {};
                    
                    VkSemaphoreSubmitInfo wait {};
                    wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                    wait.semaphore = win->imageAvailableSemaphores[win->currentFrameIndex];
                    wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                    waits.push_back(wait);

                    for (VkSemaphore sem : layerHandler.stacks[layerHandler.currentStack].extra_wait)
                    {
                        VkSemaphoreSubmitInfo wai {};
                        wai.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                        wai.semaphore = sem;
                        wai.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                        waits.push_back(wai);
                    }

                    std::vector<VkSemaphoreSubmitInfo> signals {};
                    
                    VkSemaphoreSubmitInfo signal {};
                    signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                    signal.semaphore = win->renderFinishedSemaphores[win->currentFrameIndex*win->swapchainImages.size() + win->imageIndex];
                    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                    signals.push_back(signal);

                    for (VkSemaphore sem : layerHandler.stacks[layerHandler.currentStack].extra_signal)
                    {
                        VkSemaphoreSubmitInfo sign {};
                        sign.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
                        sign.semaphore = sem;
                        sign.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
                        signals.push_back(sign);
                    }

                    VkCommandBufferSubmitInfo cbuff {};
                    cbuff.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
                    cbuff.commandBuffer = win->commandBuffers[win->currentFrameIndex];
                    cbuff.deviceMask = 0;

                    VkSubmitInfo2 submitInfo{};
                    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
                    submitInfo.waitSemaphoreInfoCount = waits.size();
                    submitInfo.pWaitSemaphoreInfos = waits.data();
                    submitInfo.commandBufferInfoCount = 1;
                    submitInfo.pCommandBufferInfos = &cbuff;
                    submitInfo.signalSemaphoreInfoCount = signals.size();
                    submitInfo.pSignalSemaphoreInfos = signals.data();
                    
                    vkQueueSubmit2(win->graphicsQueue, 1, &submitInfo, win->inFlightFences[win->currentFrameIndex]);

                    // Present this window
                    VkPresentInfoKHR presentInfo{};
                    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                    presentInfo.waitSemaphoreCount = 1;
                    presentInfo.pWaitSemaphores = &signal.semaphore;
                    presentInfo.swapchainCount = 1;
                    presentInfo.pSwapchains = &win->swapchain;
                    presentInfo.pImageIndices = &win->imageIndex;
                    
                    vkQueuePresentKHR(win->presentQueue, &presentInfo);

                    win->currentFrameIndex = (win->currentFrameIndex + 1) % settings::maxFramesInFlight;
                    win->frameTime = millis() - t0;
                }
                
                return recreate;
            }
        }

        void handle(VulkanHandler& VKH)
        {
            for (auto& w : windows)
            {   
                if (w.first->recreate_swapchain)
                {
                    int fbw, fbh;
                    glfwGetFramebufferSize(w.first->winHandle, &fbw, &fbh);
                    w.first->recreateSwap(fbw, fbh);
                    
                    for (auto& stack : w.second.stacks)
                    {
                        for (auto& lay : stack.layers)
                            lay->onrecreate_swapchain();
                    }
                    
                    w.first->recreate_swapchain = false;
                }
            }
    
            std::for_each(std::execution::par, windows.begin(), windows.end(), [&, this](auto&& w){
                handleWindow(this, VKH, w);
            });

            for (auto& w : windows)
            {
                w.second.updateStacks(globalFrameTime);
            }

            glfwPollEvents();            

            float t0 = millis();

            if (lastFrameTimestamp != 0.0)
            {
                globalFrameTime = t0 - lastFrameTimestamp;
            }

            lastFrameTimestamp = t0;
        }

    private:
        std::vector<std::pair<std::unique_ptr<Window>, LayerHandler>> windows;
        float globalFrameTime = 0.f;
        float lastFrameTimestamp = 0.f;
};

