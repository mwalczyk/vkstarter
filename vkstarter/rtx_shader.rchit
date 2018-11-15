#version 460
#extension GL_NVX_raytracing : require

layout(location = 0) rayPayloadInNVX vec3 result_color;
layout(location = 1) hitAttributeNVX vec2 hit_attribute;

layout(push_constant) uniform PushConstants 
{
	float time;
	float padding;
	vec2 resolution;
} push_constants;

void main() 
{
	float modulate = sin(push_constants.time) * 0.5 + 0.5;

    const vec3 barycentrics = vec3(1.0f - hit_attribute.x - hit_attribute.y, hit_attribute.x, hit_attribute.y);
    result_color = vec3(barycentrics * modulate);
}
