#include "vk_swapchain.h"
#include "vk_context.h"

// ─── Query helpers ──────────────────────────────────────────────────────────

struct SwapchainSupport {
    VkSurfaceCapabilitiesKHR        capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR>   presentModes;
};

static SwapchainSupport querySupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupport s;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &s.capabilities);

    uint32_t count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, nullptr);
    s.formats.resize(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &count, s.formats.data());

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, nullptr);
    s.presentModes.resize(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &count, s.presentModes.data());

    return s;
}

static VkSurfaceFormatKHR chooseFormat(const std::vector<VkSurfaceFormatKHR>& formats) {
    for (const auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB
            && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            return f;
    return formats[0];
}

static VkPresentModeKHR choosePresentMode(const std::vector<VkPresentModeKHR>& modes) {
    for (auto m : modes)
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m;
    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D chooseExtent(const VkSurfaceCapabilitiesKHR& caps, GLFWwindow* window) {
    if (caps.currentExtent.width != (std::numeric_limits<uint32_t>::max)())
        return caps.currentExtent;

    int w, h;
    glfwGetFramebufferSize(window, &w, &h);
    VkExtent2D ext = {static_cast<uint32_t>(w), static_cast<uint32_t>(h)};
    ext.width  = std::clamp(ext.width,  caps.minImageExtent.width,  caps.maxImageExtent.width);
    ext.height = std::clamp(ext.height, caps.minImageExtent.height, caps.maxImageExtent.height);
    return ext;
}

// ─── Public API ─────────────────────────────────────────────────────────────

void Swapchain::init(VkContext& ctx, GLFWwindow* window) {
    auto support = querySupport(ctx.physicalDevice, ctx.surface);
    auto format  = chooseFormat(support.formats);
    auto mode    = choosePresentMode(support.presentModes);
    auto ext     = chooseExtent(support.capabilities, window);

    uint32_t imgCount = support.capabilities.minImageCount + 1;
    if (support.capabilities.maxImageCount > 0)
        imgCount = (std::min)(imgCount, support.capabilities.maxImageCount);

    VkSwapchainCreateInfoKHR ci{};
    ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    ci.surface          = ctx.surface;
    ci.minImageCount    = imgCount;
    ci.imageFormat      = format.format;
    ci.imageColorSpace  = format.colorSpace;
    ci.imageExtent      = ext;
    ci.imageArrayLayers = 1;
    ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                        | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

    uint32_t families[] = {
        ctx.queueFamilies.graphicsFamily.value(),
        ctx.queueFamilies.presentFamily.value()
    };
    if (families[0] != families[1]) {
        ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
        ci.queueFamilyIndexCount = 2;
        ci.pQueueFamilyIndices   = families;
    } else {
        ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

    ci.preTransform   = support.capabilities.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    ci.presentMode    = mode;
    ci.clipped        = VK_TRUE;
    ci.oldSwapchain   = swapchain;

    VK_CHECK(vkCreateSwapchainKHR(ctx.device, &ci, nullptr, &swapchain));

    imageFormat = format.format;
    extent      = ext;

    vkGetSwapchainImagesKHR(ctx.device, swapchain, &imgCount, nullptr);
    images.resize(imgCount);
    vkGetSwapchainImagesKHR(ctx.device, swapchain, &imgCount, images.data());

    imageViews.resize(imgCount);
    for (uint32_t i = 0; i < imgCount; i++) {
        VkImageViewCreateInfo vci{};
        vci.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image            = images[i];
        vci.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vci.format           = imageFormat;
        vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VK_CHECK(vkCreateImageView(ctx.device, &vci, nullptr, &imageViews[i]));
    }

    std::cout << "[Swapchain] " << extent.width << "x" << extent.height
              << ", " << imgCount << " images\n";
}

void Swapchain::cleanup(VkDevice device) {
    for (auto view : imageViews)
        vkDestroyImageView(device, view, nullptr);
    imageViews.clear();
    images.clear();
    if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
}
