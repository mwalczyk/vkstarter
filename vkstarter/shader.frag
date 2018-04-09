#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 color;

layout(location = 0) out vec4 o_color;

layout(push_constant) uniform PushConstants 
{
	float time;
} push_constants;

void main() 
{
	float modulate = sin(push_constants.time) * 0.5 + 0.5;

    o_color = vec4(color * modulate, 1.0);
}