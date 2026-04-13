#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#define VK_CHECK(x)                                                         \
    do {                                                                     \
        VkResult err = x;                                                    \
        if (err != VK_SUCCESS) {                                             \
            std::cerr << "[Vulkan Error] code " << static_cast<int>(err)     \
                      << " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::abort();                                                    \
        }                                                                    \
    } while (0)

// RT extension function pointers (loaded via vkGetDeviceProcAddr)
inline PFN_vkCreateAccelerationStructureKHR           rtCreateAccelerationStructure  = nullptr;
inline PFN_vkDestroyAccelerationStructureKHR          rtDestroyAccelerationStructure = nullptr;
inline PFN_vkGetAccelerationStructureBuildSizesKHR    rtGetASBuildSizes              = nullptr;
inline PFN_vkGetAccelerationStructureDeviceAddressKHR rtGetASDeviceAddress           = nullptr;
inline PFN_vkCmdBuildAccelerationStructuresKHR        rtCmdBuildAS                   = nullptr;
inline PFN_vkCreateRayTracingPipelinesKHR             rtCreatePipelines              = nullptr;
inline PFN_vkGetRayTracingShaderGroupHandlesKHR       rtGetShaderGroupHandles        = nullptr;
inline PFN_vkCmdTraceRaysKHR                          rtCmdTraceRays                 = nullptr;

void loadRTFunctions(VkDevice device);
