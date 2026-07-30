#version 450
#extension GL_EXT_nonuniform_qualifier : enable

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) flat in int fragTexIdx;
layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler[];

void main()
{
    vec4 tex = texture(texSampler[fragTexIdx], uv);
    outColor = fragColor * tex;
}