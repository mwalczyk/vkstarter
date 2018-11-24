#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "shared.glsl"

layout(location = RAY_PAYLOAD_LOC_PRI) rayPayloadInNVX RayPayloadPri payload;

void main() 
{
	const vec3 background = vec3(0.15, 0.15, 0.175);
	
    payload.color = background;
    payload.normal = vec3(0.0f);
    payload.distance = -1.0f;
 	payload.object_id = 0.0f;   
}
