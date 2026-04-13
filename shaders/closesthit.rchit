#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require

hitAttributeEXT vec2 attribs;

struct HitPayload {
    vec3 albedo;   float t;
    vec3 normal;   float pad0;
    vec3 emission; float ior;
};

layout(location = 0) rayPayloadInEXT HitPayload prd;

struct Vertex   { vec3 pos; vec3 normal; };
struct Material { vec4 color; vec4 emission; };

layout(binding = 3, set = 0, scalar) buffer VertexBuf   { Vertex   v[];  } verts;
layout(binding = 4, set = 0, scalar) buffer IndexBuf    { uint     i[];  } idx;
layout(binding = 5, set = 0, scalar) buffer MatIndexBuf { uint     mi[]; } matIdx;
layout(binding = 6, set = 0, scalar) buffer MaterialBuf { Material m[];  } mats;

void main() {
    uint tri = gl_PrimitiveID + gl_InstanceCustomIndexEXT;
    uint i0 = idx.i[tri * 3 + 0];
    uint i1 = idx.i[tri * 3 + 1];
    uint i2 = idx.i[tri * 3 + 2];

    vec3 bary = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal = normalize(
        verts.v[i0].normal * bary.x +
        verts.v[i1].normal * bary.y +
        verts.v[i2].normal * bary.z);

    Material mat = mats.m[matIdx.mi[tri]];

    prd.albedo   = mat.color.rgb;
    prd.normal   = normal;
    prd.emission = mat.emission.rgb;
    prd.t        = gl_HitTEXT;
    prd.ior      = mat.emission.a;
}
