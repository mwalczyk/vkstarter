#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "shared.glsl"

layout(location = RAY_PAYLOAD_LOC_PRI) rayPayloadInNVX RayPayloadPri payload;
layout(location = 1) hitAttributeNVX vec2 hit_attribute;

void main() 
{
	// The index of the triangle being processed
	int triangle = gl_PrimitiveID; 

    const vec3 barycentrics = vec3(1.0f - hit_attribute.x - hit_attribute.y, hit_attribute.x, hit_attribute.y);

    vec3 hit_position = gl_WorldRayOriginNVX + gl_WorldRayDirectionNVX * gl_HitTNVX;

    // Record primary ray object ID
    // 0: icosphere
    // 1: ground plane
    //
    // For now, this works because the icosphere is centered at the origin
    vec3 normal = (gl_InstanceCustomIndexNVX == 0) ? normalize(hit_position) : vec3(0.0, -1.0, 0.0);

    payload.color = vec3(1.0);// vec3(barycentrics);
    payload.normal = normal;
    payload.distance = gl_HitTNVX;
    payload.object_id = float(gl_InstanceCustomIndexNVX);

}
