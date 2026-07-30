#version 450

layout(location = 0) in vec3 pos;
layout(location = 1) in vec3 col;
layout(location = 2) in vec2 uv;
layout(location = 3) in int texIdx;


layout(binding = 0) uniform _UBO
{
    int window_width;
    int window_height;
    float time;
    float dt;
    float time_offset;
} ubo;


layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 screen_Res;
layout(location = 2) out float time;
layout(location = 3) out vec2 fragUV;
layout(location = 4) out int fragTexIdx;

void main() {
    fragColor = col;
    screen_Res = vec2(ubo.window_width, ubo.window_height);
    fragUV = uv;
    fragTexIdx = texIdx;
    time = ubo.time;
    
    gl_Position = vec4(pos, 1.0);
}