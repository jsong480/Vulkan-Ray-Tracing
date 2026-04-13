#pragma once

#include "scene.h"
#include <string>

struct Mesh {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
};

Mesh loadOBJ(const std::string& path);
