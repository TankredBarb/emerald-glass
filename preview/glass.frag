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
uniform vec4 buttonRadii[12];
uniform vec4 buttonColor[12];
uniform vec4 buttonColorMeta[12];
uniform int buttonShape;
uniform int buttonCloseIndex;
uniform vec3 accent;
uniform vec3 accentSecondary;
uniform int gradientMode;
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

// The standalone comparison can override the selected production style.
#ifndef CLOSE_PREVIEW_STYLE
#define CLOSE_PREVIEW_STYLE 3
#endif

float boxDistance(vec2 p, vec4 box, float radius)
{
    vec2 q = abs(p - box.xy - box.zw * 0.5) - box.zw * 0.5 + radius;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - radius;
}

float roundedBoxDistance(vec2 p, vec4 box, vec4 radii)
{
    vec2 halfSize = box.zw * 0.5;
    vec2 rel = p - box.xy - halfSize;
    float r = rel.y > 0.0
        ? (rel.x > 0.0 ? radii.z : radii.w)
        : (rel.x > 0.0 ? radii.y : radii.x);
    r = min(r, min(halfSize.x, halfSize.y));
    vec2 q = abs(rel) - halfSize + r;
    return min(max(q.x, q.y), 0.0) + length(max(q, 0.0)) - r;
}

vec4 effectiveButtonRadii(int button)
{
    vec4 r = buttonRadii[button];
    if (max(max(r.x, r.y), max(r.z, r.w)) > 0.001) {
        return r;
    }
    if (buttonShape == 3) return vec4(11.0);
    if (buttonShape == 4) return vec4(5.0);
    return vec4(3.0);
}

