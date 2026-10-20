// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <KConfigGroup>
#include <KSharedConfig>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <algorithm>
#include <cmath>

namespace EmeraldGlass {

struct ColorPreset {
    const char *id;
    const char *name;
    QColor color;
};

inline constexpr std::array<ColorPreset, 8> kColorPresets = {{
    {"emerald", "Emerald Green", QColor(0x17, 0xf5, 0x59)},
    {"ruby", "Ruby Red", QColor(0xf7, 0x14, 0x0e)},
    {"emerald_dim", "Dimmed Emerald", QColor(0x14, 0xa8, 0x44)},
    {"sapphire", "Sapphire Blue", QColor(0x1e, 0x8f, 0xff)},
    {"amber", "Amber Gold", QColor(0xff, 0x9e, 0x14)},
    {"amethyst", "Amethyst Purple", QColor(0xb8, 0x33, 0xff)},
    {"crystal", "Liquid Crystal", QColor(0xa8, 0xe4, 0xf5)},
    {"peach_rose", "Peach Rose", QColor(0xff, 0x8c, 0x9d)},
}};

inline QColor resolvePresetColor(const QString &preset, const QColor &customColor, const QColor &fallback)
{
    if (preset == QLatin1String("custom")) {
        return customColor.isValid() ? customColor : fallback;
    }
    if (preset == QLatin1String("green")) {
        return QColor(0x17, 0xf5, 0x59);
    }
    if (preset == QLatin1String("red")) {
        return QColor(0xf7, 0x14, 0x0e);
    }
    for (const auto &item : kColorPresets) {
        if (preset == QLatin1String(item.id)) {
            return item.color;
        }
    }
    return fallback;
}

struct MaterialConfig {
    QString activePreset = QStringLiteral("emerald");
    QString inactivePreset = QStringLiteral("ruby");
    QColor activeCustomColor = QColor(0x17, 0xf5, 0x59);
    QColor inactiveCustomColor = QColor(0xf7, 0x14, 0x0e);

    int activeTitleText = 0; // 0 = Auto, 1 = Light, 2 = Dark
    int inactiveTitleText = 0;

    float refraction = 0.85f;
    float tint = 0.70f;
    float inactiveTint = 0.58f;
    float dispersion = 0.25f;
    float specular = 1.00f;
    float hoverGlow = 1.00f;
    float adaptiveContrast = 0.35f;
    float frameOpacity = 0.84f;
    int blurStrength = 6;

    int cornerRadius = 5;
    int titleHeight = 39;
    int borderWidth = 8;
    int titleAlignment = 0; // 0 = Left, 1 = Center, 2 = CenterFullWidth
    bool useSystemButtons = false;
    bool shaderEffectEnabled = true;

    QColor colorForWindow(bool active) const
    {
        return active
            ? resolvePresetColor(activePreset, activeCustomColor, QColor(0x17, 0xf5, 0x59))
            : resolvePresetColor(inactivePreset, inactiveCustomColor, QColor(0xf7, 0x14, 0x0e));
    }

    float tintForWindow(bool active) const
    {
        return active ? tint : inactiveTint;
    }

    QColor titleColorForWindow(bool active, bool opticalMaterial) const;

