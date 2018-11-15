#version 460
#extension GL_NVX_raytracing : require

layout(location = 0) rayPayloadInNVX vec3 result_color;

void main() 
{
	const vec3 background = vec3(0.0);
    result_color = background;
}
