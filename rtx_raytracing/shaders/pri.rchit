#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#include "shared.glsl"

layout(location = RAY_PAYLOAD_LOC_PRI) rayPayloadInNVX RayPayloadPri payload;
layout(location = 1) hitAttributeNVX vec2 hit_attribute;

layout(set = 0, binding = BINDING_NORMALS_STORAGE_BUFFERS, std430) readonly buffer NormsBuffer 
{
    vec4 norms[];
} NormsArray[];

layout(set = 0, binding = BINDING_PRIMITIVES_STORAGE_BUFFERS, std430) readonly buffer PrimsBuffer 
{
    uvec4 prims[];
} PrimsArray[];

void main() 
{
    // Barycentric coordinates at hit location
    const vec3 barycentric_coords = vec3(1.0f - hit_attribute.x - hit_attribute.y, hit_attribute.x, hit_attribute.y);
    const vec3 hit_position = gl_WorldRayOriginNVX + gl_WorldRayDirectionNVX * gl_HitTNVX;
    
    // Below, the value of `gl_InstanceCustomIndexNVX` corresponds to:
    //
    // 0: spheres
    // 1: ground plane
    //
    // See the `Application` class for details
   
    // Access like (note that `gl_PrimitiveID` is the index of the triangle being processed):
    uvec4 current_prim = PrimsArray[nonuniformEXT(gl_InstanceCustomIndexNVX)].prims[gl_PrimitiveID];

    // The three normals that make up this triangle
    vec4 normal_0 = NormsArray[nonuniformEXT(gl_InstanceCustomIndexNVX)].norms[int(current_prim.x)];
    vec4 normal_1 = NormsArray[nonuniformEXT(gl_InstanceCustomIndexNVX)].norms[int(current_prim.y)];
    vec4 normal_2 = NormsArray[nonuniformEXT(gl_InstanceCustomIndexNVX)].norms[int(current_prim.z)];

    vec3 interpolated_normal = normalize(barycentric_lerp(normal_0.xyz, normal_1.xyz, normal_2.xyz, barycentric_coords)); 

    payload.color = rand_vec(vec2(gl_InstanceCustomIndexNVX + 1, gl_InstanceID));
    payload.normal = interpolated_normal;
    payload.distance = gl_HitTNVX;
    payload.object_id = float(gl_InstanceCustomIndexNVX);
    payload.instance_id = float(gl_InstanceID);
}
