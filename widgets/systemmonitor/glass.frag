#version 440
// SPDX-License-Identifier: LGPL-3.0-or-later
// Qt Quick adaptation of the Emerald Glass sheet in preview/glass.frag.
layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 cardSize;
    vec2 sampleOrigin;
    vec2 sampleAxisX;
    vec2 sampleAxisY;
    float cornerRadius;
    float refraction;
    float dispersion;
    float specular;
    float hoverGlow;
    float blurRadius;
    float materialOpacity;
    vec4 tintColor;
    float tintStrength;
    float adaptiveContrast;
    vec2 pointer;
};
layout(binding = 1) uniform sampler2D scene;

float distanceToCard(vec2 p)
{
    vec2 q = abs(p - cardSize * 0.5) - cardSize * 0.5 + cornerRadius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - cornerRadius;
}

vec3 backdrop(vec2 p)
{
    vec2 uv = sampleOrigin + p.x * sampleAxisX + p.y * sampleAxisY;
    vec2 stepX = blurRadius * sampleAxisX;
    vec2 stepY = blurRadius * sampleAxisY;
    vec4 c = texture(scene, uv) * 0.28;
    c += texture(scene, uv + stepX) * 0.18;
    c += texture(scene, uv - stepX) * 0.18;
    c += texture(scene, uv + stepY) * 0.18;
    c += texture(scene, uv - stepY) * 0.18;
    return c.a > 0.0001 ? c.rgb / c.a : vec3(0.04, 0.08, 0.10);
}

void main()
{
    vec2 p = qt_TexCoord0 * cardSize;
    float outer = distanceToCard(p);
    float coverage = 1.0 - smoothstep(-fwidth(outer), 0.0, outer);
    vec2 gradient = vec2(distanceToCard(p + vec2(0.45, 0.0)) - distanceToCard(p - vec2(0.45, 0.0)),
                         distanceToCard(p + vec2(0.0, 0.45)) - distanceToCard(p - vec2(0.0, 0.45)));
    vec2 direction = gradient / max(length(gradient), 0.0001);
    float depth = max(-outer, 0.0);
    float shoulder = 1.0 - smoothstep(0.0, 14.0, depth);
    vec2 bend = -direction * shoulder * refraction * 20.0;
    vec2 spectrum = direction * shoulder * dispersion * 11.2 * smoothstep(0.0, 0.4, refraction);
    vec3 color = vec3(backdrop(p + bend + spectrum).r,
                      backdrop(p + bend).g,
                      backdrop(p + bend - spectrum).b);
    vec3 accent = tintColor.rgb;
    color = mix(color, vec3(0.035, 0.075, 0.095), materialOpacity);
    color *= mix(vec3(1.0), accent * 0.8 + 0.2, tintStrength * 0.65);
    // Keep graphs readable on bright wallpaper while preserving its texture.
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    float excess = max(luma - 0.20, 0.0);
    float target = 0.20 + 0.10 * (1.0 - exp(-excess / 0.10));
    color *= mix(1.0, min(1.0, target / max(luma, 0.0001)), adaptiveContrast);

    float facing = abs(dot(direction, normalize(vec2(-0.55, -1.0))));
    float rim = exp(-pow((outer + 0.7) / 0.75, 2.0));
    color = mix(color, vec3(0.88, 0.97, 1.0), clamp(rim * (0.24 + 0.38 * facing) * specular, 0.0, 1.0));
    float split = dispersion * 2.4 * smoothstep(0.0, 0.4, refraction);
    vec3 rollCoord = (vec3(depth) - vec3(4.4) - vec3(split, 0.0, -split)) / 2.0;
    vec3 roll = exp(-rollCoord * rollCoord);
    color = mix(color, vec3(0.88, 0.97, 1.0), clamp(roll * 0.22 * (0.35 + 0.65 * facing) * specular, 0.0, 1.0));
    color *= 1.0 - exp(-pow((depth - 2.5) / 1.8, 2.0)) * (1.0 - facing) * 0.12 * specular;
    float halo = exp(-length(p - pointer) / 80.0);
    color += mix(vec3(1.0), accent, 0.5) * halo * (rim * 0.35 + shoulder * 0.12) * hoverGlow;
    float alpha = coverage * qt_Opacity;
    fragColor = vec4(clamp(color, 0.0, 1.0) * alpha, alpha);
}
