#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec3 pos;
layout(location = 1) in vec4 col;
layout(location = 2) in vec2 uv;
layout(location = 3) in int texIdx;

layout(binding = 0) uniform _UBO
{
    mat4 proj;
    int window_width;
    int window_height;
    float show;
    float time;
} ubo;

layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 fragUV;
layout(location = 2) out int fragTexIdx;
layout(location = 3) out vec2 window;
layout(location = 4) out float fragshow;

void main() {
    window = vec2(ubo.window_width, ubo.window_height);
    fragColor = col;
    fragUV = (texIdx == 0) ? ((ubo.proj * vec4(pos, 1.0)).xy + vec2(1)) * 0.5 : uv;
    fragTexIdx = texIdx;
    fragshow = ubo.show;

    gl_Position = ubo.proj * vec4(pos, 1.0);
}
