#version 460
#extension GL_EXT_ray_tracing : require

struct HitPayload {
    vec3 albedo;   float t;
    vec3 normal;   float pad0;
    vec3 emission; float pad1;
};

layout(location = 0) rayPayloadInEXT HitPayload prd;

void main() {
    prd.t = -1.0;
}