vec4 buttonEvalRect(int button, vec4 radii)
{
    vec4 rect = buttonRect[button];
    if (buttonShape == 1 || buttonShape == 2) {
        if (radii.w < 1.0) {
            rect.x -= 4.0;
            rect.z += 4.0;
        }
        if (radii.z < 1.0) {
            rect.z += 4.0;
        }
    }
    return rect;
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
    if (button < 0) return frameDistance(p);
    vec4 radii = effectiveButtonRadii(button);
    return roundedBoxDistance(p, buttonEvalRect(button, radii), radii);
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

vec4 sampleScene(vec2 p)
{
    vec4 sceneSample = sampleSceneRGBA(p);
    // Cached texels are premultiplied. Missing backdrop coverage is not black.
    return vec4(sceneSample.a > 0.0001 ? sceneSample.rgb / sceneSample.a : vec3(0.0), sceneSample.a);
}

vec4 softBackground(vec2 p)
{
    // KWin supplies the selected Kawase blur level, or the untouched scene at zero.
    if (kwinMode == 1) return sampleScene(p);
    // Small scattering radius keeps fine detail visible through the glass.
    vec4 c = sampleScene(p) * 0.28;
    c += sampleScene(p + vec2(1.4, 0.0)) * 0.18;
    c += sampleScene(p - vec2(1.4, 0.0)) * 0.18;
    c += sampleScene(p + vec2(0.0, 1.4)) * 0.18;
    c += sampleScene(p - vec2(0.0, 1.4)) * 0.18;
    return c;
}

vec3 rgbToHsv(vec3 c)
{
    float maxC = max(max(c.r, c.g), c.b);
    float minC = min(min(c.r, c.g), c.b);
    float d = maxC - minC;
    float h = 0.0;
    if (d > 0.0001) {
        if (maxC == c.r) {
            h = mod((c.g - c.b) / d, 6.0);
        } else if (maxC == c.g) {
            h = (c.b - c.r) / d + 2.0;
        } else {
            h = (c.r - c.g) / d + 4.0;
        }
        h *= 60.0;
    }
    float s = maxC > 0.0001 ? d / maxC : 0.0;
    return vec3(h, s, maxC);
}

vec3 closeButtonAccent(vec3 base)
{
    vec3 hsv = rgbToHsv(base);
    if (hsv.y <= 0.15) return vec3(0.92, 0.08, 0.06);
    if (hsv.x >= 305.0 || hsv.x < 18.0) return vec3(1.0, 0.70, 0.12);
    if (hsv.x >= 18.0 && hsv.x < 52.0) return vec3(0.08, 0.40, 1.0);
    return vec3(0.92, 0.08, 0.06);
}

void evalFrameGradient(vec2 p, float edgeFactor, out vec3 bodyAccent, out vec3 rimAccent)
{
    if (gradientMode <= 0) {
        bodyAccent = accent;
        rimAccent = accent;
        return;
    }
    vec2 uvFrame = clamp((p - frameRect.xy) / max(frameRect.zw, vec2(1.0)), 0.0, 1.0);
    if (gradientMode == 1) {
        float t = smoothstep(0.08, 0.92, uvFrame.x);
        bodyAccent = mix(accent, accentSecondary, t);
        rimAccent = bodyAccent;
        return;
    }
    if (gradientMode == 2) {
        float t = smoothstep(0.12, 0.96, uvFrame.x * 0.72 + uvFrame.y * 0.38);
        bodyAccent = mix(accent, accentSecondary, t);
        rimAccent = bodyAccent;
        return;
    }
    if (gradientMode == 3) {
        float dist = length(vec2((uvFrame.x - 0.48) * 1.18, (uvFrame.y - 0.22) * 2.0));
        float t = 1.0 - smoothstep(0.06, 0.56, dist);
        bodyAccent = mix(accent, accentSecondary, t);
        rimAccent = bodyAccent;
        return;
    }
    if (gradientMode == 4) {
        float bodyT = clamp(smoothstep(0.15, 0.95, uvFrame.y) * 0.24 + edgeFactor * 0.50, 0.0, 1.0);
        float rimT = clamp(0.92 * edgeFactor + 0.45 * smoothstep(0.15, 0.90, uvFrame.y + uvFrame.x * 0.25), 0.0, 1.0);
        bodyAccent = mix(accent, accentSecondary, bodyT);
        rimAccent = mix(accent, accentSecondary, rimT);
        return;
    }
    bodyAccent = accent;
    rimAccent = accent;
}

vec4 buttonMaterial(vec2 p, float distance, int button)
{
    vec3 buttonBaseAccent = accent;
    if (button >= 0 && gradientMode > 0 && gradientMode != 4) {
        vec3 localBody;
        vec3 localRim;
        vec2 btnCenter = buttonRect[button].xy + buttonRect[button].zw * 0.5;
        evalFrameGradient(btnCenter, 0.0, localBody, localRim);
        buttonBaseAccent = localBody;
    }
    bool hasButtonColor = button >= 0 && buttonColor[button].a > 0.5;
    bool closeFeedback = !hasButtonColor && button >= 0 && button == buttonCloseIndex
        && (button == hoverButton || button == pressedButton);
    vec3 materialAccent = hasButtonColor
        ? buttonColor[button].rgb
        : (closeFeedback ? closeButtonAccent(buttonBaseAccent) : buttonBaseAccent);
    // Pale glass and semantic Close feedback need headroom for the lens body.
    // Stacking white rim, top reflection and hover light used to clip them flat.
    float restrainedLight = closeFeedback ? 1.0 : smoothstep(0.40, 0.70, min(min(buttonBaseAccent.r, buttonBaseAccent.g), buttonBaseAccent.b));
#if CLOSE_PREVIEW_STYLE > 0
    if (closeFeedback) restrainedLight = 0.35;
#endif
    vec3 lightingAccent = materialAccent;
#if CLOSE_PREVIEW_STYLE == 3
    if (closeFeedback) lightingAccent = mix(buttonBaseAccent, materialAccent, 0.20);
#endif
    float customFill = 0.0;
    float customTint = tint;
    if (hasButtonColor) {
        vec4 meta = buttonColorMeta[button];
        restrainedLight = clamp(meta.z, 0.0, 1.0);
        lightingAccent = mix(buttonBaseAccent, materialAccent, clamp(meta.y, 0.0, 1.0));
        customFill = max(buttonColor[button].a - 1.0, 0.0);
        customTint = meta.x;
    }
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

    vec4 background;
    if (dispersion > 0.001) {
        // Artistic chromatic separation in logical pixels, independent of blur radius.
        // Index-only variation was lost in the scattered backdrop even at full strength.
        vec2 spectralShift = direction * lensSlope * dispersion * 12.0
            * smoothstep(0.0, 0.4, refraction);
        vec4 red = softBackground(p + displacement + spectralShift);
        vec4 green = softBackground(p + displacement);
        vec4 blue = softBackground(p + displacement - spectralShift);
        background = vec4(red.r, green.g, blue.b, min(red.a, min(green.a, blue.a)));
    } else {
        background = softBackground(p + displacement);
    }

    // Opacity controls transmission, independently of tint and lens strength.
    // At 100%, only the material and its surface lighting remain visible.
    float alpha = mix(clamp(frameOpacity, 0.0, 1.0), 1.0, background.a);
    background.rgb = mix(background.rgb, vec3(0.22, 0.2288, 0.2332), clamp(frameOpacity, 0.0, 1.0) / max(alpha, 0.0001));

    float effectiveTint = hasButtonColor
        ? clamp(customTint, 0.0, 1.0)
        : (closeFeedback ? 1.0 : clamp(tint, 0.0, 1.0) * (button < 0 ? frameOpacity : 1.0));
    vec3 color = background.rgb * mix(vec3(1.0), materialAccent * 0.8 + 0.2, effectiveTint * 0.65);
    color = mix(color, materialAccent * 0.45, effectiveTint * 0.16);

    // Extra tint strengthens the preset hue without lowering luminance or alpha.
    float tintBoost = clamp((hasButtonColor ? customTint : tint) - 1.0, 0.0, 1.0);
    float tintLuma = dot(color, vec3(0.299, 0.587, 0.114));
    float accentLuma = dot(materialAccent, vec3(0.299, 0.587, 0.114));
    vec3 tintTarget = accentLuma <= 0.0001 ? vec3(tintLuma) : materialAccent * tintLuma / accentLuma;
    float tintPeak = max(max(tintTarget.r, tintTarget.g), tintTarget.b);
    float tintGamut = clamp((1.0 - tintLuma) / max(tintPeak - tintLuma, 0.0001), 0.0, 1.0);
    tintTarget = mix(vec3(tintLuma), tintTarget, tintGamut);
    color = mix(color, tintTarget, tintBoost);

    if (hasButtonColor) {
        if (customFill > 0.001) {
            float depth = smoothstep(0.0, 4.0, max(-distance, 0.0));
            color = mix(color, materialAccent * 0.65, customFill * depth);
        }
    } else if (closeFeedback) {
#if CLOSE_PREVIEW_STYLE == 1
        color = mix(color, materialAccent * 0.65, 0.25);
#elif CLOSE_PREVIEW_STYLE == 2
        color = mix(color, materialAccent * 0.65, 0.45);
#elif CLOSE_PREVIEW_STYLE == 3
        float depth = smoothstep(0.0, 4.0, max(-distance, 0.0));
        color = mix(color, materialAccent * 0.65, 0.35 * depth);
#else
        color = mix(color, materialAccent * 0.65, 0.75);
#endif
    }

    // Polished highlights stay at the rim instead of washing out caption glyphs.
    float specScale = specularStrength * (1.0 - 0.8 * captionBody);
    float glowScale = hoverGlowStrength;

    vec3 light = normalize(vec3(-0.5, -0.85, 1.2));
    float specular = pow(max(dot(normal, normalize(light + vec3(0.0, 0.0, 1.0))), 0.0), 40.0) * specScale;
    float fresnel = pow(1.0 - normal.z, 3.0) * specScale;
    float directionalEdge = dot(direction, normalize(vec2(-0.55, -1.0)));
    color *= 1.0 + directionalEdge * bevel * 0.28 * specScale;
    color += mix(vec3(1.0), lightingAccent, mix(0.18, 0.65, restrainedLight))
        * (specular * mix(0.65, 0.30, restrainedLight) + fresnel * mix(0.25, 0.12, restrainedLight));

    vec4 radii = button >= 0 ? effectiveButtonRadii(button) : vec4(3.0);
    bool joined = button >= 0 && (buttonShape == 1 || buttonShape == 2);
    bool seamLeft = joined && radii.w < 1.0;
    bool seamRight = joined && radii.z < 1.0;
    bool topDocked = button >= 0 && buttonShape == 2;

    float contour = exp(-pow((distance + 0.65 / scale) * scale / 0.48, 2.0));
    if (button >= 0 && buttonShape == 4 && button != hoverButton && button != pressedButton) {
        float stealthTint = clamp(effectiveTint, 0.0, 1.0);
        vec3 stealthColor = mix(background.rgb * 0.82, materialAccent * 0.35, stealthTint * 0.28 + customFill * 0.45);
        vec3 stealthRim = mix(lightingAccent, vec3(1.0), 0.40);
        stealthColor = mix(stealthColor, stealthRim, contour * 0.32);
        float stealthAlpha = alpha * clamp(0.42 + customFill * 0.35, 0.0, 0.72);
        return vec4(clamp(stealthColor, 0.0, 1.0), stealthAlpha);
    }

    color = mix(color, mix(lightingAccent, vec3(1.0), max(directionalEdge, 0.0) * mix(0.7, 0.25, restrainedLight)),
                contour * mix(0.82, 0.60, restrainedLight));
    float darkLip = exp(-pow((distance + 1.65 / scale) * scale / 0.65, 2.0));
    if (button >= 0) {
        vec4 rect = buttonRect[button];
        if (seamLeft) {
            darkLip *= smoothstep(0.0, 3.0, p.x - rect.x);
        }
        if (seamRight) {
            darkLip *= smoothstep(0.0, 3.0, (rect.x + rect.z) - p.x);
        }
        if (topDocked) {
            darkLip *= smoothstep(0.0, 3.0, p.y - rect.y);
        }
    }
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
        vec4 rect = buttonRect[button];
        float y = (p.y - rect.y) / max(rect.w, 1.0);
        color *= pressed ? 0.55 : 0.68;
        color += mix(lightingAccent, vec3(1.0), mix(0.75, 0.25, restrainedLight))
            * (1.0 - smoothstep(0.05, 0.5, y)) * (pressed ? 0.13 : mix(0.34, 0.20, restrainedLight));
        color += materialAccent * 0.11;
        if (seamLeft) {
            float seamDist = (p.x - rect.x) * scale;
            float divider = exp(-pow(seamDist / 0.9, 2.0));
            color = mix(color, mix(lightingAccent, vec3(1.0), 0.55), divider * 0.35);
        }
        if (seamRight) {
            float seamDist = ((rect.x + rect.z) - p.x) * scale;
            float divider = exp(-pow(seamDist / 0.9, 2.0));
            color *= 1.0 - divider * 0.38;
        }
        if (button == hoverButton) {
            float glow = exp(-length(p - pointer) / 24.0);
            color += materialAccent * glow * mix(0.55, 0.22, restrainedLight) * glowScale;
        }
    }
    // Clamp straight color before coverage premultiplication so bright feedback
    // cannot overwhelm the antialiased rounded edge.
    return vec4(clamp(color, 0.0, 1.0), alpha);
}