    bool operator==(const MaterialConfig &other) const = default;
};

inline MaterialConfig loadMaterialConfig()
{
    MaterialConfig cfg;
    auto config = KSharedConfig::openConfig(QStringLiteral("emeraldglassrc"));
    config->reparseConfiguration();
    KConfigGroup mat(config, QStringLiteral("Material"));

    const QString legacyColor = mat.readEntry("Color", QString());
    const QString defaultActive = (legacyColor == QLatin1String("red")) ? QStringLiteral("ruby") : QStringLiteral("emerald");
    cfg.activePreset = mat.readEntry("ActivePreset", defaultActive);
    cfg.inactivePreset = mat.readEntry("InactivePreset", QStringLiteral("ruby"));
    cfg.activeCustomColor = mat.readEntry("ActiveCustomColor", QColor(0x17, 0xf5, 0x59));
    cfg.inactiveCustomColor = mat.readEntry("InactiveCustomColor", QColor(0xf7, 0x14, 0x0e));

    cfg.activeTitleText = std::clamp(mat.readEntry("ActiveTitleText", 0), 0, 2);
    cfg.inactiveTitleText = std::clamp(mat.readEntry("InactiveTitleText", 0), 0, 2);
    cfg.refraction = std::clamp(float(mat.readEntry("Refraction", 85)) / 100.0f, 0.0f, 1.8f);
    cfg.tint = std::clamp(float(mat.readEntry("Tint", 70)) / 100.0f, 0.0f, 2.0f);
    cfg.inactiveTint = std::clamp(float(mat.readEntry("InactiveTint", 58)) / 100.0f, 0.0f, 2.0f);
    cfg.dispersion = std::clamp(float(mat.readEntry("Dispersion", 25)) / 100.0f, 0.0f, 1.0f);
    cfg.specular = std::clamp(float(mat.readEntry("Specular", 100)) / 100.0f, 0.0f, 1.5f);
    cfg.hoverGlow = std::clamp(float(mat.readEntry("HoverGlow", 100)) / 100.0f, 0.0f, 1.5f);
    cfg.adaptiveContrast = std::clamp(float(mat.readEntry("AdaptiveContrast", 35)) / 100.0f, 0.0f, 1.0f);
    cfg.frameOpacity = std::clamp(float(mat.readEntry("FrameOpacity", 84)) / 100.0f, 0.20f, 1.0f);
    cfg.blurStrength = std::clamp(mat.readEntry("BlurStrength", 6), 0, 15);
    cfg.shaderEffectEnabled = mat.readEntry("ShaderEffectEnabled", true);

    KConfigGroup geom(config, QStringLiteral("Geometry"));
    cfg.cornerRadius = std::clamp(geom.readEntry("CornerRadius", 5), 0, 18);
    cfg.titleHeight = std::clamp(geom.readEntry("TitleHeight", 39), 26, 56);
    cfg.borderWidth = std::clamp(geom.readEntry("BorderWidth", 8), 0, 20);
    cfg.titleAlignment = std::clamp(geom.readEntry("TitleAlignment", 0), 0, 2);
    cfg.useSystemButtons = geom.readEntry("UseSystemButtons", false);

    return cfg;
}

inline void saveMaterialConfig(const MaterialConfig &cfg)
{
    auto config = KSharedConfig::openConfig(QStringLiteral("emeraldglassrc"));
    KConfigGroup mat(config, QStringLiteral("Material"));
    mat.writeEntry("ActivePreset", cfg.activePreset);
    mat.writeEntry("InactivePreset", cfg.inactivePreset);
    mat.writeEntry("ActiveCustomColor", cfg.activeCustomColor);
    mat.writeEntry("InactiveCustomColor", cfg.inactiveCustomColor);
    mat.writeEntry("Color", cfg.activePreset == QLatin1String("ruby") ? QStringLiteral("red") : QStringLiteral("green"));
    mat.writeEntry("ActiveTitleText", cfg.activeTitleText);
    mat.writeEntry("InactiveTitleText", cfg.inactiveTitleText);
    mat.writeEntry("Refraction", qRound(cfg.refraction * 100.0f));
    mat.writeEntry("Tint", qRound(cfg.tint * 100.0f));
    mat.writeEntry("InactiveTint", qRound(cfg.inactiveTint * 100.0f));
    mat.writeEntry("Dispersion", qRound(cfg.dispersion * 100.0f));
    mat.writeEntry("Specular", qRound(cfg.specular * 100.0f));
    mat.writeEntry("HoverGlow", qRound(cfg.hoverGlow * 100.0f));
    mat.writeEntry("AdaptiveContrast", qRound(cfg.adaptiveContrast * 100.0f));
    mat.writeEntry("FrameOpacity", qRound(cfg.frameOpacity * 100.0f));
    mat.writeEntry("BlurStrength", cfg.blurStrength);
    mat.writeEntry("ShaderEffectEnabled", cfg.shaderEffectEnabled);

    KConfigGroup geom(config, QStringLiteral("Geometry"));
    geom.writeEntry("CornerRadius", cfg.cornerRadius);
    geom.writeEntry("TitleHeight", cfg.titleHeight);
    geom.writeEntry("BorderWidth", cfg.borderWidth);
    geom.writeEntry("TitleAlignment", cfg.titleAlignment);
    geom.writeEntry("UseSystemButtons", cfg.useSystemButtons);
    config->sync();
}

namespace Math {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline float clampf(float v, float lo, float hi)
{
    return std::max(lo, std::min(hi, v));
}

inline float smoothstep(float edge0, float edge1, float x)
{
    const float t = clampf((x - edge0) / std::max(edge1 - edge0, 0.0001f), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

inline float mixf(float a, float b, float t)
{
    return a + (b - a) * t;
}

inline Vec3 mix3(Vec3 a, Vec3 b, float t)
{
    return {mixf(a.x, b.x, t), mixf(a.y, b.y, t), mixf(a.z, b.z, t)};
}

// Above 100%, move toward the preset hue at the same luminance.
inline Vec3 boostTint(Vec3 color, Vec3 accent, float strength)
{
    const float amount = clampf(strength - 1.0f, 0.0f, 1.0f);
    if (amount == 0.0f) {
        return color;
    }
    const float luma = color.x * 0.299f + color.y * 0.587f + color.z * 0.114f;
    const float accentLuma = std::max(accent.x * 0.299f + accent.y * 0.587f + accent.z * 0.114f, 0.0001f);
    Vec3 target = accentLuma <= 0.0001f ? Vec3{luma, luma, luma}
        : Vec3{accent.x * luma / accentLuma, accent.y * luma / accentLuma, accent.z * luma / accentLuma};
    const float peak = std::max({target.x, target.y, target.z});
    const float gamut = clampf((1.0f - luma) / std::max(peak - luma, 0.0001f), 0.0f, 1.0f);
    target = mix3({luma, luma, luma}, target, gamut);
    return mix3(color, target, amount);
}

// Red/pink materials need a distinct warm warning color for Close feedback.
inline Vec3 closeButtonAccent(Vec3 base)
{
    const float peak = std::max({base.x, base.y, base.z});
    const float chroma = peak - std::min({base.x, base.y, base.z});
    const bool redFamily = base.x == peak && chroma > peak * 0.1f
        && std::abs(base.y - base.z) <= chroma * 0.5f;
    return redFamily ? Vec3{1.0f, 0.70f, 0.12f} : Vec3{0.92f, 0.08f, 0.04f};
}

inline float boxDistance(float px, float py, float bx, float by, float bw, float bh, float radius)
{
    const float qx = std::abs(px - bx - bw * 0.5f) - bw * 0.5f + radius;
    const float qy = std::abs(py - by - bh * 0.5f) - bh * 0.5f + radius;
    const float mx = std::max(qx, 0.0f);
    const float my = std::max(qy, 0.0f);
    return std::min(std::max(qx, qy), 0.0f) + std::sqrt(mx * mx + my * my) - radius;
}

inline float frameDistance(float px, float py, const QRectF &frame, const QRectF &content, float outerRadius)
{
    const float dOuter = boxDistance(px, py, float(frame.x()), float(frame.y()),
                                     float(frame.width()), float(frame.height()), outerRadius);
    const float dInner = (content.width() > 0.0 && content.height() > 0.0)
        ? boxDistance(px, py, float(content.x()), float(content.y()),
                      float(content.width()), float(content.height()), 1.4f)
        : 10000.0f;
    return std::max(dOuter, -dInner);
}

inline QRgb toPremultipliedRgb(Vec3 color, float alpha)
{
    const float a = clampf(alpha, 0.0f, 1.0f);
    const int ai = int(std::lround(a * 255.0f));
    const int ri = int(std::lround(clampf(color.x, 0.0f, 1.0f) * a * 255.0f));
    const int gi = int(std::lround(clampf(color.y, 0.0f, 1.0f) * a * 255.0f));
    const int bi = int(std::lround(clampf(color.z, 0.0f, 1.0f) * a * 255.0f));
    return qRgba(ri, gi, bi, ai);
}

} // namespace Math

// Linear sRGB luminance for comparing foreground/background contrast.
inline float relativeLuminance(const QColor &color)
{
    auto linear = [](float c) { return c <= 0.04045f ? c / 12.92f : std::pow((c + 0.055f) / 1.055f, 2.4f); };
    return linear(color.redF()) * 0.2126f + linear(color.greenF()) * 0.7152f + linear(color.blueF()) * 0.0722f;
}

inline QColor MaterialConfig::titleColorForWindow(bool active, bool opticalMaterial) const
{
    const QColor light = active ? QColor("#effff6") : QColor("#d4e2db");
    const QColor dark = active ? QColor("#101714") : QColor("#253029");
    const int mode = active ? activeTitleText : inactiveTitleText;
    if (mode == 1) return light;
    if (mode == 2) return dark;

    const auto preset = colorForWindow(active);
    const Math::Vec3 accent{float(preset.redF()), float(preset.greenF()), float(preset.blueF())};
    const float strength = tintForWindow(active);
    // The preset is a stable reference backdrop, not a live wallpaper sample.
    // Model the caption body rather than choosing text from raw preset brightness.
    const Math::Vec3 neutral{0.22f, 0.2288f, 0.2332f};
    auto color = opticalMaterial ? Math::mix3(accent, neutral, frameOpacity) : neutral;
    const float baseTint = std::clamp(strength, 0.0f, 1.0f) * (opticalMaterial ? frameOpacity : 1.0f);
    color.x *= Math::mixf(1.0f, accent.x * 0.8f + 0.2f, baseTint * 0.65f);
    color.y *= Math::mixf(1.0f, accent.y * 0.8f + 0.2f, baseTint * 0.65f);
    color.z *= Math::mixf(1.0f, accent.z * 0.8f + 0.2f, baseTint * 0.65f);
    color = Math::mix3(color, {accent.x * 0.45f, accent.y * 0.45f, accent.z * 0.45f}, baseTint * (opticalMaterial ? 0.16f : 0.26f));
    color = Math::boostTint(color, accent, strength);
    if (opticalMaterial) {
        const float luma = color.x * 0.299f + color.y * 0.587f + color.z * 0.114f;
        const float excess = std::max(luma - 0.32f, 0.0f);
        const float readable = 0.32f + 0.14f * (1.0f - std::exp(-excess / 0.14f));
        const float adaptation = Math::mixf(1.0f, std::min(1.0f, readable / std::max(luma, 0.0001f)), adaptiveContrast);
        color = {color.x * adaptation, color.y * adaptation, color.z * adaptation};
    } else {
        color = Math::mix3(accent, {color.x * 0.84f, color.y * 0.84f, color.z * 0.84f}, frameOpacity);
    }
    const float background = relativeLuminance(QColor::fromRgbF(
        std::clamp(color.x, 0.0f, 1.0f), std::clamp(color.y, 0.0f, 1.0f), std::clamp(color.z, 0.0f, 1.0f)));
    auto contrast = [background](const QColor &text) {
        const float foreground = relativeLuminance(text);
        return (std::max(background, foreground) + 0.05f) / (std::min(background, foreground) + 0.05f);
    };
    return contrast(dark) > contrast(light) ? dark : light;
}

inline QColor titleShadowColor(const QColor &text)
{
    return relativeLuminance(text) < 0.5f ? QColor(255, 255, 255, 160) : QColor(0, 0, 0, 205);
}

// Renders the exact SDF glass frame surface (matching preview/glass.frag) into a cached QImage.
// Used when the KWin shader effect is not active (e.g. in System Settings preview or standard KWin blur).
inline QImage renderFrameGlassImage(const QSize &pixelSize,
                                    qreal dpr,
                                    const QRectF &frameRect,
                                    const QRectF &contentRect,
                                    const QColor &accentColor,
                                    float tintStrength,
                                    float outerRadius,
                                    const MaterialConfig &cfg)
{
    if (pixelSize.isEmpty()) {
        return {};
    }
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    const float scale = std::max(float(dpr), 1.0f);
    const float invScale = 1.0f / scale;
    const Math::Vec3 accent{float(accentColor.redF()), float(accentColor.greenF()), float(accentColor.blueF())};

    // Normalized light vector (-0.5, -0.85, 1.2) + view (0, 0, 1) -> half-vector
    constexpr float hx = -0.2073f;
    constexpr float hy = -0.3524f;
    constexpr float hz = 0.9124f;
    // Normalized edge light direction (-0.55, -1.0)
    constexpr float dirLightX = -0.4819f;
    constexpr float dirLightY = -0.8762f;

    const int w = pixelSize.width();
    const int h = pixelSize.height();
    const int holeLeft = std::max(0, int(std::floor((contentRect.left() + 3.0) * scale)));
    const int holeRight = std::min(w, int(std::ceil((contentRect.right() - 3.0) * scale)));
    const int holeTop = std::max(0, int(std::floor((contentRect.top() + 3.0) * scale)));
    const int holeBottom = std::min(h, int(std::ceil((contentRect.bottom() - 3.0) * scale)));

    const float aaBand = 0.65f / scale;
    const float epsilon = 0.45f;
    const float titleH = float(cfg.titleHeight);

    for (int py = 0; py < h; ++py) {
        QRgb *scanline = reinterpret_cast<QRgb *>(image.scanLine(py));
        const float y = (float(py) + 0.5f) * invScale;
        const bool inHoleY = (py >= holeTop && py < holeBottom);

        auto processSpan = [&](int xStart, int xEnd) {
            for (int px = xStart; px < xEnd; ++px) {
                const float x = (float(px) + 0.5f) * invScale;
                const float dist = Math::frameDistance(x, y, frameRect, contentRect, outerRadius);
                const float coverage = 1.0f - Math::smoothstep(-aaBand, aaBand, dist);
                if (coverage <= 0.001f) {
                    continue;
                }

                const float gx = Math::frameDistance(x + epsilon, y, frameRect, contentRect, outerRadius)
                               - Math::frameDistance(x - epsilon, y, frameRect, contentRect, outerRadius);
                const float gy = Math::frameDistance(x, y + epsilon, frameRect, contentRect, outerRadius)
                               - Math::frameDistance(x, y - epsilon, frameRect, contentRect, outerRadius);
                const float glen = std::max(std::sqrt(gx * gx + gy * gy), 0.0001f);
                const float dx = gx / glen;
                const float dy = gy / glen;

                const float bevel = std::exp(-std::max(-dist, 0.0f) / 2.4f);
                float nx = dx * bevel * 1.6f;
                float ny = dy * bevel * 1.6f;
                float nz = 1.0f;
                const float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
                nx /= nlen;
                ny /= nlen;
                nz /= nlen;

                // Subtle synthetic refraction sheen for standalone/preview mode
                const float sheen = 0.22f + 0.08f * (nx * -0.6f + ny * -0.8f) * cfg.refraction;
                Math::Vec3 bg{sheen, sheen * 1.04f, sheen * 1.06f};

                const float baseTint = std::clamp(tintStrength, 0.0f, 1.0f);
                const float tintMul = baseTint * 0.65f;
                Math::Vec3 color{
                    bg.x * Math::mixf(1.0f, accent.x * 0.8f + 0.2f, tintMul),
                    bg.y * Math::mixf(1.0f, accent.y * 0.8f + 0.2f, tintMul),
                    bg.z * Math::mixf(1.0f, accent.z * 0.8f + 0.2f, tintMul),
                };
                color = Math::mix3(color, {accent.x * 0.45f, accent.y * 0.45f, accent.z * 0.45f}, baseTint * 0.26f);
                color = Math::boostTint(color, accent, tintStrength);

                const float ndoth = std::max(nx * hx + ny * hy + nz * hz, 0.0f);
                const float specular = std::pow(ndoth, 40.0f) * cfg.specular;
                const float fresnel = std::pow(1.0f - nz, 3.0f) * cfg.specular;
                const float directionalEdge = dx * dirLightX + dy * dirLightY;

                const float edgeBoost = 1.0f + directionalEdge * bevel * 0.28f * cfg.specular;
                color.x *= edgeBoost;
                color.y *= edgeBoost;
                color.z *= edgeBoost;

                const Math::Vec3 specTint = Math::mix3({1.0f, 1.0f, 1.0f}, accent, 0.18f);
                const float specAdd = specular * 0.65f + fresnel * 0.25f;
                color.x += specTint.x * specAdd;
                color.y += specTint.y * specAdd;
                color.z += specTint.z * specAdd;

                // Chromatic dispersion highlight along bevel
                if (cfg.dispersion > 0.01f) {
                    const float prism = bevel * directionalEdge * cfg.dispersion * 0.18f;
                    color.x = Math::clampf(color.x + prism, 0.0f, 1.0f);
                    color.z = Math::clampf(color.z - prism, 0.0f, 1.0f);
                }

                // Crisp outer/inner contour and dark lip (exact match to glass.frag)
                const float cArg = (dist + 0.65f * invScale) * scale / 0.48f;
                const float contour = std::exp(-cArg * cArg);
                const Math::Vec3 contourColor = Math::mix3(accent, {1.0f, 1.0f, 1.0f}, std::max(directionalEdge, 0.0f) * 0.7f);
                color = Math::mix3(color, contourColor, contour * 0.82f);

                const float lArg = (dist + 1.65f * invScale) * scale / 0.65f;
                const float darkLip = std::exp(-lArg * lArg);
                const float lipMul = 1.0f - darkLip * 0.24f;
                color.x *= lipMul;
                color.y *= lipMul;
                color.z *= lipMul;

                // Polished reflection band along the titlebar
                const float titleY = y - float(frameRect.y());
                float bandHighlight = 0.0f;
                if (titleY < titleH) {
                    const float bArg = (titleY - 9.0f) / 5.0f;
                    bandHighlight = 0.18f * std::exp(-bArg * bArg) * cfg.specular;
                    color.x += bandHighlight;
                    color.y += bandHighlight;
                    color.z += bandHighlight;
                    const float stepDark = 1.0f - 0.16f * Math::smoothstep(16.0f, 21.0f, titleY);
                    color.x *= stepDark;
                    color.y *= stepDark;
                    color.z *= stepDark;
                }

                const float localOpacity = Math::clampf(
                    cfg.frameOpacity + contour * 0.35f + specAdd * 0.45f + bevel * 0.14f + bandHighlight * 0.5f,
                    0.0f,
                    1.0f);
                scanline[px] = Math::toPremultipliedRgb(color, localOpacity * coverage);
            }
        };

        if (inHoleY && holeRight > holeLeft) {
            processSpan(0, holeLeft);
            processSpan(holeRight, w);
        } else {
            processSpan(0, w);
        }
    }

    return image;
}

// Renders a single 3D glass button lens (matching preview/glass.frag) directly onto QPainter.
inline void paintButtonGlass(QPainter *p,
                             const QRectF &buttonRect,
                             qreal dpr,
                             const QColor &accentColor,
                             float tintStrength,
                             bool isCloseButton,
                             bool hovered,
                             bool pressed,
                             const QPointF &pointer,
                             const MaterialConfig &cfg)
{
    const float scale = std::max(float(dpr), 1.0f);
    const float invScale = 1.0f / scale;
    const QRect aligned = buttonRect.adjusted(-1, -1, 1, 1).toAlignedRect();
    const QSize pixelSize( std::max(1, int(std::ceil(aligned.width() * scale))),
                           std::max(1, int(std::ceil(aligned.height() * scale))) );

    QImage img(pixelSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    const bool closeFeedback = isCloseButton && (hovered || pressed);
    const Math::Vec3 baseAccent{float(accentColor.redF()), float(accentColor.greenF()), float(accentColor.blueF())};
    const Math::Vec3 accent = closeFeedback ? Math::closeButtonAccent(baseAccent) : baseAccent;
    constexpr float hx = -0.2073f;
    constexpr float hy = -0.3524f;
    constexpr float hz = 0.9124f;
    constexpr float dirLightX = -0.4819f;
    constexpr float dirLightY = -0.8762f;

    const float bx = float(buttonRect.x());
    const float by = float(buttonRect.y());
    const float bw = float(buttonRect.width());
    const float bh = float(buttonRect.height());
    const float aaBand = 0.65f / scale;
    const float epsilon = 0.45f;

    for (int py = 0; py < pixelSize.height(); ++py) {
        QRgb *scanline = reinterpret_cast<QRgb *>(img.scanLine(py));
        const float y = float(aligned.y()) + (float(py) + 0.5f) * invScale;
        for (int px = 0; px < pixelSize.width(); ++px) {
            const float x = float(aligned.x()) + (float(px) + 0.5f) * invScale;
            const float d = Math::boxDistance(x, y, bx, by, bw, bh, 3.0f);
            const float coverage = 1.0f - Math::smoothstep(-aaBand, aaBand, d);
            if (coverage <= 0.001f) {
                continue;
            }

            const float gx = Math::boxDistance(x + epsilon, y, bx, by, bw, bh, 3.0f)
                           - Math::boxDistance(x - epsilon, y, bx, by, bw, bh, 3.0f);
            const float gy = Math::boxDistance(x, y + epsilon, bx, by, bw, bh, 3.0f)
                           - Math::boxDistance(x, y - epsilon, bx, by, bw, bh, 3.0f);
            const float glen = std::max(std::sqrt(gx * gx + gy * gy), 0.0001f);
            const float dx = gx / glen;
            const float dy = gy / glen;

            const float bevel = std::exp(-std::max(-d, 0.0f) / 4.0f);
            const float normalSign = pressed ? -0.8f : 1.6f;
            float nx = dx * bevel * normalSign;
            float ny = dy * bevel * normalSign;
            float nz = 1.0f;
            const float nlen = std::sqrt(nx * nx + ny * ny + nz * nz);
            nx /= nlen;
            ny /= nlen;
            nz /= nlen;

            Math::Vec3 color{0.18f, 0.20f, 0.21f};
            const float baseTint = closeFeedback ? 1.0f : std::clamp(tintStrength, 0.0f, 1.0f);
            const float tintMul = baseTint * 0.65f;
            color.x *= Math::mixf(1.0f, accent.x * 0.8f + 0.2f, tintMul);
            color.y *= Math::mixf(1.0f, accent.y * 0.8f + 0.2f, tintMul);
            color.z *= Math::mixf(1.0f, accent.z * 0.8f + 0.2f, tintMul);
            color = Math::mix3(color, {accent.x * 0.45f, accent.y * 0.45f, accent.z * 0.45f}, baseTint * 0.20f);
            color = Math::boostTint(color, accent, tintStrength);
            if (closeFeedback) {
                color = Math::mix3(color, {accent.x * 0.65f, accent.y * 0.65f, accent.z * 0.65f}, 0.75f);
            }

            const float ndoth = std::max(nx * hx + ny * hy + nz * hz, 0.0f);
            const float specular = std::pow(ndoth, 40.0f) * cfg.specular;
            const float fresnel = std::pow(1.0f - nz, 3.0f) * cfg.specular;
            const float directionalEdge = dx * dirLightX + dy * dirLightY;

            const float edgeBoost = 1.0f + directionalEdge * bevel * 0.28f * cfg.specular;
            color.x *= edgeBoost;
            color.y *= edgeBoost;
            color.z *= edgeBoost;

            const Math::Vec3 specTint = Math::mix3({1.0f, 1.0f, 1.0f}, accent, 0.18f);
            const float specAdd = specular * 0.65f + fresnel * 0.25f;
            color.x += specTint.x * specAdd;
            color.y += specTint.y * specAdd;
            color.z += specTint.z * specAdd;

            const float cArg = (d + 0.65f * invScale) * scale / 0.48f;
            const float contour = std::exp(-cArg * cArg);
            const Math::Vec3 contourColor = Math::mix3(accent, {1.0f, 1.0f, 1.0f}, std::max(directionalEdge, 0.0f) * 0.7f);
            color = Math::mix3(color, contourColor, contour * 0.82f);

            const float lArg = (d + 1.65f * invScale) * scale / 0.65f;
            const float darkLip = std::exp(-lArg * lArg);
            const float lipMul = 1.0f - darkLip * 0.24f;
            color.x *= lipMul;
            color.y *= lipMul;
            color.z *= lipMul;

            // Button lens profile (exact match to glass.frag lines 83-92)
            const float relY = (y - by) / std::max(bh, 1.0f);
            const float pressDim = pressed ? 0.55f : 0.68f;
            color.x *= pressDim;
            color.y *= pressDim;
            color.z *= pressDim;

            const Math::Vec3 topHighlight = Math::mix3(accent, {1.0f, 1.0f, 1.0f}, 0.75f);
            const float topFactor = (1.0f - Math::smoothstep(0.05f, 0.5f, relY)) * (pressed ? 0.13f : 0.34f);
            color.x += topHighlight.x * topFactor + accent.x * 0.11f;
            color.y += topHighlight.y * topFactor + accent.y * 0.11f;
            color.z += topHighlight.z * topFactor + accent.z * 0.11f;

            if (hovered) {
                const float distPtr = std::hypot(x - float(pointer.x()), y - float(pointer.y()));
                const float glow = std::exp(-distPtr / 24.0f) * 0.55f * cfg.hoverGlow;
                const Math::Vec3 glowColor = accent;
                color.x += glowColor.x * glow;
                color.y += glowColor.y * glow;
                color.z += glowColor.z * glow;
            }

            const float alpha = Math::clampf(0.88f + contour * 0.12f, 0.0f, 1.0f) * coverage;
            scanline[px] = Math::toPremultipliedRgb(color, alpha);
        }
    }

    img.setDevicePixelRatio(scale);
    p->drawImage(aligned.topLeft(), img);
}

// Builds a 9-patch shadow image with rounded-box SDF and transparent interior.
inline QImage renderShadowImage(int pad, int inside, float radius)
{
    const int total = inside + 2 * pad;
    QImage image(total, total, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    const float bx = float(pad);
    const float by = float(pad);
    const float bw = float(inside);
    const float bh = float(inside);

    for (int py = 0; py < total; ++py) {
        QRgb *scanline = reinterpret_cast<QRgb *>(image.scanLine(py));
        const float y = float(py) + 0.5f;
        for (int px = 0; px < total; ++px) {
            const float x = float(px) + 0.5f;
            const float d = Math::boxDistance(x, y, bx, by, bw, bh, radius);
            if (d <= -0.5f) {
                continue;
            }
            const float edgeFade = Math::smoothstep(-0.5f, 0.5f, d);
            const float falloff = std::exp(-std::max(d, 0.0f) / 5.0f);
            const float outerFade = 1.0f - Math::smoothstep(float(pad - 4), float(pad), d);
            const float alpha = Math::clampf(falloff * 0.18f * edgeFade * outerFade, 0.0f, 1.0f);
            const int ai = int(std::lround(alpha * 255.0f));
            scanline[px] = qRgba(0, 0, 0, ai);
        }
    }
    return image;
}

} // namespace EmeraldGlass
