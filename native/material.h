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
#include <QSizeF>
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

enum class GradientMode : int {
    Solid = 0,
    HorizontalSweep = 1,
    DiagonalPrism = 2,
    CenterAura = 3,
    RimEdgeSplit = 4,
};

enum class GradientSecondarySource : int {
    AutoAnalogous = 0,
    Custom = 1,
};

enum class ButtonShape : int {
    Classic = 0,
    JoinedStrip = 1,
    TopDockedTabs = 2,
    LiquidCapsules = 3,
    StealthLens = 4,
};

struct ButtonRadii {
    float tl = 3.0f;
    float tr = 3.0f;
    float br = 3.0f;
    float bl = 3.0f;
    bool operator==(const ButtonRadii &) const = default;
};

inline QSizeF buttonSizeForShape(int shape, bool isClose, bool isSpacer)
{
    const qreal height = (shape == int(ButtonShape::LiquidCapsules)) ? 22.0 : 23.0;
    if (isSpacer) {
        return QSizeF(10.0, height);
    }
    if (shape == int(ButtonShape::JoinedStrip) || shape == int(ButtonShape::TopDockedTabs)) {
        return QSizeF(isClose ? 35.0 : 26.0, 23.0);
    }
    if (shape == int(ButtonShape::LiquidCapsules)) {
        return QSizeF(29.0, 22.0);
    }
    return QSizeF(27.0, 23.0);
}

inline qreal buttonSpacingForShape(int shape)
{
    if (shape == int(ButtonShape::JoinedStrip) || shape == int(ButtonShape::TopDockedTabs)) {
        return 0.0;
    }
    if (shape == int(ButtonShape::LiquidCapsules)) {
        return 4.0;
    }
    return 3.0;
}

inline qreal buttonTopYForShape(int shape, qreal titleHeight, bool maximized)
{
    if (shape == int(ButtonShape::TopDockedTabs)) {
        return maximized ? 0.0 : 1.0;
    }
    const qreal btnH = (shape == int(ButtonShape::LiquidCapsules)) ? 22.0 : 23.0;
    return std::max(2.0, std::floor((titleHeight - btnH) * 0.5));
}

inline qreal buttonEdgeInsetForShape(int shape, qreal cornerRadius, bool isLeft)
{
    const qreal base = isLeft ? 14.0 : 17.0;
    if (shape == int(ButtonShape::TopDockedTabs)) {
        return std::max(base, cornerRadius + 2.0);
    }
    return base;
}