// The window is one glass sheet. The client hole only clips output;
// it does not create a second lens running along the inside of the titlebar.
vec4 material(vec2 p, float distance, int button)
{
    if (button >= 0) return buttonMaterial(p, distance, button);
    float radius = max(frameRadius, 0.0);
    float outer = boxDistance(p, frameRect, radius);
    float epsilon = 0.45;
    vec2 gradient = vec2(
        boxDistance(p + vec2(epsilon, 0.0), frameRect, radius) - boxDistance(p - vec2(epsilon, 0.0), frameRect, radius),
        boxDistance(p + vec2(0.0, epsilon), frameRect, radius) - boxDistance(p - vec2(0.0, epsilon), frameRect, radius));
    vec2 direction = gradient / max(length(gradient), 0.0001);
    float depth = max(-outer, 0.0);
    float edgeFactor = exp(-depth / 3.4);
    vec3 frameBodyAccent;
    vec3 frameRimAccent;
    evalFrameGradient(p, edgeFactor, frameBodyAccent, frameRimAccent);
    float opticalThickness = clamp(refraction / 1.8, 0.0, 1.0);
    // Keep the accepted shoulder near the usual 75% setting, with a useful
    // thickness range toward either end of the control.
    float shoulderWidth = min(9.0 + refraction * 4.0, min(frameRect.z, frameRect.w) * 0.25);
    float shoulder = 1.0 - smoothstep(0.0, shoulderWidth, depth);
    // Pixel displacement gives the control a predictable, visible range.
    vec2 bend = -direction * shoulder * refraction * 18.0;
    vec2 spectrum = direction * shoulder * dispersion * 32.0 * smoothstep(0.0, 0.4, refraction);
    vec4 background;
    if (dispersion > 0.001) {
        vec4 red = sampleScene(p + bend + spectrum);
        vec4 green = sampleScene(p + bend);
        vec4 blue = sampleScene(p + bend - spectrum);
        background = vec4(red.r, green.g, blue.b, min(red.a, min(green.a, blue.a)));
    } else {
        background = sampleScene(p + bend);
    }
    float alpha = mix(clamp(frameOpacity, 0.0, 1.0), 1.0, background.a);
    background.rgb = mix(background.rgb, vec3(0.22, 0.2288, 0.2332), clamp(frameOpacity, 0.0, 1.0) / max(alpha, 0.0001));
    float baseTint = clamp(tint, 0.0, 1.0) * frameOpacity;
    vec3 color = background.rgb * mix(vec3(1.0), frameBodyAccent * 0.8 + 0.2, baseTint * 0.65);
    color = mix(color, frameBodyAccent * 0.45, baseTint * 0.16);
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    float accentLuma = dot(frameBodyAccent, vec3(0.299, 0.587, 0.114));
    vec3 target = accentLuma <= 0.0001 ? vec3(luma) : frameBodyAccent * luma / accentLuma;
    float peak = max(max(target.r, target.g), target.b);
    float gamut = clamp((1.0 - luma) / max(peak - luma, 0.0001), 0.0, 1.0);
    color = mix(color, mix(vec3(luma), target, gamut), clamp(tint - 1.0, 0.0, 1.0));

    // Reflection is confined to the silhouette, not a bright tube across the body.
    float lightFacing = dot(direction, normalize(vec2(-0.55, -1.0)));
    float rim = exp(-pow((outer + 0.65 / scale) * scale / 0.7, 2.0));
    float reflection = rim * (0.24 + 0.52 * abs(lightFacing)) * specularStrength;
    float rimTintMix = gradientMode == 4 ? 0.42 : 0.12;
    color = mix(color, mix(vec3(1.0), frameRimAccent, rimTintMix), clamp(reflection, 0.0, 1.0));
    // The rolled edge reflects light as well as transmitting the scene. This
    // keeps thickness and color separation visible over a smooth/blurred wall.
    // Disperse the reflected light spatially, without recoloring the flat body.
    float rollCenter = 1.2 + opticalThickness * 4.5;
    float rollWidth = 0.65 + opticalThickness * 2.0;
    float spectralSplit = dispersion * 3.2 * smoothstep(0.0, 0.4, refraction);
    vec3 rollPosition = vec3(rollCenter) + vec3(spectralSplit, 0.0, -spectralSplit);
    vec3 rollCoord = (vec3(depth) - rollPosition) / rollWidth;
    vec3 roll = exp(-rollCoord * rollCoord);
    // Give dispersion its own reflection range: a moderate thickness must not
    // reduce the entire color control to a barely visible single-pixel fringe.
    float chromaticReflection = dispersion * 0.8 * smoothstep(0.0, 0.4, refraction);
    vec3 edgeReflection = roll * (opticalThickness * 0.42 + chromaticReflection) * specularStrength
        * (0.35 + 0.65 * abs(lightFacing));
    color = mix(color, mix(vec3(1.0), frameRimAccent, rimTintMix), clamp(edgeReflection, vec3(0.0), vec3(1.0)));

    float innerShade = exp(-pow((depth - 2.5) / 1.8, 2.0));
    color *= 1.0 - innerShade * (1.0 - abs(lightFacing)) * 0.12 * specularStrength;

    float captionY = (p.y - frameRect.y) / max(titleHeight, 1.0);
    float captionBody = smoothstep(0.12, 0.30, captionY) * (1.0 - smoothstep(0.78, 0.94, captionY));
    float bodyLuma = dot(color, vec3(0.299, 0.587, 0.114));
    float excess = max(bodyLuma - 0.32, 0.0);
    float readableLuma = 0.32 + 0.14 * (1.0 - exp(-excess / 0.14));
    color *= mix(1.0, min(1.0, readableLuma / max(bodyLuma, 0.0001)), clamp(adaptiveContrast, 0.0, 1.0) * captionBody);
    return vec4(color, alpha);
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
            vec4 surface = material(p, distance, -1);
            float a = coverage * surface.a;
            result = vec4(surface.rgb * a, a);
        }
        for (int button = 0; button < 12; ++button) {
            if (button >= count) break;
            vec4 rect = buttonRect[button];
            vec4 radii = effectiveButtonRadii(button);
            float d = roundedBoxDistance(p, buttonEvalRect(button, radii), radii);
            float a = 1.0 - smoothstep(-0.65 / scale, 0.65 / scale, d);
            bool joined = buttonShape == 1 || buttonShape == 2;
            if (joined && radii.w < 1.0 && p.x < rect.x) a = 0.0;
            if (joined && radii.z < 1.0 && p.x >= rect.x + rect.z) a = 0.0;
            if (a > 0.0) {
                vec4 surface = material(p, d, button);
                a *= surface.a;
                result = vec4(surface.rgb * a, a) + result * (1.0 - a);
            }
        }
        return result;
    }

    vec3 color = texture(scene, uv).rgb;
    float outside = boxDistance(p, frameRect, r);
    float shadow = exp(-max(outside, 0.0) / 12.0) * 0.36;
    if (outside > 0.0) color *= 1.0 - shadow;

    if (coverage > 0.0) color = mix(color, material(p, distance, -1).rgb, coverage);
    for (int button = 0; button < 12; ++button) {
        if (button >= count) break;
        vec4 rect = buttonRect[button];
        vec4 radii = effectiveButtonRadii(button);
        float d = roundedBoxDistance(p, buttonEvalRect(button, radii), radii);
        float a = 1.0 - smoothstep(-0.65 / scale, 0.65 / scale, d);
        bool joined = buttonShape == 1 || buttonShape == 2;
        if (joined && radii.w < 1.0 && p.x < rect.x) a = 0.0;
        if (joined && radii.z < 1.0 && p.x >= rect.x + rect.z) a = 0.0;
        if (a > 0.0) {
            vec4 surface = material(p, d, button);
            float blendA = (buttonShape == 4 && button != hoverButton && button != pressedButton) ? a * surface.a : a;
            color = mix(color, surface.rgb, blendA);
        }
    }
    return vec4(color, 1.0);
}

void main()
{
    fragColor = emeraldMaterialMain();
}
