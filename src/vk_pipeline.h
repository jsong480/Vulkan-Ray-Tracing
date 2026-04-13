#pragma once

#include "vk_buffer.h"

VkShaderModule loadShaderModule(VkDevice device, const std::string& path);

struct RTPipelineBuilder {
    std::vector<VkPipelineShaderStageCreateInfo>        stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR>   groups;

    uint32_t addRayGenShader(VkShaderModule module);
    uint32_t addMissShader(VkShaderModule module);
    uint32_t addHitGroup(VkShaderModule closestHit,
                         VkShaderModule anyHit       = VK_NULL_HANDLE,
                         VkShaderModule intersection = VK_NULL_HANDLE);

    VkPipeline build(VkDevice device, VkPipelineLayout layout,
                     uint32_t maxRecursionDepth = 2);
};

struct ShaderBindingTable {
    AllocatedBuffer raygenBuf;
    AllocatedBuffer missBuf;
    AllocatedBuffer hitBuf;

    VkStridedDeviceAddressRegionKHR raygenRegion{};
    VkStridedDeviceAddressRegionKHR missRegion{};
    VkStridedDeviceAddressRegionKHR hitRegion{};
    VkStridedDeviceAddressRegionKHR callableRegion{};

    void build(VkDevice device, VmaAllocator allocator,
               VkPipeline pipeline,
               const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtProps,
               uint32_t raygenCount, uint32_t missCount, uint32_t hitCount);
    void cleanup(VmaAllocator allocator);
};
