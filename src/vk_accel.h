#pragma once

#include "vk_buffer.h"

struct CommandManager;

struct AccelStructure {
    VkAccelerationStructureKHR handle = VK_NULL_HANDLE;
    AllocatedBuffer            buffer;
    VkDeviceAddress            deviceAddress = 0;
};

// Build BLAS from indexed triangle mesh. Blocks until complete.
AccelStructure buildBLAS(
    VkDevice device, VmaAllocator allocator, CommandManager& commands, VkQueue queue,
    VkDeviceAddress vertexAddress, uint32_t vertexCount, VkDeviceSize vertexStride,
    VkDeviceAddress indexAddress, uint32_t triangleCount,
    VkGeometryFlagsKHR geometryFlags = VK_GEOMETRY_OPAQUE_BIT_KHR);

struct TLASBuildResult {
    AccelStructure  accel;
    AllocatedBuffer instanceBuffer;   // host-visible, kept for per-frame updates
    AllocatedBuffer scratchBuffer;    // kept when allowUpdate=true
};

// Build TLAS from instances. If allowUpdate=true, keeps scratch for in-place updates.
TLASBuildResult buildTLAS(
    VkDevice device, VmaAllocator allocator, CommandManager& commands, VkQueue queue,
    const std::vector<VkAccelerationStructureInstanceKHR>& instances,
    bool allowUpdate = false);

// Record TLAS update into command buffer (caller must update instance data beforehand).
void cmdUpdateTLAS(VkCommandBuffer cmd,
    AccelStructure& tlas,
    AllocatedBuffer& instanceBuffer,
    AllocatedBuffer& scratchBuffer,
    uint32_t instanceCount);

void destroyAccelStructure(VkDevice device, VmaAllocator allocator, AccelStructure& as);
