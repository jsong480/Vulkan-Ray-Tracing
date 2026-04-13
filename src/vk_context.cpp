#define VMA_IMPLEMENTATION
#include "vk_context.h"

// ─── Configuration ──────────────────────────────────────────────────────────

#ifdef NDEBUG
static constexpr bool kEnableValidation = false;
#else
static constexpr bool kEnableValidation = true;
#endif

static const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
};

// ─── Debug messenger ────────────────────────────────────────────────────────

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT           severity,
    VkDebugUtilsMessageTypeFlagsEXT                  /*type*/,
    const VkDebugUtilsMessengerCallbackDataEXT*      data,
    void*                                            /*user*/)
{
    if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        std::cerr << "[Validation] " << data->pMessage << "\n";
    }
    return VK_FALSE;
}

static VkDebugUtilsMessengerCreateInfoEXT makeDebugMessengerCI() {
    VkDebugUtilsMessengerCreateInfoEXT ci{};
    ci.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    ci.messageSeverity  = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    ci.messageType      = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
                        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    ci.pfnUserCallback  = debugCallback;
    return ci;
}

// ─── Helpers ────────────────────────────────────────────────────────────────

static bool checkValidationLayerSupport() {
    uint32_t count;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());

    for (const char* name : kValidationLayers) {
        bool found = false;
        for (const auto& layer : available)
            if (std::strcmp(name, layer.layerName) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

static std::vector<const char*> getRequiredInstanceExtensions() {
    uint32_t     glfwCount = 0;
    const char** glfwExts  = glfwGetRequiredInstanceExtensions(&glfwCount);
    std::vector<const char*> exts(glfwExts, glfwExts + glfwCount);

    if (kEnableValidation)
        exts.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    return exts;
}

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
    std::vector<VkQueueFamilyProperties> families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, families.data());

    for (uint32_t i = 0; i < count; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphicsFamily = i;

        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present);
        if (present) indices.presentFamily = i;

        if (indices.isComplete()) break;
    }
    return indices;
}

static bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
    uint32_t count;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
    std::vector<VkExtensionProperties> available(count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &count, available.data());

    std::set<std::string> required(kDeviceExtensions.begin(), kDeviceExtensions.end());
    for (const auto& ext : available)
        required.erase(ext.extensionName);

    return required.empty();
}

static bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
    if (!findQueueFamilies(device, surface).isComplete()) return false;
    if (!checkDeviceExtensionSupport(device))             return false;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asF{};
    asF.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtF{};
    rtF.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtF.pNext = &asF;

    VkPhysicalDeviceFeatures2 f2{};
    f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    f2.pNext = &rtF;
    vkGetPhysicalDeviceFeatures2(device, &f2);

    return asF.accelerationStructure && rtF.rayTracingPipeline;
}

// ─── Init steps ─────────────────────────────────────────────────────────────

static void createInstance(VkContext& ctx) {
    if (kEnableValidation && !checkValidationLayerSupport())
        throw std::runtime_error("Validation layers requested but not available");

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName   = "Vulkan Ray Tracing";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName        = "No Engine";
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion         = VK_API_VERSION_1_3;

    auto extensions = getRequiredInstanceExtensions();

    VkInstanceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo        = &appInfo;
    ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
    ci.ppEnabledExtensionNames = extensions.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCI{};
    if (kEnableValidation) {
        ci.enabledLayerCount   = static_cast<uint32_t>(kValidationLayers.size());
        ci.ppEnabledLayerNames = kValidationLayers.data();
        debugCI                = makeDebugMessengerCI();
        ci.pNext               = &debugCI;
    }

    VK_CHECK(vkCreateInstance(&ci, nullptr, &ctx.instance));
}

static void setupDebugMessenger(VkContext& ctx) {
    if (!kEnableValidation) return;

    auto ci   = makeDebugMessengerCI();
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(ctx.instance, "vkCreateDebugUtilsMessengerEXT"));

    if (!func || func(ctx.instance, &ci, nullptr, &ctx.debugMessenger) != VK_SUCCESS)
        throw std::runtime_error("Failed to create debug messenger");
}

static void createSurface(VkContext& ctx, GLFWwindow* window) {
    VK_CHECK(glfwCreateWindowSurface(ctx.instance, window, nullptr, &ctx.surface));
}

static void pickPhysicalDevice(VkContext& ctx) {
    uint32_t count = 0;
    vkEnumeratePhysicalDevices(ctx.instance, &count, nullptr);
    if (count == 0) throw std::runtime_error("No Vulkan-capable GPU found");

    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(ctx.instance, &count, devices.data());

    // Prefer discrete GPU with ray tracing support
    for (auto d : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(d, &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
            && isDeviceSuitable(d, ctx.surface)) {
            ctx.physicalDevice = d;
            std::cout << "[GPU] " << props.deviceName << " (discrete)\n";
            return;
        }
    }
    for (auto d : devices) {
        if (isDeviceSuitable(d, ctx.surface)) {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(d, &props);
            ctx.physicalDevice = d;
            std::cout << "[GPU] " << props.deviceName << "\n";
            return;
        }
    }
    throw std::runtime_error("No GPU with ray tracing support found");
}

