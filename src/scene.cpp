#include "scene.h"
#include "vk_commands.h"
#include "obj_loader.h"
#include <cfloat>

void Scene::buildCornellBox() {
    materials = {
        {{0.73f, 0.73f, 0.73f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},   // 0: white
        {{0.65f, 0.05f, 0.05f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},   // 1: red
        {{0.12f, 0.45f, 0.15f, 1.0f}, {0.0f, 0.0f, 0.0f, 0.0f}},   // 2: green
        {{0.78f, 0.78f, 0.78f, 1.0f}, {17.0f, 12.0f, 4.0f, 0.0f}},  // 3: light
    };

    auto addQuad = [&](glm::vec3 v0, glm::vec3 v1, glm::vec3 v2, glm::vec3 v3,
                       glm::vec3 n, uint32_t mat) {
        uint32_t base = static_cast<uint32_t>(vertices.size());
        vertices.push_back({v0, n});
        vertices.push_back({v1, n});
        vertices.push_back({v2, n});
        vertices.push_back({v3, n});
        indices.insert(indices.end(), {base, base+1, base+2, base, base+2, base+3});
        materialIndices.push_back(mat);
        materialIndices.push_back(mat);
    };

    addQuad({-1,-1,-1}, { 1,-1,-1}, { 1,-1, 1}, {-1,-1, 1}, { 0, 1, 0}, 0); // floor
    addQuad({-1, 1,-1}, {-1, 1, 1}, { 1, 1, 1}, { 1, 1,-1}, { 0,-1, 0}, 0); // ceiling
    addQuad({-1,-1,-1}, {-1, 1,-1}, { 1, 1,-1}, { 1,-1,-1}, { 0, 0, 1}, 0); // back wall
    addQuad({-1,-1, 1}, {-1, 1, 1}, {-1, 1,-1}, {-1,-1,-1}, { 1, 0, 0}, 1); // left (red)
    addQuad({ 1,-1,-1}, { 1, 1,-1}, { 1, 1, 1}, { 1,-1, 1}, {-1, 0, 0}, 2); // right (green)

    addQuad({-0.25f, 0.999f, -0.25f}, { 0.25f, 0.999f, -0.25f},
            { 0.25f, 0.999f,  0.25f}, {-0.25f, 0.999f,  0.25f},
            {0, -1, 0}, 3); // area light

    // 4: blue, 5: yellow
    materials.push_back({{0.10f, 0.20f, 0.70f, 1.0f}, {0,0,0,0}});
    materials.push_back({{0.80f, 0.70f, 0.10f, 1.0f}, {0,0,0,0}});

    auto addRotatedBox = [&](glm::vec3 center, glm::vec3 halfExt,
                              float angleDeg, uint32_t mat) {
        float rad = glm::radians(angleDeg);
        float co = std::cos(rad), si = std::sin(rad);
        auto rotY = [co, si](glm::vec3 v) -> glm::vec3 {
            return {v.x * co + v.z * si, v.y, -v.x * si + v.z * co};
        };

        float hx = halfExt.x, hy = halfExt.y, hz = halfExt.z;
        glm::vec3 p[8] = {
            {-hx,-hy,-hz},{ hx,-hy,-hz},{ hx,-hy, hz},{-hx,-hy, hz},
            {-hx, hy,-hz},{ hx, hy,-hz},{ hx, hy, hz},{-hx, hy, hz}
        };
        for (auto& v : p) v = rotY(v) + center;

        glm::vec3 nF = rotY({0,0,1}),  nB = rotY({0,0,-1});
        glm::vec3 nL = rotY({-1,0,0}), nR = rotY({1,0,0});

        addQuad(p[3],p[2],p[6],p[7], nF, mat);
        addQuad(p[1],p[0],p[4],p[5], nB, mat);
        addQuad(p[0],p[3],p[7],p[4], nL, mat);
        addQuad(p[2],p[1],p[5],p[6], nR, mat);
        addQuad(p[7],p[6],p[5],p[4], {0,1,0}, mat);
        addQuad(p[0],p[1],p[2],p[3], {0,-1,0}, mat);
    };

    addRotatedBox({-0.33f,-0.40f,-0.35f}, {0.225f,0.60f,0.225f},  18.0f, 4);
    addRotatedBox({ 0.33f,-0.70f,-0.20f}, {0.225f,0.30f,0.225f}, -15.0f, 5);
}

void Scene::loadBunny(const std::string& path) {
    Mesh mesh = loadOBJ(path);

    glm::vec3 mn(FLT_MAX), mx(-FLT_MAX);
    for (auto& v : mesh.vertices) {
        mn = glm::min(mn, v.pos);
        mx = glm::max(mx, v.pos);
    }
    glm::vec3 center = (mn + mx) * 0.5f;
    float maxDim = std::max({mx.x - mn.x, mx.y - mn.y, mx.z - mn.z});
    float scale = 1.0f / maxDim;

    float newMinY = FLT_MAX;
    for (auto& v : mesh.vertices) {
        v.pos = (v.pos - center) * scale;
        newMinY = std::min(newMinY, v.pos.y);
    }
    for (auto& v : mesh.vertices)
        v.pos.y -= newMinY;

    boxVertexCount = static_cast<uint32_t>(vertices.size());
    boxIndexCount  = static_cast<uint32_t>(indices.size());
    boxTriCount    = boxIndexCount / 3;

    uint32_t bunnyMatIdx = static_cast<uint32_t>(materials.size());
    materials.push_back({{0.95f, 0.95f, 0.95f, 1.0f}, {0.0f, 0.0f, 0.0f, 1.5f}});

    for (auto& idx : mesh.indices)
        idx += boxVertexCount;

    vertices.insert(vertices.end(), mesh.vertices.begin(), mesh.vertices.end());
    indices.insert(indices.end(), mesh.indices.begin(), mesh.indices.end());

    bunnyTriCount = static_cast<uint32_t>(mesh.indices.size()) / 3;
    for (uint32_t t = 0; t < bunnyTriCount; t++)
        materialIndices.push_back(bunnyMatIdx);
}

void Scene::upload(VkDevice device, VmaAllocator allocator,
                   CommandManager& commands, VkQueue queue)
{
    VkBufferUsageFlags asInput =
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR
        | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    vertexBuffer = uploadBuffer(allocator, device, commands, queue,
        vertices.data(), vertices.size() * sizeof(Vertex),
        asInput | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    indexBuffer = uploadBuffer(allocator, device, commands, queue,
        indices.data(), indices.size() * sizeof(uint32_t),
        asInput | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    materialIndexBuffer = uploadBuffer(allocator, device, commands, queue,
        materialIndices.data(), materialIndices.size() * sizeof(uint32_t),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    materialBuffer = uploadBuffer(allocator, device, commands, queue,
        materials.data(), materials.size() * sizeof(Material),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);

    uniformBuffer = createBuffer(allocator, device, sizeof(CameraUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT);
}

void Scene::buildAccel(VkDevice device, VmaAllocator allocator,
                       CommandManager& commands, VkQueue queue)
{
    uint32_t totalVertexCount = static_cast<uint32_t>(vertices.size());

    boxBLAS = buildBLAS(device, allocator, commands, queue,
        vertexBuffer.deviceAddress,
        totalVertexCount, sizeof(Vertex),
        indexBuffer.deviceAddress, boxTriCount);

    VkDeviceAddress bunnyIndexAddr = indexBuffer.deviceAddress
                                   + boxIndexCount * sizeof(uint32_t);
    bunnyBLAS = buildBLAS(device, allocator, commands, queue,
        vertexBuffer.deviceAddress,
        totalVertexCount, sizeof(Vertex),
        bunnyIndexAddr, bunnyTriCount);

    VkTransformMatrixKHR identity = {1,0,0,0, 0,1,0,0, 0,0,1,0};
    VkTransformMatrixKHR bunnyXform = {1,0,0,0, 0,1,0,-1, 0,0,1,0.35f};

    VkAccelerationStructureInstanceKHR instances[2]{};

    instances[0].transform                              = identity;
    instances[0].instanceCustomIndex                    = 0;
    instances[0].mask                                   = 0xFF;
    instances[0].instanceShaderBindingTableRecordOffset = 0;
    instances[0].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instances[0].accelerationStructureReference         = boxBLAS.deviceAddress;

    instances[1].transform                              = bunnyXform;
    instances[1].instanceCustomIndex                    = boxTriCount;
    instances[1].mask                                   = 0xFF;
    instances[1].instanceShaderBindingTableRecordOffset = 0;
    instances[1].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instances[1].accelerationStructureReference         = bunnyBLAS.deviceAddress;

    tlas = buildTLAS(device, allocator, commands, queue,
        {instances[0], instances[1]}, true);
}

void Scene::cleanup(VkDevice device, VmaAllocator allocator) {
    destroyBuffer(allocator, tlas.scratchBuffer);
    destroyBuffer(allocator, tlas.instanceBuffer);
    destroyAccelStructure(device, allocator, tlas.accel);
    destroyAccelStructure(device, allocator, bunnyBLAS);
    destroyAccelStructure(device, allocator, boxBLAS);
    destroyBuffer(allocator, uniformBuffer);
    destroyBuffer(allocator, materialBuffer);
    destroyBuffer(allocator, materialIndexBuffer);
    destroyBuffer(allocator, indexBuffer);
    destroyBuffer(allocator, vertexBuffer);
}