inline ButtonRadii buttonRadiiForShape(int shape, bool joinedLeft, bool joinedRight)
{
    if (shape == int(ButtonShape::JoinedStrip)) {
        return ButtonRadii{
            joinedLeft ? 0.5f : 5.0f,
            joinedRight ? 0.5f : 5.0f,
            joinedRight ? 0.5f : 5.0f,
            joinedLeft ? 0.5f : 5.0f,
        };
    }
    if (shape == int(ButtonShape::TopDockedTabs)) {
        return ButtonRadii{
            0.5f,
            0.5f,
            joinedRight ? 0.5f : 5.5f,
            joinedLeft ? 0.5f : 5.5f,
        };
    }
    if (shape == int(ButtonShape::LiquidCapsules)) {
        return ButtonRadii{11.0f, 11.0f, 11.0f, 11.0f};
    }
    if (shape == int(ButtonShape::StealthLens)) {
        return ButtonRadii{5.0f, 5.0f, 5.0f, 5.0f};
    }
    return ButtonRadii{3.0f, 3.0f, 3.0f, 3.0f};
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
    bool desktopBackdropForMaximized = false;

    int cornerRadius = 5;
    int titleHeight = 39;
    int borderWidth = 8;
    int titleAlignment = 0; // 0 = Left, 1 = Center, 2 = CenterFullWidth
    int gradientMode = 0; // 0 = Solid, 1 = HorizontalSweep, 2 = DiagonalPrism, 3 = CenterAura, 4 = RimEdgeSplit
    int gradientSecondarySource = 0; // 0 = AutoAnalogous, 1 = Custom
    QColor activeSecondaryColor = QColor(0x14, 0xd8, 0xff);
    QColor inactiveSecondaryColor = QColor(0xff, 0x9e, 0x14);
    bool gradientActiveOnly = false;
    int buttonShape = 0; // 0 = Classic, 1 = JoinedStrip, 2 = TopDockedTabs, 3 = LiquidCapsules, 4 = StealthLens
    int buttonColorMode = 0; // 0 = Monochrome, 1 = AlwaysTintedClose, 2 = AdaptiveJewelTrio, 3 = HarmonicHsl, 4 = SmokedChrome
    bool useSystemButtons = false;
    bool shaderEffectEnabled = true;

    QColor colorForWindow(bool active) const
    {
        return active
            ? resolvePresetColor(activePreset, activeCustomColor, QColor(0x17, 0xf5, 0x59))
            : resolvePresetColor(inactivePreset, inactiveCustomColor, QColor(0xf7, 0x14, 0x0e));
    }

    int effectiveGradientMode(bool active) const
    {
        if (gradientMode <= 0 || (gradientActiveOnly && !active)) {
            return 0;
        }
        return std::clamp(gradientMode, 0, 4);
    }

    QColor secondaryColorForWindow(bool active) const;

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
    cfg.desktopBackdropForMaximized = mat.readEntry("DesktopBackdropForMaximized", false);
    cfg.shaderEffectEnabled = mat.readEntry("ShaderEffectEnabled", true);
    cfg.gradientMode = std::clamp(mat.readEntry("GradientMode", 0), 0, 4);
    cfg.gradientSecondarySource = std::clamp(mat.readEntry("GradientSecondarySource", 0), 0, 1);
    cfg.activeSecondaryColor = mat.readEntry("ActiveSecondaryColor", QColor(0x14, 0xd8, 0xff));
    cfg.inactiveSecondaryColor = mat.readEntry("InactiveSecondaryColor", QColor(0xff, 0x9e, 0x14));
    cfg.gradientActiveOnly = mat.readEntry("GradientActiveOnly", false);

    KConfigGroup geom(config, QStringLiteral("Geometry"));
    cfg.cornerRadius = std::clamp(geom.readEntry("CornerRadius", 5), 0, 18);
    cfg.titleHeight = std::clamp(geom.readEntry("TitleHeight", 39), 26, 56);
    cfg.borderWidth = std::clamp(geom.readEntry("BorderWidth", 8), 0, 20);
    cfg.titleAlignment = std::clamp(geom.readEntry("TitleAlignment", 0), 0, 2);
    cfg.buttonShape = std::clamp(geom.readEntry("ButtonShape", 0), 0, 4);
    cfg.buttonColorMode = std::clamp(geom.readEntry("ButtonColorMode", 0), 0, 4);
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
    mat.writeEntry("DesktopBackdropForMaximized", cfg.desktopBackdropForMaximized);
    mat.writeEntry("ShaderEffectEnabled", cfg.shaderEffectEnabled);
    mat.writeEntry("GradientMode", std::clamp(cfg.gradientMode, 0, 4));
    mat.writeEntry("GradientSecondarySource", std::clamp(cfg.gradientSecondarySource, 0, 1));
    mat.writeEntry("ActiveSecondaryColor", cfg.activeSecondaryColor);
    mat.writeEntry("InactiveSecondaryColor", cfg.inactiveSecondaryColor);
    mat.writeEntry("GradientActiveOnly", cfg.gradientActiveOnly);

    KConfigGroup geom(config, QStringLiteral("Geometry"));
    geom.writeEntry("CornerRadius", cfg.cornerRadius);
    geom.writeEntry("TitleHeight", cfg.titleHeight);
    geom.writeEntry("BorderWidth", cfg.borderWidth);
    geom.writeEntry("TitleAlignment", cfg.titleAlignment);
    geom.writeEntry("ButtonShape", std::clamp(cfg.buttonShape, 0, 4));
    geom.writeEntry("ButtonColorMode", std::clamp(cfg.buttonColorMode, 0, 4));
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

struct Hsv {
    float h = 0.0f; // 0..360
    float s = 0.0f; // 0..1
    float v = 0.0f; // 0..1
};

inline Hsv rgbToHsv(Vec3 c)
{
    const float maxC = std::max({c.x, c.y, c.z});
    const float minC = std::min({c.x, c.y, c.z});
    const float d = maxC - minC;
    float h = 0.0f;
    if (d > 0.0001f) {
        if (maxC == c.x) {
            h = std::fmod((c.y - c.z) / d, 6.0f);
        } else if (maxC == c.y) {
            h = (c.z - c.x) / d + 2.0f;
        } else {
            h = (c.x - c.y) / d + 4.0f;
        }
        h *= 60.0f;
        if (h < 0.0f) {
            h += 360.0f;
        }
    }
    const float s = maxC > 0.0001f ? d / maxC : 0.0f;
    return {h, s, maxC};
}

inline Vec3 hsvToRgb(float hueDeg, float sat, float val)
{
    float h = std::fmod(hueDeg, 360.0f);
    if (h < 0.0f) {
        h += 360.0f;
    }
    const float s = clampf(sat, 0.0f, 1.0f);
    const float v = clampf(val, 0.0f, 1.0f);
    const float c = v * s;
    const float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    const float m = v - c;
    Vec3 rgb;
    if (h < 60.0f) {
        rgb = {c, x, 0.0f};
    } else if (h < 120.0f) {
        rgb = {x, c, 0.0f};
    } else if (h < 180.0f) {
        rgb = {0.0f, c, x};
    } else if (h < 240.0f) {
        rgb = {0.0f, x, c};
    } else if (h < 300.0f) {
        rgb = {x, 0.0f, c};
    } else {
        rgb = {c, 0.0f, x};
    }
    return {rgb.x + m, rgb.y + m, rgb.z + m};
}

// Close feedback adapts by HSV hue so red/pink/rose and orange/amber/gold materials stay distinct.
inline Vec3 closeButtonAccent(Vec3 base)
{
    const Hsv hsv = rgbToHsv(base);
    if (hsv.s <= 0.15f) {
        return {0.92f, 0.08f, 0.06f};
    }
    if (hsv.h >= 305.0f || hsv.h < 18.0f) {
        return {1.0f, 0.70f, 0.12f};
    }
    if (hsv.h >= 18.0f && hsv.h < 52.0f) {
        return {0.08f, 0.40f, 1.0f};
    }
    return {0.92f, 0.08f, 0.06f};
}

inline float boxDistance(float px, float py, float bx, float by, float bw, float bh, float radius)
{
    const float qx = std::abs(px - bx - bw * 0.5f) - bw * 0.5f + radius;
    const float qy = std::abs(py - by - bh * 0.5f) - bh * 0.5f + radius;
    const float mx = std::max(qx, 0.0f);
    const float my = std::max(qy, 0.0f);
    return std::min(std::max(qx, qy), 0.0f) + std::sqrt(mx * mx + my * my) - radius;
}

inline float roundedBoxDistance(float px, float py, float bx, float by, float bw, float bh, const ButtonRadii &radii)
{
    const float hx = bw * 0.5f;
    const float hy = bh * 0.5f;
    const float rx = px - (bx + hx);
    const float ry = py - (by + hy);
    float r = ry > 0.0f ? (rx > 0.0f ? radii.br : radii.bl) : (rx > 0.0f ? radii.tr : radii.tl);
    r = std::min(r, std::min(hx, hy));
    const float qx = std::abs(rx) - hx + r;
    const float qy = std::abs(ry) - hy + r;
    const float mx = std::max(qx, 0.0f);
    const float my = std::max(qy, 0.0f);
    return std::min(std::max(qx, qy), 0.0f) + std::sqrt(mx * mx + my * my) - r;
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

inline void evalFrameGradient(float x,
                              float y,
                              const QRectF &frame,
                              float edgeFactor,
                              int gradMode,
                              Vec3 c1,
                              Vec3 c2,
                              Vec3 &bodyAccent,
                              Vec3 &rimAccent)
{
    if (gradMode <= 0) {
        bodyAccent = c1;
        rimAccent = c1;
        return;
    }
    const float u = clampf((x - float(frame.x())) / std::max(float(frame.width()), 1.0f), 0.0f, 1.0f);
    const float v = clampf((y - float(frame.y())) / std::max(float(frame.height()), 1.0f), 0.0f, 1.0f);
    if (gradMode == int(GradientMode::HorizontalSweep)) {
        const float t = smoothstep(0.08f, 0.92f, u);
        bodyAccent = mix3(c1, c2, t);
        rimAccent = bodyAccent;
        return;
    }
    if (gradMode == int(GradientMode::DiagonalPrism)) {
        const float t = smoothstep(0.12f, 0.96f, u * 0.72f + v * 0.38f);
        bodyAccent = mix3(c1, c2, t);
        rimAccent = bodyAccent;
        return;
    }
    if (gradMode == int(GradientMode::CenterAura)) {
        const float dist = std::hypot((u - 0.48f) * 1.18f, (v - 0.22f) * 2.0f);
        const float t = 1.0f - smoothstep(0.06f, 0.56f, dist);
        bodyAccent = mix3(c1, c2, t);
        rimAccent = bodyAccent;
        return;
    }
    if (gradMode == int(GradientMode::RimEdgeSplit)) {
        const float bodyT = clampf(smoothstep(0.15f, 0.95f, v) * 0.24f + edgeFactor * 0.50f, 0.0f, 1.0f);
        const float rimT = clampf(0.92f * edgeFactor + 0.45f * smoothstep(0.15f, 0.90f, v + u * 0.25f), 0.0f, 1.0f);
        bodyAccent = mix3(c1, c2, bodyT);
        rimAccent = mix3(c1, c2, rimT);
        return;
    }
    bodyAccent = c1;
    rimAccent = c1;
}

} // namespace Math

inline QColor autoAnalogousSecondaryColor(const QColor &primary)
{
    const Math::Vec3 c{float(primary.redF()), float(primary.greenF()), float(primary.blueF())};
    const Math::Hsv hsv = Math::rgbToHsv(c);
    Math::Vec3 out;
    if (hsv.s < 0.15f) {
        out = Math::hsvToRgb(235.0f, 0.28f, Math::clampf(std::max(hsv.v, 0.78f), 0.78f, 0.96f));
    } else if (hsv.h >= 175.0f && hsv.h < 230.0f && hsv.s < 0.45f && hsv.v > 0.75f) {
        out = Math::hsvToRgb(hsv.h + 62.0f, 0.30f, 0.96f);
    } else {
        float h2 = hsv.h + 45.0f;
        if (hsv.h >= 340.0f || hsv.h < 18.0f) {
            h2 = hsv.h + 38.0f;
        } else if (hsv.h >= 18.0f && hsv.h < 65.0f) {
            h2 = hsv.h - 36.0f;
        } else if (hsv.h >= 65.0f && hsv.h < 175.0f) {
            h2 = hsv.h + 52.0f;
        } else if (hsv.h >= 175.0f && hsv.h < 230.0f) {
            h2 = hsv.h + 68.0f;
        } else {
            h2 = hsv.h - 52.0f;
        }
        out = Math::hsvToRgb(h2, Math::clampf(hsv.s, 0.28f, 0.95f), Math::clampf(std::max(hsv.v, 0.85f), 0.85f, 1.0f));
    }
    return QColor::fromRgbF(
        Math::clampf(out.x, 0.0f, 1.0f),
        Math::clampf(out.y, 0.0f, 1.0f),
        Math::clampf(out.z, 0.0f, 1.0f));
}

inline QColor MaterialConfig::secondaryColorForWindow(bool active) const
{
    if (gradientSecondarySource == int(GradientSecondarySource::Custom)) {
        const QColor custom = active ? activeSecondaryColor : inactiveSecondaryColor;
        if (custom.isValid()) {
            return custom;
        }
    }
    return autoAnalogousSecondaryColor(colorForWindow(active));
}

inline QColor sampleGradientColor(const QRectF &frame,
                                  const QPointF &pos,
                                  int gradMode,
                                  const QColor &primary,
                                  const QColor &secondary)
{
    if (gradMode <= 0 || gradMode == int(GradientMode::RimEdgeSplit)) {
        return primary;
    }
    Math::Vec3 body, rim;
    Math::evalFrameGradient(
        float(pos.x()),
        float(pos.y()),
        frame,
        0.0f,
        gradMode,
        {float(primary.redF()), float(primary.greenF()), float(primary.blueF())},
        {float(secondary.redF()), float(secondary.greenF()), float(secondary.blueF())},
        body,
        rim);
    return QColor::fromRgbF(
        Math::clampf(body.x, 0.0f, 1.0f),
        Math::clampf(body.y, 0.0f, 1.0f),
        Math::clampf(body.z, 0.0f, 1.0f));
}

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

    const int gradMode = effectiveGradientMode(active);
    QColor preset = colorForWindow(active);
    if (gradMode > 0 && gradMode != int(GradientMode::RimEdgeSplit)) {
        const qreal titleX = (titleAlignment == 0) ? 25.0 : 50.0;
        preset = sampleGradientColor(
            QRectF(0.0, 0.0, 100.0, 100.0),
            QPointF(titleX, 14.0),
            gradMode,
            preset,
            secondaryColorForWindow(active));
    }
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

enum class ButtonColorMode : int {
    Monochrome = 0,
    AlwaysTintedClose = 1,
    AdaptiveJewelTrio = 2,
    HarmonicHsl = 3,
    SmokedChrome = 4,
};

enum class ButtonColorRole : int {
    Other = 0,
    Minimize = 1,
    Maximize = 2,
    Close = 3,
};

struct ButtonColorSpec {
    QColor color;
    float fillBoost = 0.0f;
    float effectiveTint = 0.70f;
    float rimBlend = 0.0f;
    float restrainedLight = 0.0f;
    bool operator==(const ButtonColorSpec &) const = default;
};

inline ButtonColorSpec resolveButtonColor(const QColor &windowAccent,
                                          float windowTint,
                                          int colorMode,
                                          ButtonColorRole role,
                                          bool hovered,
                                          bool pressed,
                                          bool active)
{
    const Math::Vec3 baseAccent{float(windowAccent.redF()), float(windowAccent.greenF()), float(windowAccent.blueF())};
    const float paleBase = Math::smoothstep(0.40f, 0.70f, std::min({baseAccent.x, baseAccent.y, baseAccent.z}));
    const bool isClose = (role == ButtonColorRole::Close);
    const bool interactive = hovered || pressed;
    const float inactiveFillScale = active ? 1.0f : 0.72f;
    auto toQColor = [](const Math::Vec3 &v) {
        return QColor::fromRgbF(
            Math::clampf(v.x, 0.0f, 1.0f),
            Math::clampf(v.y, 0.0f, 1.0f),
            Math::clampf(v.z, 0.0f, 1.0f));
    };

    if (colorMode == int(ButtonColorMode::AlwaysTintedClose)) {
        if (isClose) {
            const Math::Vec3 c = Math::closeButtonAccent(baseAccent);
            return ButtonColorSpec{
                toQColor(c),
                interactive ? 0.44f : 0.24f * inactiveFillScale,
                interactive ? 1.0f : 0.90f,
                interactive ? 0.45f : 0.30f,
                interactive ? 0.28f : 0.32f,
            };
        }
        return ButtonColorSpec{windowAccent, 0.0f, windowTint, 0.0f, paleBase};
    }

    if (colorMode == int(ButtonColorMode::AdaptiveJewelTrio)) {
        if (role == ButtonColorRole::Other) {
            return ButtonColorSpec{windowAccent, 0.0f, windowTint, 0.0f, paleBase};
        }
        const Math::Hsv hsv = Math::rgbToHsv(baseAccent);
        float minH = 38.0f;
        float maxH = 140.0f;
        float clsH = 356.0f;
        if (hsv.s > 0.22f) {
            if (hsv.h >= 305.0f || hsv.h < 18.0f) {
                minH = 204.0f;
                maxH = 140.0f;
                clsH = 44.0f;
            } else if (hsv.h >= 18.0f && hsv.h < 52.0f) {
                minH = 185.0f;
                maxH = 135.0f;
                clsH = 222.0f;
            } else if (hsv.h >= 52.0f && hsv.h < 68.0f) {
                minH = 190.0f;
                maxH = 138.0f;
                clsH = 356.0f;
            } else if (hsv.h >= 68.0f && hsv.h < 168.0f) {
                minH = 38.0f;
                maxH = 205.0f;
                clsH = 356.0f;
            }
        }
        const float hue = role == ButtonColorRole::Minimize ? minH
            : (role == ButtonColorRole::Maximize ? maxH : clsH);
        const bool paleTheme = (hsv.s < 0.38f && hsv.v > 0.75f);
        const float sat = paleTheme ? 0.80f : 0.88f;
        const float val = paleTheme ? 0.84f : 0.95f;
        return ButtonColorSpec{
            toQColor(Math::hsvToRgb(hue, sat, val)),
            interactive ? 0.44f : 0.28f * inactiveFillScale,
            interactive ? 1.0f : 0.95f,
            interactive ? 0.65f : 0.55f,
            interactive ? 0.24f : 0.28f,
        };
    }

    if (colorMode == int(ButtonColorMode::HarmonicHsl)) {
        if (role == ButtonColorRole::Other) {
            return ButtonColorSpec{windowAccent, 0.0f, windowTint, 0.0f, paleBase};
        }
        const Math::Hsv hsv = Math::rgbToHsv(baseAccent);
        const bool achromatic = (hsv.s < 0.18f);
        const float h = achromatic ? 195.0f : hsv.h;
        const float effS = achromatic ? 0.72f : Math::clampf(std::max(hsv.s, 0.68f), 0.68f, 0.92f);
        const float effV = achromatic ? 0.90f : Math::clampf(std::max(hsv.v, 0.85f), 0.85f, 0.96f);
        if (role == ButtonColorRole::Minimize) {
            return ButtonColorSpec{
                toQColor(Math::hsvToRgb(h - 30.0f, effS * 0.88f, effV)),
                interactive ? 0.34f : 0.18f * inactiveFillScale,
                interactive ? 1.0f : 0.88f,
                0.65f,
                0.26f,
            };
        }
        if (role == ButtonColorRole::Maximize) {
            return ButtonColorSpec{
                toQColor(Math::hsvToRgb(h + 30.0f, effS * 0.92f, effV)),
                interactive ? 0.34f : 0.18f * inactiveFillScale,
                interactive ? 1.0f : 0.88f,
                0.65f,
                0.26f,
            };
        }
        return ButtonColorSpec{
            toQColor(Math::hsvToRgb(h + 175.0f, std::max(effS, 0.86f), 0.96f)),
            interactive ? 0.45f : 0.28f * inactiveFillScale,
            interactive ? 1.0f : 0.95f,
            0.55f,
            0.26f,
        };
    }

    if (colorMode == int(ButtonColorMode::SmokedChrome)) {
        if (interactive) {
            if (isClose) {
                return ButtonColorSpec{
                    toQColor(Math::closeButtonAccent(baseAccent)),
                    0.42f,
                    1.0f,
                    0.75f,
                    0.28f,
                };
            }
            return ButtonColorSpec{
                windowAccent,
                0.20f,
                0.90f,
                0.85f,
                paleBase,
            };
        }
        return ButtonColorSpec{
            toQColor(Math::mix3({0.60f, 0.64f, 0.68f}, baseAccent, 0.18f)),
            0.0f,
            0.45f,
            1.0f,
            0.48f,
        };
    }

    // Mode 0: Monochrome
    if (isClose && interactive) {
        return ButtonColorSpec{
            toQColor(Math::closeButtonAccent(baseAccent)),
            0.35f,
            1.0f,
            0.20f,
            0.35f,
        };
    }
    return ButtonColorSpec{windowAccent, 0.0f, windowTint, 0.0f, paleBase};
}

inline QColor buttonGlyphColor(const ButtonColorSpec &spec,
                               const MaterialConfig &cfg,
                               bool active,
                               bool opticalMaterial,
                               bool enabled,
                               bool stealthIdle = false)
{
    if (!enabled) {
        return QColor("#7c9184");
    }
    const QColor light = active ? QColor("#effff6") : QColor("#d9e8e0");
    const QColor dark = active ? QColor("#14201c") : QColor("#253029");
    if (spec.fillBoost >= 0.12f) {
        return light;
    }
    const int titleMode = active ? cfg.activeTitleText : cfg.inactiveTitleText;
    const bool darkTitle = relativeLuminance(cfg.titleColorForWindow(active, opticalMaterial)) < 0.2f;
    if (stealthIdle) {
        return darkTitle ? dark : light;
    }
    if (!darkTitle && !(titleMode == 0 && cfg.effectiveGradientMode(active) > 0)) {
        return light;
    }
    const Math::Vec3 base{float(spec.color.redF()), float(spec.color.greenF()), float(spec.color.blueF())};
    const Math::Vec3 neutral{0.18f, 0.20f, 0.21f};
    Math::Vec3 body = opticalMaterial ? Math::mix3(base, neutral, cfg.frameOpacity) : neutral;
    const float baseTint = std::clamp(spec.effectiveTint, 0.0f, 1.0f);
    const float tintMul = baseTint * 0.65f;
    body.x *= Math::mixf(1.0f, base.x * 0.8f + 0.2f, tintMul);
    body.y *= Math::mixf(1.0f, base.y * 0.8f + 0.2f, tintMul);
    body.z *= Math::mixf(1.0f, base.z * 0.8f + 0.2f, tintMul);
    body = Math::mix3(body, {base.x * 0.45f, base.y * 0.45f, base.z * 0.45f}, baseTint * 0.20f);
    body = Math::boostTint(body, base, spec.effectiveTint);
    body = {
        Math::clampf(body.x * 0.68f + base.x * 0.11f, 0.0f, 1.0f),
        Math::clampf(body.y * 0.68f + base.y * 0.11f, 0.0f, 1.0f),
        Math::clampf(body.z * 0.68f + base.z * 0.11f, 0.0f, 1.0f),
    };
    const float bgLuma = relativeLuminance(QColor::fromRgbF(body.x, body.y, body.z));
    auto contrast = [bgLuma](const QColor &fg) {
        const float fgLuma = relativeLuminance(fg);
        return (std::max(bgLuma, fgLuma) + 0.05f) / (std::min(bgLuma, fgLuma) + 0.05f);
    };
    return contrast(dark) > contrast(light) ? dark : light;
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
                                    const MaterialConfig &cfg,
                                    int gradientMode = 0,
                                    const QColor &secondaryColor = QColor())
{
    if (pixelSize.isEmpty()) {
        return {};
    }
    QImage image(pixelSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);

    const float scale = std::max(float(dpr), 1.0f);
    const float invScale = 1.0f / scale;
    const Math::Vec3 primaryAccent{float(accentColor.redF()), float(accentColor.greenF()), float(accentColor.blueF())};
    const QColor resolvedSecondary = secondaryColor.isValid() ? secondaryColor : accentColor;
    const Math::Vec3 secondaryAccent{float(resolvedSecondary.redF()), float(resolvedSecondary.greenF()), float(resolvedSecondary.blueF())};

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
                Math::Vec3 accent = primaryAccent;
                Math::Vec3 rimAccent = primaryAccent;
                if (gradientMode > 0) {
                    float gradEdge = bevel;
                    if (gradientMode == int(GradientMode::RimEdgeSplit)) {
                        const float dOuter = Math::boxDistance(
                            x, y, float(frameRect.x()), float(frameRect.y()),
                            float(frameRect.width()), float(frameRect.height()), outerRadius);
                        gradEdge = std::exp(-std::max(-dOuter, 0.0f) / 3.4f);
                    }
                    Math::evalFrameGradient(x, y, frameRect, gradEdge, gradientMode, primaryAccent, secondaryAccent, accent, rimAccent);
                }
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

                const Math::Vec3 specTint = Math::mix3({1.0f, 1.0f, 1.0f}, rimAccent, gradientMode == int(GradientMode::RimEdgeSplit) ? 0.35f : 0.18f);
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
                const Math::Vec3 contourColor = Math::mix3(rimAccent, {1.0f, 1.0f, 1.0f}, std::max(directionalEdge, 0.0f) * 0.7f);
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
                             const MaterialConfig &cfg,
                             const ButtonRadii &radii = ButtonRadii{3.0f, 3.0f, 3.0f, 3.0f},
                             const ButtonColorSpec *colorSpec = nullptr)
{
    const float scale = std::max(float(dpr), 1.0f);
    const float invScale = 1.0f / scale;
    const QRect aligned = buttonRect.adjusted(-1, -1, 1, 1).toAlignedRect();
    const QSize pixelSize( std::max(1, int(std::ceil(aligned.width() * scale))),
                           std::max(1, int(std::ceil(aligned.height() * scale))) );

    QImage img(pixelSize, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);

    const bool stealthIdle = (cfg.buttonShape == int(ButtonShape::StealthLens)) && !hovered && !pressed;
    const bool joined = (cfg.buttonShape == int(ButtonShape::JoinedStrip)) || (cfg.buttonShape == int(ButtonShape::TopDockedTabs));
    const bool seamLeft = joined && radii.bl < 1.0f;
    const bool seamRight = joined && radii.br < 1.0f;
    const bool topDocked = (cfg.buttonShape == int(ButtonShape::TopDockedTabs));

    const bool closeFeedback = !colorSpec && isCloseButton && (hovered || pressed);
    const Math::Vec3 baseAccent{float(accentColor.redF()), float(accentColor.greenF()), float(accentColor.blueF())};
    Math::Vec3 accent = closeFeedback ? Math::closeButtonAccent(baseAccent) : baseAccent;
    float restrainedLight = closeFeedback ? 0.35f
        : Math::smoothstep(0.40f, 0.70f, std::min({baseAccent.x, baseAccent.y, baseAccent.z}));
    Math::Vec3 lightingAccent = closeFeedback ? Math::mix3(baseAccent, accent, 0.20f) : accent;
    float fillBoost = closeFeedback ? 0.35f : 0.0f;
    float effectiveTintValue = closeFeedback ? 1.0f : tintStrength;
    if (colorSpec) {
        accent = {float(colorSpec->color.redF()), float(colorSpec->color.greenF()), float(colorSpec->color.blueF())};
        restrainedLight = Math::clampf(colorSpec->restrainedLight, 0.0f, 1.0f);
        lightingAccent = Math::mix3(baseAccent, accent, Math::clampf(colorSpec->rimBlend, 0.0f, 1.0f));
        fillBoost = std::max(colorSpec->fillBoost, 0.0f);
        effectiveTintValue = colorSpec->effectiveTint;
    }
    constexpr float hx = -0.2073f;
    constexpr float hy = -0.3524f;
    constexpr float hz = 0.9124f;
    constexpr float dirLightX = -0.4819f;
    constexpr float dirLightY = -0.8762f;

    const float bx = float(buttonRect.x());
    const float by = float(buttonRect.y());
    const float bw = float(buttonRect.width());
    const float bh = float(buttonRect.height());
    const float evalBx = seamLeft ? bx - 4.0f : bx;
    const float evalBw = bw + (seamLeft ? 4.0f : 0.0f) + (seamRight ? 4.0f : 0.0f);
    const float aaBand = 0.65f / scale;
    const float epsilon = 0.45f;

    for (int py = 0; py < pixelSize.height(); ++py) {
        QRgb *scanline = reinterpret_cast<QRgb *>(img.scanLine(py));
        const float y = float(aligned.y()) + (float(py) + 0.5f) * invScale;
        for (int px = 0; px < pixelSize.width(); ++px) {
            const float x = float(aligned.x()) + (float(px) + 0.5f) * invScale;
            if ((seamLeft && x < bx) || (seamRight && x >= bx + bw)) {
                continue;
            }
            const float d = Math::roundedBoxDistance(x, y, evalBx, by, evalBw, bh, radii);
            const float coverage = 1.0f - Math::smoothstep(-aaBand, aaBand, d);
            if (coverage <= 0.001f) {
                continue;
            }

            const float gx = Math::roundedBoxDistance(x + epsilon, y, evalBx, by, evalBw, bh, radii)
                           - Math::roundedBoxDistance(x - epsilon, y, evalBx, by, evalBw, bh, radii);
            const float gy = Math::roundedBoxDistance(x, y + epsilon, evalBx, by, evalBw, bh, radii)
                           - Math::roundedBoxDistance(x, y - epsilon, evalBx, by, evalBw, bh, radii);
            const float glen = std::max(std::sqrt(gx * gx + gy * gy), 0.0001f);
            const float dx = gx / glen;
            const float dy = gy / glen;

            const float cArg = (d + 0.65f * invScale) * scale / 0.48f;
            const float contour = std::exp(-cArg * cArg);

            if (stealthIdle) {
                Math::Vec3 stealth{0.06f, 0.08f, 0.09f};
                const float stealthTint = std::clamp(effectiveTintValue, 0.0f, 1.0f);
                stealth = Math::mix3(stealth, accent, Math::clampf(stealthTint * 0.18f + fillBoost * 0.45f, 0.0f, 0.65f));
                const Math::Vec3 rim = Math::mix3(lightingAccent, {1.0f, 1.0f, 1.0f}, 0.40f);
                stealth = Math::mix3(stealth, rim, contour * 0.42f);
                const float alpha = Math::clampf(0.16f + fillBoost * 0.28f + contour * 0.18f, 0.0f, 0.72f) * coverage;
                scanline[px] = Math::toPremultipliedRgb(stealth, alpha);
                continue;
            }

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
            const float baseTint = std::clamp(effectiveTintValue, 0.0f, 1.0f);
            const float tintMul = baseTint * 0.65f;
            color.x *= Math::mixf(1.0f, accent.x * 0.8f + 0.2f, tintMul);
            color.y *= Math::mixf(1.0f, accent.y * 0.8f + 0.2f, tintMul);
            color.z *= Math::mixf(1.0f, accent.z * 0.8f + 0.2f, tintMul);
            color = Math::mix3(color, {accent.x * 0.45f, accent.y * 0.45f, accent.z * 0.45f}, baseTint * 0.20f);
            color = Math::boostTint(color, accent, effectiveTintValue);
            if (fillBoost > 0.001f) {
                const float depth = Math::smoothstep(0.0f, 4.0f, std::max(-d, 0.0f));
                color = Math::mix3(color, {accent.x * 0.65f, accent.y * 0.65f, accent.z * 0.65f}, fillBoost * depth);
            }

            const float ndoth = std::max(nx * hx + ny * hy + nz * hz, 0.0f);
            const float specular = std::pow(ndoth, 40.0f) * cfg.specular;
            const float fresnel = std::pow(1.0f - nz, 3.0f) * cfg.specular;
            const float directionalEdge = dx * dirLightX + dy * dirLightY;

            const float edgeBoost = 1.0f + directionalEdge * bevel * 0.28f * cfg.specular;
            color.x *= edgeBoost;
            color.y *= edgeBoost;
            color.z *= edgeBoost;

            const Math::Vec3 specTint = Math::mix3({1.0f, 1.0f, 1.0f}, lightingAccent, Math::mixf(0.18f, 0.65f, restrainedLight));
            const float specAdd = specular * Math::mixf(0.65f, 0.30f, restrainedLight)
                + fresnel * Math::mixf(0.25f, 0.12f, restrainedLight);
            color.x += specTint.x * specAdd;
            color.y += specTint.y * specAdd;
            color.z += specTint.z * specAdd;

            const Math::Vec3 contourColor = Math::mix3(lightingAccent, {1.0f, 1.0f, 1.0f},
                std::max(directionalEdge, 0.0f) * Math::mixf(0.7f, 0.25f, restrainedLight));
            color = Math::mix3(color, contourColor, contour * Math::mixf(0.82f, 0.60f, restrainedLight));

            const float lArg = (d + 1.65f * invScale) * scale / 0.65f;
            float darkLip = std::exp(-lArg * lArg);
            if (seamLeft && x - bx < 3.0f) {
                darkLip *= Math::smoothstep(0.0f, 3.0f, x - bx);
            }
            if (seamRight && (bx + bw) - x < 3.0f) {
                darkLip *= Math::smoothstep(0.0f, 3.0f, (bx + bw) - x);
            }
            if (topDocked && y - by < 3.0f) {
                darkLip *= Math::smoothstep(0.0f, 3.0f, y - by);
            }
            const float lipMul = 1.0f - darkLip * 0.24f;
            color.x *= lipMul;
            color.y *= lipMul;
            color.z *= lipMul;

            // Keep button reflection/hover lighting in sync with buttonMaterial().
            const float relY = (y - by) / std::max(bh, 1.0f);
            const float pressDim = pressed ? 0.55f : 0.68f;
            color.x *= pressDim;
            color.y *= pressDim;
            color.z *= pressDim;

            const Math::Vec3 topHighlight = Math::mix3(lightingAccent, {1.0f, 1.0f, 1.0f}, Math::mixf(0.75f, 0.25f, restrainedLight));
            const float topFactor = (1.0f - Math::smoothstep(0.05f, 0.5f, relY))
                * (pressed ? 0.13f : Math::mixf(0.34f, 0.20f, restrainedLight));
            color.x += topHighlight.x * topFactor + accent.x * 0.11f;
            color.y += topHighlight.y * topFactor + accent.y * 0.11f;
            color.z += topHighlight.z * topFactor + accent.z * 0.11f;

            if (seamLeft) {
                const float seamDist = (x - bx) * scale;
                const float divider = std::exp(-(seamDist / 0.9f) * (seamDist / 0.9f));
                const Math::Vec3 dividerColor = Math::mix3(lightingAccent, {1.0f, 1.0f, 1.0f}, 0.55f);
                color = Math::mix3(color, dividerColor, divider * 0.35f);
            }
            if (seamRight) {
                const float seamDist = ((bx + bw) - x) * scale;
                const float divider = std::exp(-(seamDist / 0.9f) * (seamDist / 0.9f));
                color.x *= 1.0f - divider * 0.38f;
                color.y *= 1.0f - divider * 0.38f;
                color.z *= 1.0f - divider * 0.38f;
            }

            if (hovered) {
                const float distPtr = std::hypot(x - float(pointer.x()), y - float(pointer.y()));
                const float glow = std::exp(-distPtr / 24.0f) * Math::mixf(0.55f, 0.22f, restrainedLight) * cfg.hoverGlow;
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