static void createLogicalDevice(VkContext& ctx) {
    ctx.queueFamilies = findQueueFamilies(ctx.physicalDevice, ctx.surface);

    std::set<uint32_t> uniqueFamilies = {
        ctx.queueFamilies.graphicsFamily.value(),
        ctx.queueFamilies.presentFamily.value()
    };

    float priority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCIs;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo qci{};
        qci.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        qci.queueFamilyIndex = family;
        qci.queueCount       = 1;
        qci.pQueuePriorities = &priority;
        queueCIs.push_back(qci);
    }

    // Build the feature chain: Features2 -> Vulkan12 -> Vulkan13 -> AS -> RT
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rtFeatures{};
    rtFeatures.sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rtFeatures.rayTracingPipeline = VK_TRUE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures{};
    asFeatures.sType                 = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    asFeatures.accelerationStructure = VK_TRUE;
    asFeatures.pNext                 = &rtFeatures;

    VkPhysicalDeviceVulkan13Features features13{};
    features13.sType              = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering   = VK_TRUE;
    features13.synchronization2   = VK_TRUE;
    features13.pNext              = &asFeatures;

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType                                  = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress                    = VK_TRUE;
    features12.descriptorIndexing                     = VK_TRUE;
    features12.runtimeDescriptorArray                 = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.scalarBlockLayout                      = VK_TRUE;
    features12.pNext                                  = &features13;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType               = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features.shaderInt64 = VK_TRUE;
    features2.pNext                = &features12;

    VkDeviceCreateInfo ci{};
    ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.pNext                   = &features2;
    ci.queueCreateInfoCount    = static_cast<uint32_t>(queueCIs.size());
    ci.pQueueCreateInfos       = queueCIs.data();
    ci.enabledExtensionCount   = static_cast<uint32_t>(kDeviceExtensions.size());
    ci.ppEnabledExtensionNames = kDeviceExtensions.data();

    VK_CHECK(vkCreateDevice(ctx.physicalDevice, &ci, nullptr, &ctx.device));

    vkGetDeviceQueue(ctx.device, ctx.queueFamilies.graphicsFamily.value(), 0, &ctx.graphicsQueue);
    vkGetDeviceQueue(ctx.device, ctx.queueFamilies.presentFamily.value(),  0, &ctx.presentQueue);
}

static void createAllocator(VkContext& ctx) {
    VmaAllocatorCreateInfo ci{};
    ci.physicalDevice   = ctx.physicalDevice;
    ci.device           = ctx.device;
    ci.instance         = ctx.instance;
    ci.vulkanApiVersion = VK_API_VERSION_1_3;
    ci.flags            = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

    VK_CHECK(vmaCreateAllocator(&ci, &ctx.allocator));
}

static void queryRTProperties(VkContext& ctx) {
    ctx.rtPipelineProperties = {};
    ctx.rtPipelineProperties.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &ctx.rtPipelineProperties;
    vkGetPhysicalDeviceProperties2(ctx.physicalDevice, &props2);

    ctx.asFeatures = {};
    ctx.asFeatures.sType =
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    VkPhysicalDeviceFeatures2 f2{};
    f2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    f2.pNext = &ctx.asFeatures;
    vkGetPhysicalDeviceFeatures2(ctx.physicalDevice, &f2);
}

// ─── RT function loading ────────────────────────────────────────────────────

void loadRTFunctions(VkDevice device) {
    #define LOAD_FN(var, name) var = reinterpret_cast<decltype(var)>(vkGetDeviceProcAddr(device, #name))
    LOAD_FN(rtCreateAccelerationStructure,  vkCreateAccelerationStructureKHR);
    LOAD_FN(rtDestroyAccelerationStructure, vkDestroyAccelerationStructureKHR);
    LOAD_FN(rtGetASBuildSizes,              vkGetAccelerationStructureBuildSizesKHR);
    LOAD_FN(rtGetASDeviceAddress,           vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_FN(rtCmdBuildAS,                   vkCmdBuildAccelerationStructuresKHR);
    LOAD_FN(rtCreatePipelines,              vkCreateRayTracingPipelinesKHR);
    LOAD_FN(rtGetShaderGroupHandles,        vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_FN(rtCmdTraceRays,                 vkCmdTraceRaysKHR);
    #undef LOAD_FN
}

// ─── Public API ─────────────────────────────────────────────────────────────

void VkContext::init(GLFWwindow* window) {
    createInstance(*this);
    setupDebugMessenger(*this);
    createSurface(*this, window);
    pickPhysicalDevice(*this);
    createLogicalDevice(*this);
    loadRTFunctions(device);
    createAllocator(*this);
    queryRTProperties(*this);

    std::cout << "[RT] maxRayRecursionDepth = "
              << rtPipelineProperties.maxRayRecursionDepth
              << ", shaderGroupHandleSize = "
              << rtPipelineProperties.shaderGroupHandleSize << "\n";
}

void VkContext::cleanup() {
    if (allocator) { vmaDestroyAllocator(allocator); allocator = VK_NULL_HANDLE; }
    if (device)    { vkDestroyDevice(device, nullptr); device = VK_NULL_HANDLE; }

    if (kEnableValidation && debugMessenger) {
        auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (func) func(instance, debugMessenger, nullptr);
        debugMessenger = VK_NULL_HANDLE;
    }

    if (surface)  { vkDestroySurfaceKHR(instance, surface, nullptr); surface = VK_NULL_HANDLE; }
    if (instance) { vkDestroyInstance(instance, nullptr); instance = VK_NULL_HANDLE; }
}
