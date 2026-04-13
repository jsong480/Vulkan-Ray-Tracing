#include "vk_commands.h"

void CommandManager::init(VkDevice device, uint32_t queueFamily) {
    VkCommandPoolCreateInfo ci{};
    ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    ci.queueFamilyIndex = queueFamily;
    VK_CHECK(vkCreateCommandPool(device, &ci, nullptr, &pool));
}

void CommandManager::cleanup(VkDevice device) {
    if (pool) vkDestroyCommandPool(device, pool, nullptr);
    pool = VK_NULL_HANDLE;
}

VkCommandBuffer CommandManager::allocate(VkDevice device) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = pool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device, &ai, &cmd));
    return cmd;
}

std::vector<VkCommandBuffer> CommandManager::allocate(VkDevice device, uint32_t count) {
    VkCommandBufferAllocateInfo ai{};
    ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool        = pool;
    ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = count;

    std::vector<VkCommandBuffer> cmds(count);
    VK_CHECK(vkAllocateCommandBuffers(device, &ai, cmds.data()));
    return cmds;
}

VkCommandBuffer CommandManager::beginSingleTime(VkDevice device) {
    auto cmd = allocate(device);

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &bi));

    return cmd;
}

void CommandManager::endSingleTime(VkDevice device, VkQueue queue, VkCommandBuffer cmd) {
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo si{};
    si.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers    = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &si, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    vkFreeCommandBuffers(device, pool, 1, &cmd);
}
