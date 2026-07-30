#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 screen_Res;
layout(location = 2) in vec2 uv;
layout(location = 3) flat in int fragTexIdx;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D frameBuffer;
layout(binding = 2) uniform sampler2D textures[];


void main() {
    outColor = fragColor;

    if(fragTexIdx == -1)
    {
        vec4 val = texture(frameBuffer, uv);
        outColor = val * fragColor;    
    }

    if (fragTexIdx >= 0)
    {
        vec4 val = texture(textures[fragTexIdx], uv);
        outColor = val * fragColor;
    }
}