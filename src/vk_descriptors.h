#pragma once

#include "vk_types.h"

struct DescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    DescriptorLayoutBuilder& addBinding(uint32_t binding, VkDescriptorType type,
        VkShaderStageFlags stageFlags, uint32_t count = 1);
    VkDescriptorSetLayout build(VkDevice device);
};

VkDescriptorPool createDescriptorPool(VkDevice device,
    const std::vector<VkDescriptorPoolSize>& sizes, uint32_t maxSets);

VkDescriptorSet allocateDescriptorSet(VkDevice device,
    VkDescriptorPool pool, VkDescriptorSetLayout layout);
