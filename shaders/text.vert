#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec2 pos;
layout(location = 1) in vec2 instPos;
layout(location = 2) in vec2 instSize;
layout(location = 3) in vec4 uvBBox;
layout(location = 4) in int texIdx;
layout(location = 5) in int transformIdx;

struct transform
{
    float r;
    float g;
    float b;
    float a;
    float x;
    float y;
};

layout(std430, binding = 0) readonly buffer TransformBuffer
{
    transform ts[];
};

layout( push_constant ) uniform _UBO
{
    mat4 proj;
} ubo;


layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out int fragTexIdx;

void main() {
    fragColor = vec4(ts[transformIdx].r, ts[transformIdx].g, ts[transformIdx].b, ts[transformIdx].a);
    fragUV = mix(uvBBox.xy, uvBBox.zw, pos);
    fragTexIdx = texIdx;

    vec2 world = pos * instSize + instPos + vec2(ts[transformIdx].x, ts[transformIdx].y);

    gl_Position = ubo.proj * vec4(world, 0.0, 1.0);
}