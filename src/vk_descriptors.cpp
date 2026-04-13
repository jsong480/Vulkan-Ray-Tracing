#include "vk_descriptors.h"

DescriptorLayoutBuilder& DescriptorLayoutBuilder::addBinding(
    uint32_t binding, VkDescriptorType type,
    VkShaderStageFlags stageFlags, uint32_t count)
{
    VkDescriptorSetLayoutBinding b{};
    b.binding         = binding;
    b.descriptorType  = type;
    b.descriptorCount = count;
    b.stageFlags      = stageFlags;
    bindings.push_back(b);
    return *this;
}

VkDescriptorSetLayout DescriptorLayoutBuilder::build(VkDevice device) {
    VkDescriptorSetLayoutCreateInfo ci{};
    ci.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.bindingCount = static_cast<uint32_t>(bindings.size());
    ci.pBindings    = bindings.data();

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &ci, nullptr, &layout));
    return layout;
}

VkDescriptorPool createDescriptorPool(VkDevice device,
    const std::vector<VkDescriptorPoolSize>& sizes, uint32_t maxSets)
{
    VkDescriptorPoolCreateInfo ci{};
    ci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.maxSets       = maxSets;
    ci.poolSizeCount = static_cast<uint32_t>(sizes.size());
    ci.pPoolSizes    = sizes.data();

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(device, &ci, nullptr, &pool));
    return pool;
}

VkDescriptorSet allocateDescriptorSet(VkDevice device,
    VkDescriptorPool pool, VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool     = pool;
    ai.descriptorSetCount = 1;
    ai.pSetLayouts        = &layout;

    VkDescriptorSet set;
    VK_CHECK(vkAllocateDescriptorSets(device, &ai, &set));
    return set;
}
