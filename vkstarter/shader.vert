#version 460
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex 
{
    vec4 gl_Position;
};

layout(location = 0) in vec3 position;

layout(location = 0) out vec3 color;

void main() 
{
	// Render the geometry orthographically for now
	vec3 scaled_position = position;
	scaled_position.z = 0.0f;

	// Calculate fragment color from model-space position
	color = position;

    gl_Position = vec4(scaled_position, 1.0); 
}