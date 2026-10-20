#version 330 core
// SPDX-License-Identifier: LGPL-3.0-or-later
in vec2 uv;
out vec4 fragColor;
uniform vec2 resolution;
uniform float time;
uniform int lightBackground;

void main()
{
    vec2 p = vec2(uv.x, 1.0 - uv.y);
    vec3 low = lightBackground == 1 ? vec3(0.76, 0.83, 0.86) : vec3(0.015, 0.035, 0.065);
    vec3 high = lightBackground == 1 ? vec3(0.98, 0.91, 0.78) : vec3(0.05, 0.21, 0.28);
    vec3 color = mix(low, high, p.x * 0.45 + (1.0 - p.y) * 0.55);
    float wave = sin(p.x * 8.0 + p.y * 4.0 + time * 0.15);
    color += vec3(0.03, 0.12, 0.09) * wave;
    vec2 pixel = p * resolution + vec2(time * 9.0, time * 3.0);
    vec2 grid = abs(fract(pixel / 56.0) - 0.5) * 56.0;
    float line = 1.0 - smoothstep(0.5, 1.3, min(grid.x, grid.y));
    color = mix(color, lightBackground == 1 ? vec3(0.27, 0.39, 0.43) : vec3(0.28, 0.55, 0.61), line * 0.17);
    float ribbon = exp(-pow((p.y - 0.47 - 0.18 * sin(p.x * 5.0 + time * 0.1)) * 38.0, 2.0));
    color += vec3(0.18, 0.34, 0.29) * ribbon;
    float disc = 1.0 - smoothstep(0.11, 0.113, length((p - vec2(0.81, 0.25)) * vec2(resolution.x / resolution.y, 1.0)));
    color = mix(color, vec3(0.85, 0.41, 0.19), disc * 0.6);
    fragColor = vec4(color, 1.0);
}
