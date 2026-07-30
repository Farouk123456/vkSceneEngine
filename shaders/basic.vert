#version 450
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
    float t;
} ubo;


layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec2 screen_Res;
layout(location = 2) out vec2 fragUV;
layout(location = 3) out int fragTexIdx;

void main()
{
    fragColor = col * vec4(1,1,1, ubo.t);
    screen_Res = vec2(ubo.window_width, ubo.window_height);
    fragUV = uv;
    fragTexIdx = texIdx;
    
    gl_Position = ubo.proj * vec4(pos, 1.0);
}