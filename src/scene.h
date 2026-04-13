#pragma once

#include "vk_buffer.h"
#include "vk_accel.h"
#include "camera.h"

struct Vertex {
    glm::vec3 pos;
    glm::vec3 normal;
};

struct Material {
    glm::vec4 color;
    glm::vec4 emission;
};

struct Scene {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
    std::vector<uint32_t> materialIndices;
    std::vector<Material> materials;

    uint32_t boxVertexCount = 0;
    uint32_t boxIndexCount  = 0;
    uint32_t boxTriCount    = 0;
    uint32_t bunnyTriCount  = 0;

    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    AllocatedBuffer materialIndexBuffer;
    AllocatedBuffer materialBuffer;
    AllocatedBuffer uniformBuffer;

    AccelStructure  boxBLAS;
    AccelStructure  bunnyBLAS;
    TLASBuildResult tlas;

    void buildCornellBox();
    void loadBunny(const std::string& path);
    void upload(VkDevice device, VmaAllocator allocator,
                CommandManager& commands, VkQueue queue);
    void buildAccel(VkDevice device, VmaAllocator allocator,
                    CommandManager& commands, VkQueue queue);
    void cleanup(VkDevice device, VmaAllocator allocator);
};
