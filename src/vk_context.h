#pragma once

#include "vk_types.h"

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

struct VkContext {
    VkInstance               instance       = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR             surface        = VK_NULL_HANDLE;
    VkPhysicalDevice         physicalDevice = VK_NULL_HANDLE;
    VkDevice                 device         = VK_NULL_HANDLE;

    QueueFamilyIndices queueFamilies;
    VkQueue            graphicsQueue = VK_NULL_HANDLE;
    VkQueue            presentQueue  = VK_NULL_HANDLE;

    VmaAllocator allocator = VK_NULL_HANDLE;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR   rtPipelineProperties{};
    VkPhysicalDeviceAccelerationStructureFeaturesKHR  asFeatures{};

    void init(GLFWwindow* window);
    void cleanup();
};
