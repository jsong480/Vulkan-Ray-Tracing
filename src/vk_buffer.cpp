#include "vk_buffer.h"
#include "vk_commands.h"

AllocatedBuffer createBuffer(VmaAllocator allocator, VkDevice device,
    VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags vmaFlags)
{
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = size;
    bci.usage = usage;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;
    aci.flags = vmaFlags;

    AllocatedBuffer buf{};
    buf.size = size;
    VK_CHECK(vmaCreateBuffer(allocator, &bci, &aci, &buf.buffer, &buf.allocation, nullptr));

    if (usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo info{};
        info.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        info.buffer = buf.buffer;
        buf.deviceAddress = vkGetBufferDeviceAddress(device, &info);
    }
    return buf;
}

AllocatedBuffer uploadBuffer(VmaAllocator allocator, VkDevice device,
    CommandManager& commands, VkQueue queue,
    const void* data, VkDeviceSize size, VkBufferUsageFlags usage)
{
    auto staging = createBuffer(allocator, device, size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    void* mapped;
    vmaMapMemory(allocator, staging.allocation, &mapped);
    std::memcpy(mapped, data, size);
    vmaUnmapMemory(allocator, staging.allocation);

    auto dst = createBuffer(allocator, device, size,
        usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, 0);

    auto cmd = commands.beginSingleTime(device);
    VkBufferCopy region{};
    region.size = size;
    vkCmdCopyBuffer(cmd, staging.buffer, dst.buffer, 1, &region);
    commands.endSingleTime(device, queue, cmd);

    destroyBuffer(allocator, staging);
    return dst;
}

AllocatedImage createStorageImage(VmaAllocator allocator, VkDevice device,
    VkExtent2D extent, VkFormat format)
{
    VkImageCreateInfo ici{};
    ici.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.imageType     = VK_IMAGE_TYPE_2D;
    ici.format        = format;
    ici.extent        = {extent.width, extent.height, 1};
    ici.mipLevels     = 1;
    ici.arrayLayers   = 1;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    VmaAllocationCreateInfo aci{};
    aci.usage = VMA_MEMORY_USAGE_AUTO;

    AllocatedImage img{};
    img.extent = extent;
    img.format = format;
    VK_CHECK(vmaCreateImage(allocator, &ici, &aci, &img.image, &img.allocation, nullptr));

    VkImageViewCreateInfo vci{};
    vci.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.image            = img.image;
    vci.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    vci.format           = format;
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VK_CHECK(vkCreateImageView(device, &vci, nullptr, &img.view));

    return img;
}

void destroyBuffer(VmaAllocator allocator, AllocatedBuffer& buf) {
    if (buf.buffer) vmaDestroyBuffer(allocator, buf.buffer, buf.allocation);
    buf = {};
}

void destroyImage(VmaAllocator allocator, VkDevice device, AllocatedImage& img) {
    if (img.view)  vkDestroyImageView(device, img.view, nullptr);
    if (img.image) vmaDestroyImage(allocator, img.image, img.allocation);
    img = {};
}

void cmdTransitionImageLayout(VkCommandBuffer cmd, VkImage image,
    VkImageLayout oldLayout, VkImageLayout newLayout,
    VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
    VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
    VkImageMemoryBarrier barrier{};
    barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask       = srcAccess;
    barrier.dstAccessMask       = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}
