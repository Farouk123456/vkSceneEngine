#version 450

layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 uv;
layout(location = 2) flat in int fragTexIdx;
layout(location = 3) in vec2 window;
layout(location = 4) in float fragshow;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D frameBuffer;

// Tunable blur radius
const float radius = 4.0;

// Poisson-like circular sampling pattern
const vec2 offsets[12] = vec2[](
    vec2( 0.326,  0.406),
    vec2( 0.840,  0.074),
    vec2( 0.696,  0.457),
    vec2(-0.204,  0.621),
    vec2(-0.322, -0.933),
    vec2( 0.171, -0.826),
    vec2(-0.781, -0.154),
    vec2(-0.559,  0.339),
    vec2( 0.391, -0.206),
    vec2( 0.707, -0.568),
    vec2(-0.122, -0.310),
    vec2( 0.045,  0.128)
);

// Pass aspectRatio (width / height) as a uniform or parameter
float box(vec2 position, vec2 halfSize, float cornerRadius) {
    // Standard SDF rounded box calculation
    position = abs(position) - halfSize + cornerRadius;
    return length(max(position, 0.0)) + min(max(position.x, position.y), 0.0) - cornerRadius;
}   


void main()
{
    if (fragTexIdx == 0)
    {
        float aspect = window.y / window.x;
        vec2 p = 2*uv - vec2(1);
        p.x /= aspect;
        float d = box(p, vec2(1 / aspect - 0.075, 0.925), 0.1);

        if (d > 0)
        {
            float t = clamp((d - 0.01) / 0.0025, 0.f, 1.f);
            outColor = vec4(mix(vec3(1), texture(frameBuffer, uv).rgb, t), fragshow);
            return;
        }

        vec2 texel = radius / window;

        vec4 sum = texture(frameBuffer, uv);
        float weight = 1.0;

        // 12 sample blur ring
        for (int i = 0; i < 12; i++)
        {
            vec2 off = offsets[i] * texel;
            sum += texture(frameBuffer, uv + off);
            weight += 1.0;
        }

        float t = clamp(-d / 0.0025, 0.f, 1.f);
        outColor = vec4(mix(vec3(1), (sum / weight).rgb * 0.3 + vec3(0.05), t), fragColor.a * fragshow);

        return;
    }

    outColor = vec4(fragColor.rgb, fragColor.a * fragshow);
}
