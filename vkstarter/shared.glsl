#define RAY_PAYLOAD_LOC_PRI 0
#define RAY_PAYLOAD_LOC_SEC 1
#define HIT_SHADER_PRI 0
#define MISS_SHADER_PRI 0
#define HIT_SHADER_SEC 1
#define MISS_SHADER_SEC 1

#define MOTION_RANGE 4.0f
//#define CURSOR_CONTROLS

const vec3 black = { 0.0f, 0.0f, 0.0f };
const vec3 white = { 1.0f, 1.0f, 1.0f };
const vec3 red = { 1.0f, 0.0f, 0.0f };
const vec3 blue = { 0.0f, 1.0f, 0.0f };
const vec3 green = { 0.0f, 0.0f, 1.0f };

const vec3 origin = { 0.0f, 0.0f, 0.0f };
const vec3 world_up = { 0.0f, -1.0f, 0.0f };

struct RayPayloadPri
{
    vec3 color;
    vec3 normal;
    float distance;
    float object_id;
};

struct RayPayloadSec
{
    float distance;
};

float psin(float x) { return sin(x) * 0.5 + 0.5; }
float pcos(float x) { return cos(x) * 0.5 + 0.5; }

// See: https://github.com/KhronosGroup/GLSL/blob/master/extensions/nv/GLSL_NV_ray_tracing.txt