#version 460
#extension GL_NVX_raytracing : require

layout(location = 0) rayPayloadInNVX vec3 result_color;

void main() 
{
    result_color = vec3(0.412f, 0.796f, 1.0f);
}
