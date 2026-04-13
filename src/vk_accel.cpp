#include "vk_accel.h"
#include "vk_commands.h"

// ─── BLAS ───────────────────────────────────────────────────────────────────

AccelStructure buildBLAS(
    VkDevice device, VmaAllocator allocator, CommandManager& commands, VkQueue queue,
    VkDeviceAddress vertexAddress, uint32_t vertexCount, VkDeviceSize vertexStride,
    VkDeviceAddress indexAddress, uint32_t triangleCount,
    VkGeometryFlagsKHR geometryFlags)
{
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = vertexAddress;
    triangles.vertexStride = vertexStride;
    triangles.maxVertex    = vertexCount - 1;
    triangles.indexType    = indexAddress ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_NONE_KHR;
    triangles.indexData.deviceAddress = indexAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags              = geometryFlags;
    geometry.geometry.triangles = triangles;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    rtGetASBuildSizes(device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &triangleCount, &sizeInfo);

    AccelStructure as{};
    as.buffer = createBuffer(allocator, device, sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = as.buffer.buffer;
    createInfo.size   = sizeInfo.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VK_CHECK(rtCreateAccelerationStructure(device, &createInfo, nullptr, &as.handle));

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = as.handle;
    as.deviceAddress = rtGetASDeviceAddress(device, &addrInfo);

    auto scratch = createBuffer(allocator, device, sizeInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);

    buildInfo.dstAccelerationStructure  = as.handle;
    buildInfo.scratchData.deviceAddress = scratch.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = triangleCount;
    const auto* pRange = &rangeInfo;

    auto cmd = commands.beginSingleTime(device);
    rtCmdBuildAS(cmd, 1, &buildInfo, &pRange);
    commands.endSingleTime(device, queue, cmd);

    destroyBuffer(allocator, scratch);
    return as;
}

// ─── TLAS ───────────────────────────────────────────────────────────────────

TLASBuildResult buildTLAS(
    VkDevice device, VmaAllocator allocator, CommandManager& commands, VkQueue queue,
    const std::vector<VkAccelerationStructureInstanceKHR>& instances,
    bool allowUpdate)
{
    uint32_t     instanceCount = static_cast<uint32_t>(instances.size());
    VkDeviceSize instanceSize  = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;

    auto instanceBuffer = createBuffer(allocator, device, instanceSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);

    void* mapped;
    vmaMapMemory(allocator, instanceBuffer.allocation, &mapped);
    std::memcpy(mapped, instances.data(), instanceSize);
    vmaUnmapMemory(allocator, instanceBuffer.allocation);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.data.deviceAddress = instanceBuffer.deviceAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instancesData;

    VkBuildAccelerationStructureFlagsKHR flags =
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    if (allowUpdate) flags |= VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags         = flags;
    buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    rtGetASBuildSizes(device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instanceCount, &sizeInfo);

    AccelStructure as{};
    as.buffer = createBuffer(allocator, device, sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);

    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = as.buffer.buffer;
    createInfo.size   = sizeInfo.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VK_CHECK(rtCreateAccelerationStructure(device, &createInfo, nullptr, &as.handle));

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.accelerationStructure = as.handle;
    as.deviceAddress = rtGetASDeviceAddress(device, &addrInfo);

    VkDeviceSize scratchSize = allowUpdate
        ? (std::max)(sizeInfo.buildScratchSize, sizeInfo.updateScratchSize)
        : sizeInfo.buildScratchSize;
    auto scratchBuffer = createBuffer(allocator, device, scratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, 0);

    buildInfo.dstAccelerationStructure  = as.handle;
    buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = instanceCount;
    const auto* pRange = &rangeInfo;

    auto cmd = commands.beginSingleTime(device);
    rtCmdBuildAS(cmd, 1, &buildInfo, &pRange);
    commands.endSingleTime(device, queue, cmd);

    TLASBuildResult result{};
    result.accel          = as;
    result.instanceBuffer = instanceBuffer;

    if (allowUpdate) {
        result.scratchBuffer = scratchBuffer;
    } else {
        destroyBuffer(allocator, scratchBuffer);
    }
    return result;
}

// ─── TLAS in-place update ───────────────────────────────────────────────────

void cmdUpdateTLAS(VkCommandBuffer cmd,
    AccelStructure& tlas,
    AllocatedBuffer& instanceBuffer,
    AllocatedBuffer& scratchBuffer,
    uint32_t instanceCount)
{
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.data.deviceAddress = instanceBuffer.deviceAddress;

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instancesData;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags                    = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR
                                       | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR;
    buildInfo.srcAccelerationStructure = tlas.handle;
    buildInfo.dstAccelerationStructure = tlas.handle;
    buildInfo.geometryCount            = 1;
    buildInfo.pGeometries              = &geometry;
    buildInfo.scratchData.deviceAddress = scratchBuffer.deviceAddress;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = instanceCount;
    const auto* pRange = &rangeInfo;

    rtCmdBuildAS(cmd, 1, &buildInfo, &pRange);

    VkMemoryBarrier barrier{};
    barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
    barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, 1, &barrier, 0, nullptr, 0, nullptr);
}

// ─── Cleanup ────────────────────────────────────────────────────────────────

void destroyAccelStructure(VkDevice device, VmaAllocator allocator, AccelStructure& as) {
    if (as.handle) rtDestroyAccelerationStructure(device, as.handle, nullptr);
    destroyBuffer(allocator, as.buffer);
    as = {};
}
