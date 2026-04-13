#define TINYOBJLOADER_IMPLEMENTATION
#include "obj_loader.h"
#include "tiny_obj_loader.h"
#include <unordered_map>
#include <stdexcept>

struct PairHash {
    size_t operator()(const std::pair<int,int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 16);
    }
};

Mesh loadOBJ(const std::string& path) {
    tinyobj::ObjReader reader;
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;

    if (!reader.ParseFromFile(path, config))
        throw std::runtime_error("OBJ load failed: " + reader.Error());

    if (!reader.Warning().empty())
        std::cerr << "[OBJ] " << reader.Warning() << std::endl;

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    Mesh mesh;
    std::unordered_map<std::pair<int,int>, uint32_t, PairHash> uniqueVerts;

    for (const auto& shape : shapes) {
        size_t indexOffset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            int fv = shape.mesh.num_face_vertices[f];

            glm::vec3 facePositions[3];
            int       faceVidx[3], faceNidx[3];

            for (int v = 0; v < fv; v++) {
                auto idx = shape.mesh.indices[indexOffset + v];
                faceVidx[v] = idx.vertex_index;
                faceNidx[v] = idx.normal_index;
                facePositions[v] = {
                    attrib.vertices[3 * idx.vertex_index + 0],
                    attrib.vertices[3 * idx.vertex_index + 1],
                    attrib.vertices[3 * idx.vertex_index + 2],
                };
            }

            glm::vec3 faceNormal = glm::normalize(
                glm::cross(facePositions[1] - facePositions[0],
                           facePositions[2] - facePositions[0]));

            for (int v = 0; v < fv; v++) {
                glm::vec3 normal;
                if (faceNidx[v] >= 0) {
                    normal = {
                        attrib.normals[3 * faceNidx[v] + 0],
                        attrib.normals[3 * faceNidx[v] + 1],
                        attrib.normals[3 * faceNidx[v] + 2],
                    };
                } else {
                    normal = faceNormal;
                }

                auto key = std::make_pair(faceVidx[v], faceNidx[v]);
                auto it = uniqueVerts.find(key);
                if (it != uniqueVerts.end()) {
                    mesh.indices.push_back(it->second);
                } else {
                    uint32_t newIdx = static_cast<uint32_t>(mesh.vertices.size());
                    mesh.vertices.push_back({facePositions[v], normal});
                    mesh.indices.push_back(newIdx);
                    uniqueVerts[key] = newIdx;
                }
            }
            indexOffset += fv;
        }
    }

    std::cout << "[OBJ] Loaded " << path << ": "
              << mesh.vertices.size() << " vertices, "
              << mesh.indices.size() / 3 << " triangles" << std::endl;
    return mesh;
}
