#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "shared.glsl"

layout(location = RAY_PAYLOAD_LOC_PRI) rayPayloadInNVX RayPayloadPri payload;

void main() 
{	
	// Create a sky backdrop using the current ray's y-coordinate
	vec3 unit_ray_direction = normalize(gl_WorldRayDirectionNVX);
	float t = unit_ray_direction.y * 0.5f + 0.5f;

	const vec3 atmosp = { 0.0f, 0.2f, 1.0f };
	const vec3 clouds = white;
	vec3 sky = mix(atmosp, white, t);

    payload.color = sky;
    payload.normal = vec3(0.0f);
    payload.distance = MISS_DISTANCE;
 	payload.object_id = 0.0f;   
}
