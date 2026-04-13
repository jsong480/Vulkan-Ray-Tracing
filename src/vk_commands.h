#pragma once

#include "vk_types.h"

struct CommandManager {
    VkCommandPool pool = VK_NULL_HANDLE;

    void init(VkDevice device, uint32_t queueFamily);
    void cleanup(VkDevice device);

    VkCommandBuffer              allocate(VkDevice device);
    std::vector<VkCommandBuffer> allocate(VkDevice device, uint32_t count);

    VkCommandBuffer beginSingleTime(VkDevice device);
    void            endSingleTime(VkDevice device, VkQueue queue, VkCommandBuffer cmd);
};
