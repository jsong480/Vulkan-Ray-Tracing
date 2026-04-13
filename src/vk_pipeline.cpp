#include "vk_pipeline.h"

// ─── Shader loading ─────────────────────────────────────────────────────────

VkShaderModule loadShaderModule(VkDevice device, const std::string& path) {
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open shader file: " + path);

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), static_cast<std::streamsize>(fileSize));

    VkShaderModuleCreateInfo ci{};
    ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.codeSize = code.size();
    ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &ci, nullptr, &module));
    return module;
}

// ─── RT Pipeline Builder ────────────────────────────────────────────────────

static VkRayTracingShaderGroupCreateInfoKHR makeGeneralGroup(uint32_t stageIndex) {
    VkRayTracingShaderGroupCreateInfoKHR g{};
    g.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    g.type               = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
    g.generalShader      = stageIndex;
    g.closestHitShader   = VK_SHADER_UNUSED_KHR;
    g.anyHitShader       = VK_SHADER_UNUSED_KHR;
    g.intersectionShader = VK_SHADER_UNUSED_KHR;
    return g;
}

static VkPipelineShaderStageCreateInfo makeStage(VkShaderStageFlagBits stage,
                                                  VkShaderModule module) {
    VkPipelineShaderStageCreateInfo s{};
    s.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    s.stage  = stage;
    s.module = module;
    s.pName  = "main";
    return s;
}

uint32_t RTPipelineBuilder::addRayGenShader(VkShaderModule module) {
    uint32_t idx = static_cast<uint32_t>(stages.size());
    stages.push_back(makeStage(VK_SHADER_STAGE_RAYGEN_BIT_KHR, module));
    groups.push_back(makeGeneralGroup(idx));
    return static_cast<uint32_t>(groups.size() - 1);
}

uint32_t RTPipelineBuilder::addMissShader(VkShaderModule module) {
    uint32_t idx = static_cast<uint32_t>(stages.size());
    stages.push_back(makeStage(VK_SHADER_STAGE_MISS_BIT_KHR, module));
    groups.push_back(makeGeneralGroup(idx));
    return static_cast<uint32_t>(groups.size() - 1);
}

uint32_t RTPipelineBuilder::addHitGroup(VkShaderModule closestHit,
    VkShaderModule anyHit, VkShaderModule intersection)
{
    VkRayTracingShaderGroupCreateInfoKHR g{};
    g.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
    g.type               = intersection != VK_NULL_HANDLE
        ? VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR
        : VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
    g.generalShader      = VK_SHADER_UNUSED_KHR;
    g.closestHitShader   = VK_SHADER_UNUSED_KHR;
    g.anyHitShader       = VK_SHADER_UNUSED_KHR;
    g.intersectionShader = VK_SHADER_UNUSED_KHR;

    if (closestHit != VK_NULL_HANDLE) {
        g.closestHitShader = static_cast<uint32_t>(stages.size());
        stages.push_back(makeStage(VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHit));
    }
    if (anyHit != VK_NULL_HANDLE) {
        g.anyHitShader = static_cast<uint32_t>(stages.size());
        stages.push_back(makeStage(VK_SHADER_STAGE_ANY_HIT_BIT_KHR, anyHit));
    }
    if (intersection != VK_NULL_HANDLE) {
        g.intersectionShader = static_cast<uint32_t>(stages.size());
        stages.push_back(makeStage(VK_SHADER_STAGE_INTERSECTION_BIT_KHR, intersection));
    }
    groups.push_back(g);
    return static_cast<uint32_t>(groups.size() - 1);
}

VkPipeline RTPipelineBuilder::build(VkDevice device, VkPipelineLayout layout,
    uint32_t maxRecursionDepth)
{
    VkRayTracingPipelineCreateInfoKHR ci{};
    ci.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    ci.stageCount                   = static_cast<uint32_t>(stages.size());
    ci.pStages                      = stages.data();
    ci.groupCount                   = static_cast<uint32_t>(groups.size());
    ci.pGroups                      = groups.data();
    ci.maxPipelineRayRecursionDepth = maxRecursionDepth;
    ci.layout                       = layout;

    VkPipeline pipeline;
    VK_CHECK(rtCreatePipelines(device,
        VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &ci, nullptr, &pipeline));
    return pipeline;
}

// ─── Shader Binding Table ───────────────────────────────────────────────────

static uint32_t alignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

void ShaderBindingTable::build(VkDevice device, VmaAllocator allocator,
    VkPipeline pipeline,
    const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& rtProps,
    uint32_t raygenCount, uint32_t missCount, uint32_t hitCount)
{
    uint32_t handleSize    = rtProps.shaderGroupHandleSize;
    uint32_t handleAligned = alignUp(handleSize, rtProps.shaderGroupHandleAlignment);
    uint32_t baseAlign     = rtProps.shaderGroupBaseAlignment;

    uint32_t raygenSize = alignUp(handleAligned * raygenCount, baseAlign);
    uint32_t missSize   = alignUp(handleAligned * missCount,   baseAlign);
    uint32_t hitSize    = alignUp(handleAligned * hitCount,    baseAlign);

    uint32_t totalGroups = raygenCount + missCount + hitCount;
    std::vector<uint8_t> handles(static_cast<size_t>(totalGroups) * handleSize);
    VK_CHECK(rtGetShaderGroupHandles(device, pipeline,
        0, totalGroups, handles.size(), handles.data()));

    auto makeSBTBuffer = [&](uint32_t regionSize) {
        return createBuffer(allocator, device, regionSize,
            VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR
            | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
    };

    auto copyHandles = [&](AllocatedBuffer& buf, uint32_t groupOffset, uint32_t count) {
        void* dst;
        vmaMapMemory(allocator, buf.allocation, &dst);
        auto* base = static_cast<uint8_t*>(dst);
        for (uint32_t i = 0; i < count; i++) {
            std::memcpy(base + i * handleAligned,
                        handles.data() + (groupOffset + i) * handleSize,
                        handleSize);
        }
        vmaUnmapMemory(allocator, buf.allocation);
    };

    raygenBuf = makeSBTBuffer(raygenSize);
    copyHandles(raygenBuf, 0, raygenCount);
    raygenRegion.deviceAddress = raygenBuf.deviceAddress;
    raygenRegion.stride        = raygenSize;   // raygen: stride == size
    raygenRegion.size          = raygenSize;

    missBuf = makeSBTBuffer(missSize);
    copyHandles(missBuf, raygenCount, missCount);
    missRegion.deviceAddress = missBuf.deviceAddress;
    missRegion.stride        = handleAligned;
    missRegion.size          = missSize;

    hitBuf = makeSBTBuffer(hitSize);
    copyHandles(hitBuf, raygenCount + missCount, hitCount);
    hitRegion.deviceAddress = hitBuf.deviceAddress;
    hitRegion.stride        = handleAligned;
    hitRegion.size          = hitSize;

    callableRegion = {};
}

void ShaderBindingTable::cleanup(VmaAllocator allocator) {
    destroyBuffer(allocator, raygenBuf);
    destroyBuffer(allocator, missBuf);
    destroyBuffer(allocator, hitBuf);
}
