#version 450
#extension GL_ARB_separate_shader_objects : enable

out gl_PerVertex 
{
    vec4 gl_Position;
};

layout(location = 0) out vec3 color;

// draws a fullscreen quad
const vec2 positions[6] = vec2[](vec2(-1.0,  1.0),   // lower left
								 vec2(-1.0, -1.0),   // upper left
								 vec2( 1.0, -1.0),   // upper right

								 vec2(-1.0,  1.0),   // lower left
								 vec2( 1.0, -1.0),   // upper right
								 vec2( 1.0,  1.0));  // lower right

const vec3 colors[6] = vec3[](vec3(1.0, 0.0, 0.0),
					          vec3(0.0, 1.0, 0.0),
					          vec3(0.0, 0.0, 1.0),
					          vec3(1.0, 0.0, 0.0),
					          vec3(0.0, 1.0, 0.0),
					          vec3(0.0, 0.0, 1.0));

void main() 
{
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);

    color = colors[gl_VertexIndex];
}