#version 460
#extension GL_NVX_raytracing : require
#extension GL_GOOGLE_include_directive : require

#include "shared.glsl"

layout(location = RAY_PAYLOAD_LOC_SEC) rayPayloadInNVX RayPayloadSec payload;

void main() 
{
    payload.distance = MISS_DISTANCE;
}
