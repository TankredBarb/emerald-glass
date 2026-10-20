#version 330 core
// SPDX-License-Identifier: LGPL-3.0-or-later
in vec2 uv;
out vec4 fragColor;
uniform sampler2D scene;
uniform vec2 resolution;
uniform vec4 frameRect;
uniform vec4 contentRect;
uniform int buttonCount;
uniform vec4 buttonRect[12];
uniform int buttonCloseIndex;
uniform vec3 accent;
uniform vec2 pointer;
uniform float refraction;
uniform float tint;
uniform float frameOpacity;
uniform float backgroundBlurOffset;
uniform float dispersion;
uniform float specularStrength;
uniform float hoverGlowStrength;
uniform float adaptiveContrast;
uniform float frameRadius;
uniform float titleHeight;
uniform float scale;
uniform int hoverButton;
uniform int pressedButton;
uniform int kwinMode;
uniform int opticsDisabled;
// Standard Blur saturation minus one; zero preserves the sampled colors.
uniform float backdropSaturationOffset;

float boxDistance(vec2 p, vec4 box, float radius)
{
    vec2 q = abs(p - box.xy - box.zw * 0.5) - box.zw * 0.5 + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float frameDistance(vec2 p)
{
    float r = frameRadius >= 0.0 ? frameRadius : 5.0;
    float dOuter = boxDistance(p, frameRect, r);
    float dInner = (contentRect.z > 0.0 && contentRect.w > 0.0)
        ? boxDistance(p, contentRect, 1.4)
        : 10000.0;
    return max(dOuter, -dInner);
}

float materialDistance(vec2 p, int button)
{
    return button < 0 ? frameDistance(p) : boxDistance(p, buttonRect[button], 3.0);
}

vec4 sampleSceneRGBA(vec2 p)
{
    vec2 coord = clamp(vec2(p.x / resolution.x, 1.0 - p.y / resolution.y), vec2(0.001), vec2(0.999));
    if (kwinMode == 1 && backgroundBlurOffset > 0.0) {
        // Final Kawase reconstruction, matching the standard Blur sampling pattern.
        vec2 h = 0.5 * backgroundBlurOffset / vec2(textureSize(scene, 0));
        vec4 sum = texture(scene, coord + vec2(-2.0 * h.x, 0.0));
        sum += texture(scene, coord + vec2(-h.x, h.y)) * 2.0;
        sum += texture(scene, coord + vec2(0.0, 2.0 * h.y));
        sum += texture(scene, coord + vec2(h.x, h.y)) * 2.0;
        sum += texture(scene, coord + vec2(2.0 * h.x, 0.0));
        sum += texture(scene, coord + vec2(h.x, -h.y)) * 2.0;
        sum += texture(scene, coord + vec2(0.0, -2.0 * h.y));
        sum += texture(scene, coord + vec2(-h.x, -h.y)) * 2.0;
        return sum / 12.0;
    }
    return texture(scene, coord);
}

vec3 sampleScene(vec2 p)
{
    return sampleSceneRGBA(p).rgb;
}

vec3 softBackground(vec2 p)
{
    // KWin supplies the selected Kawase blur level, or the untouched scene at zero.
    if (kwinMode == 1) return sampleScene(p);
    // Small scattering radius keeps fine detail visible through the glass.
    vec3 c = sampleScene(p) * 0.28;
    c += sampleScene(p + vec2(1.4, 0.0)) * 0.18;
    c += sampleScene(p - vec2(1.4, 0.0)) * 0.18;
    c += sampleScene(p + vec2(0.0, 1.4)) * 0.18;
    c += sampleScene(p - vec2(0.0, 1.4)) * 0.18;
    return c;
}

vec3 closeButtonAccent(vec3 base)
{
    float peak = max(max(base.r, base.g), base.b);
    float chroma = peak - min(min(base.r, base.g), base.b);
    bool redFamily = base.r == peak && chroma > peak * 0.1
        && abs(base.g - base.b) <= chroma * 0.5;
    return redFamily ? vec3(1.0, 0.70, 0.12) : vec3(0.92, 0.08, 0.04);
}

vec3 material(vec2 p, float distance, int button)
{
    bool closeFeedback = button >= 0 && button == buttonCloseIndex
        && (button == hoverButton || button == pressedButton);
    vec3 materialAccent = closeFeedback ? closeButtonAccent(accent) : accent;
    float epsilon = 0.45;
    vec2 gradient = vec2(
        materialDistance(p + vec2(epsilon, 0.0), button) - materialDistance(p - vec2(epsilon, 0.0), button),
        materialDistance(p + vec2(0.0, epsilon), button) - materialDistance(p - vec2(0.0, epsilon), button));
    vec2 direction = gradient / max(length(gradient), 0.0001);
    float bevelWidth = button < 0 ? 12.0 : min(buttonRect[button].z, buttonRect[button].w) * 0.5;
    float lensWidth = bevelWidth;
    if (button < 0 && contentRect.z > 0.0 && contentRect.w > 0.0) {
        vec2 frameEnd = frameRect.xy + frameRect.zw;
        vec2 contentEnd = contentRect.xy + contentRect.zw;
        float horizontalBorder = p.x < frameRect.x + frameRect.z * 0.5
            ? contentRect.x - frameRect.x : frameEnd.x - contentEnd.x;
        float verticalBorder = p.y < contentRect.y
            ? contentRect.y - frameRect.y : frameEnd.y - contentEnd.y;
        vec2 weight = abs(direction);
        float thickness = dot(weight, vec2(horizontalBorder, verticalBorder)) / max(weight.x + weight.y, 0.0001);
        lensWidth = max(0.75, thickness * 0.5);
        bevelWidth = min(bevelWidth, lensWidth);
    }
    // A rounded lens shoulder reaches a flat tangent before opposing edges meet.
    // This avoids an optical seam through narrow side/bottom borders.
    float bevel = 1.0 - smoothstep(0.0, bevelWidth, max(-distance, 0.0));
    bool pressed = button >= 0 && button == pressedButton;
    vec3 normal = normalize(vec3(direction * bevel * (pressed ? -0.8 : 1.6), 1.0));
    // The lens spans the whole border thickness; the polished bevel stays narrow.
    float lensSlope = 1.0 - smoothstep(0.0, lensWidth, max(-distance, 0.0));
    vec3 opticalNormal = normalize(vec3(direction * lensSlope * (pressed ? -0.8 : 1.6), 1.0));
    vec3 ray = refract(vec3(0.0, 0.0, -1.0), opticalNormal, 1.0 / 1.48);
    float opticalDepth = refraction * (button < 0 ? 28.0 : 18.0);
    vec2 displacement = ray.xy / max(-ray.z, 0.1) * opticalDepth;
    // Separate polished rim highlights from the caption band.
    float captionY = (p.y - frameRect.y) / max(titleHeight, 1.0);
    float captionBody = button < 0 ? smoothstep(0.12, 0.30, captionY)
        * (1.0 - smoothstep(0.78, 0.94, captionY)) : 0.0;

    vec3 background;
    if (dispersion > 0.001) {
        // Artistic chromatic separation in logical pixels, independent of blur radius.
        // Index-only variation was lost in the scattered backdrop even at full strength.
        vec2 spectralShift = direction * lensSlope * dispersion * 12.0
            * smoothstep(0.0, 0.4, refraction);
        background.r = softBackground(p + displacement + spectralShift).r;
        background.g = softBackground(p + displacement).g;
        background.b = softBackground(p + displacement - spectralShift).b;
    } else {
        background = softBackground(p + displacement);
    }

    // Opacity controls transmission, independently of tint and lens strength.
    // At 100%, only the material and its surface lighting remain visible.
    background = mix(background, vec3(0.22, 0.2288, 0.2332), clamp(frameOpacity, 0.0, 1.0));

    float effectiveTint = closeFeedback ? 1.0 : clamp(tint, 0.0, 1.0) * (button < 0 ? frameOpacity : 1.0);
    vec3 color = background * mix(vec3(1.0), materialAccent * 0.8 + 0.2, effectiveTint * 0.65);
    color = mix(color, materialAccent * 0.45, effectiveTint * 0.16);

    // Extra tint strengthens the preset hue without lowering luminance or alpha.
    float tintBoost = clamp(tint - 1.0, 0.0, 1.0);
    float tintLuma = dot(color, vec3(0.299, 0.587, 0.114));
    float accentLuma = dot(materialAccent, vec3(0.299, 0.587, 0.114));
    vec3 tintTarget = accentLuma <= 0.0001 ? vec3(tintLuma) : materialAccent * tintLuma / accentLuma;
    float tintPeak = max(max(tintTarget.r, tintTarget.g), tintTarget.b);
    float tintGamut = clamp((1.0 - tintLuma) / max(tintPeak - tintLuma, 0.0001), 0.0, 1.0);
    tintTarget = mix(vec3(tintLuma), tintTarget, tintGamut);
    color = mix(color, tintTarget, tintBoost);

    if (closeFeedback) color = mix(color, materialAccent * 0.65, 0.75);

    // Polished highlights stay at the rim instead of washing out caption glyphs.
    float specScale = specularStrength * (1.0 - 0.8 * captionBody);
    float glowScale = hoverGlowStrength;

    vec3 light = normalize(vec3(-0.5, -0.85, 1.2));
    float specular = pow(max(dot(normal, normalize(light + vec3(0.0, 0.0, 1.0))), 0.0), 40.0) * specScale;
    float fresnel = pow(1.0 - normal.z, 3.0) * specScale;
    float directionalEdge = dot(direction, normalize(vec2(-0.55, -1.0)));
    color *= 1.0 + directionalEdge * bevel * 0.28 * specScale;
    color += mix(vec3(1.0), materialAccent, 0.18) * (specular * 0.65 + fresnel * 0.25);

    float contour = exp(-pow((distance + 0.65 / scale) * scale / 0.48, 2.0));
    color = mix(color, mix(materialAccent, vec3(1.0), max(directionalEdge, 0.0) * 0.7), contour * 0.82);
    float darkLip = exp(-pow((distance + 1.65 / scale) * scale / 0.65, 2.0));
    color *= 1.0 - darkLip * 0.24;

    // A narrow polished reflection along the titlebar, with transparent glass below.
    float tHeight = titleHeight > 0.0 ? titleHeight : 39.0;
    float titleY = p.y - frameRect.y;
    if (button < 0 && titleY < tHeight) {
        color += vec3(0.18 * specScale) * exp(-pow((titleY - 9.0) / 5.0, 2.0));
        color *= 1.0 - 0.16 * smoothstep(16.0, 21.0, titleY);
    }
    // Limit bright caption backdrops independently of tint/opacity. Dark scenes
    // stay dark. A soft shoulder preserves backdrop variation instead of clipping it.
    float bodyLuma = dot(color, vec3(0.299, 0.587, 0.114));
    float excess = max(bodyLuma - 0.32, 0.0);
    float readableLuma = 0.32 + 0.14 * (1.0 - exp(-excess / 0.14));
    float readableScale = min(1.0, readableLuma / max(bodyLuma, 0.0001));
    color *= mix(1.0, readableScale, clamp(adaptiveContrast, 0.0, 1.0) * captionBody);

    // The live decoration draws its cursor glow above the material in both modes.
    if (kwinMode != 1 && button < 0 && pointer.x >= 0.0) {
        float rimGlow = exp(-length(p - pointer) / 58.0) * bevel;
        color += mix(materialAccent, vec3(1.0), 0.45) * rimGlow * 0.16 * glowScale;
    }
    if (button >= 0) {
        float y = (p.y - buttonRect[button].y) / max(buttonRect[button].w, 1.0);
        color *= pressed ? 0.55 : 0.68;
        color += mix(materialAccent, vec3(1.0), 0.75) * (1.0 - smoothstep(0.05, 0.5, y)) * (pressed ? 0.13 : 0.34);
        color += materialAccent * 0.11;
        if (button == hoverButton) {
            float glow = exp(-length(p - pointer) / 24.0);
            color += materialAccent * glow * 0.55 * glowScale;
        }
    }
    return color;
}

vec4 emeraldMaterialMain()
{
    vec2 p = vec2(uv.x, 1.0 - uv.y) * resolution;
    float r = frameRadius >= 0.0 ? frameRadius : 5.0;
    int count = clamp(buttonCount, 0, 12);

    float distance = frameDistance(p);
    float coverage = 1.0 - smoothstep(-0.65 / scale, 0.65 / scale, distance);

    if (kwinMode == 1) {
        // Cached blur can contain transparent samples at occlusion boundaries.
        // Keep their premultiplied coverage, as standard Blur does; forcing alpha
        // to one would turn the missing samples black once per stacked window.
        if (opticsDisabled == 1) {
            vec4 background = sampleSceneRGBA(p);
            float luma = dot(background.rgb, vec3(0.2126, 0.7152, 0.0722));
            background.rgb += (background.rgb - vec3(luma)) * backdropSaturationOffset;
            return background * coverage;
        }
        vec4 result = vec4(0.0);
        if (coverage > 0.0) {
            // Replace the backdrop with its blurred version even for a clear
            // material; alpha here would leak sharp text from the original scene.
            vec3 surface = material(p, distance, -1);
            result = vec4(surface * coverage, coverage);
        }
        for (int button = 0; button < 12; ++button) {
            if (button >= count) break;
            float d = boxDistance(p, buttonRect[button], 3.0);
            float a = 1.0 - smoothstep(-0.65 / scale, 0.65 / scale, d);
            if (a > 0.0) {
                result = vec4(material(p, d, button) * a, a) + result * (1.0 - a);
            }
        }
        return result;
    }

    vec3 color = texture(scene, uv).rgb;
    float outside = boxDistance(p, frameRect, r);
    float shadow = exp(-max(outside, 0.0) / 12.0) * 0.36;
    if (outside > 0.0) color *= 1.0 - shadow;

    if (coverage > 0.0) color = mix(color, material(p, distance, -1), coverage);
    for (int button = 0; button < 12; ++button) {
        if (button >= count) break;
        float d = boxDistance(p, buttonRect[button], 3.0);
        float a = 1.0 - smoothstep(-0.65 / scale, 0.65 / scale, d);
        if (a > 0.0) color = mix(color, material(p, d, button), a);
    }
    return vec4(color, 1.0);
}

void main()
{
    fragColor = emeraldMaterialMain();
}
