#pragma once

#include "vk_types.h"

struct VkContext;

struct Swapchain {
    VkSwapchainKHR           swapchain   = VK_NULL_HANDLE;
    VkFormat                 imageFormat  = VK_FORMAT_UNDEFINED;
    VkExtent2D               extent      = {0, 0};
    std::vector<VkImage>     images;
    std::vector<VkImageView> imageViews;

    void     init(VkContext& ctx, GLFWwindow* window);
    void     cleanup(VkDevice device);
    uint32_t imageCount() const { return static_cast<uint32_t>(images.size()); }
};
