#version 460
#extension GL_EXT_ray_tracing : require

layout(location = 1) rayPayloadInEXT float isShadowed;

void main() {
    isShadowed = 0.0;
}
