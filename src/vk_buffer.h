#pragma once

#include "vk_types.h"

struct CommandManager;

struct AllocatedBuffer {
    VkBuffer       buffer        = VK_NULL_HANDLE;
    VmaAllocation  allocation    = VK_NULL_HANDLE;
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize   size          = 0;
};

struct AllocatedImage {
    VkImage       image      = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VkImageView   view       = VK_NULL_HANDLE;
    VkExtent2D    extent     = {0, 0};
    VkFormat      format     = VK_FORMAT_UNDEFINED;
};

// GPU-only by default. Pass VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT for host-visible.
AllocatedBuffer createBuffer(VmaAllocator allocator, VkDevice device,
    VkDeviceSize size, VkBufferUsageFlags usage,
    VmaAllocationCreateFlags vmaFlags = 0);

// Staging upload: creates temp host buffer, copies data, returns device-local buffer.
AllocatedBuffer uploadBuffer(VmaAllocator allocator, VkDevice device,
    CommandManager& commands, VkQueue queue,
    const void* data, VkDeviceSize size, VkBufferUsageFlags usage);

// R32G32B32A32_SFLOAT storage image for RT output.
AllocatedImage createStorageImage(VmaAllocator allocator, VkDevice device,
    VkExtent2D extent, VkFormat format);

void destroyBuffer(VmaAllocator allocator, AllocatedBuffer& buf);
void destroyImage(VmaAllocator allocator, VkDevice device, AllocatedImage& img);

void cmdTransitionImageLayout(VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
    VkAccessFlags srcAccess, VkAccessFlags dstAccess);
