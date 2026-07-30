#version 450
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_debug_printf : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 screen_Res;
layout(location = 2) in float time;
layout(location = 3) in vec2 uv;
layout(location = 4) flat in int fragTexIdx;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSampler[];


vec3 colormap(float u, float maxAmp)
{
    float x = clamp(u / maxAmp, -1.0, 1.0);
    if (x < 0.0)
        return mix(vec3(0.0, 0.0, 1.0), vec3(0.4, 0.4, 0.4), smoothstep(-1.0, 0.0, x));
    else
        return mix(vec3(0.4, 0.4, 0.4), vec3(1.0, 0.0, 0.0), smoothstep(0.0, 1.0, x));
}

vec3 Map_Jet_JoshuaFraser(float t) {
    return clamp((vec3(1.5) - abs(4.0 * vec3(t) + vec3(-3, -2, -1))), 0.0, 1.0);
}

vec3 TurboColormap(in float x) {
    const vec4 kRedVec4 = vec4(0.13572138, 4.61539260, -42.66032258, 132.13108234);
    const vec4 kGreenVec4 = vec4(0.09140261, 2.19418839, 4.84296658, -14.18503333);
    const vec4 kBlueVec4 = vec4(0.10667330, 12.64194608, -60.58204836, 110.36276771);
    const vec2 kRedVec2 = vec2(-152.94239396, 59.28637943);
    const vec2 kGreenVec2 = vec2(4.27729857, 2.82956604);
    const vec2 kBlueVec2 = vec2(-89.90310912, 27.34824973);

    x = clamp(x, 0, 1);
    vec4 v4 = vec4(1.0, x, x * x, x * x * x);
    vec2 v2 = v4.zw * v4.z;

    return vec3(
        dot(v4, kRedVec4) + dot(v2, kRedVec2),
        dot(v4, kGreenVec4) + dot(v2, kGreenVec2),
        dot(v4, kBlueVec4) + dot(v2, kBlueVec2)
    );
}   

void main() {
    outColor = vec4(fragColor, 1.0);

    if (fragTexIdx == 0)
    {
        vec4 val = texture(texSampler[fragTexIdx], uv);
        outColor = vec4(TurboColormap(abs(val.r) ), 1.0);

        if (val.a > 0)
        {
            outColor = vec4(vec3(1,1,1) * val.a, 1.0);            
        }
    }
    else if (fragTexIdx > 0)
    {
        outColor = vec4(texture(texSampler[fragTexIdx], uv).rgb * fragColor, texture(texSampler[fragTexIdx], uv).a);
    }
}