#define RAY_PAYLOAD_LOC_PRI 0
#define RAY_PAYLOAD_LOC_SEC 1
#define HIT_SHADER_PRI 0
#define HIT_SHADER_SEC 1
#define MISS_SHADER_PRI 0
#define MISS_SHADER_SEC 1
#define MAX_RECURSION_DEPTH 10

#define BINDING_ACCELERATION_STRUCTURE 0
#define BINDING_STORAGE_IMAGE 1
#define BINDING_NORMALS_STORAGE_BUFFERS 2
#define BINDING_PRIMITIVES_STORAGE_BUFFERS 3

#define MATERIAL_REFLECTIVE 0
#define MATERIAL_REFRACTIVE 1
#define MATERIAL_LAMBERTIAN 2

#define MISS_DISTANCE -1.0f

#define MOTION_RANGE 4.0f
#define AMBIENT_CONTRIBUTION 0.25f
#define CURSOR_CONTROLS

const vec3 black = { 0.0f, 0.0f, 0.0f };
const vec3 white = { 1.0f, 1.0f, 1.0f };
const vec3 red = { 1.0f, 0.0f, 0.0f };
const vec3 green = { 0.0f, 1.0f, 0.0f };
const vec3 blue = { 0.0f, 0.0f, 1.0f };
const vec3 origin = { 0.0f, 0.0f, 0.0f };
const vec3 world_up = { 0.0f, -1.0f, 0.0f };

struct RayPayloadPri
{
    vec3 color;
    vec3 normal;
    float distance;
    float object_id;
    float instance_id;
};

struct RayPayloadSec
{
    float distance;
};

float psin(float x) { return sin(x) * 0.5 + 0.5; }
float pcos(float x) { return cos(x) * 0.5 + 0.5; }
void gamma(inout vec3 linear) { linear = pow(linear, vec3(1.0 / 2.2)); }

vec3 barycentric_lerp(in vec3 a, in vec3 b, in vec3 c, in vec3 bc_coords) 
{
    return a * bc_coords.x + b * bc_coords.y + c * bc_coords.z;
}

float rand(in vec2 seed) { return fract(sin(dot(seed.xy, vec2(12.9898,78.233))) * 43758.5453); }

vec3 rand_vec(in vec2 seed) 
{ 
	return vec3(rand(seed + 0), 
				rand(seed + 1), 
				rand(seed + 2)); 
}

// See: https://github.com/KhronosGroup/GLSL/blob/master/extensions/nv/GLSL_NV_ray_tracing.txt
// See: https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GL_EXT_nonuniform_qualifier.txt