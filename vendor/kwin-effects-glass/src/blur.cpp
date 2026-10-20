/*
    SPDX-FileCopyrightText: 2010 Fredrik Höglund <fredrik@kde.org>
    SPDX-FileCopyrightText: 2011 Philipp Knechtges <philipp-dev@knechtges.com>
    SPDX-FileCopyrightText: 2018 Alex Nemeth <alex.nemeth329@gmail.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "blur.h"
#include "../../../native/blurprofile.h"
// KConfigSkeleton
#include "blurconfig.h"
#include "settings.h"

#include "core/pixelgrid.h"
#ifndef GLASS_X11
#include "core/region.h"
#endif
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glplatform.h"
#include "scene/decorationitem.h"
#include "scene/scene.h"
#include "scene/surfaceitem.h"
#include "scene/windowitem.h"
#if defined(GLASS_X11) || !defined(GLASS_KWIN_67)
#include "wayland/blur.h"
#include "wayland/contrast.h"
#endif
#include "wayland/display.h"
#include "wayland/surface.h"
#include "window.h"

#if PLASMA_VERSION >= 0x060404 && !defined(GLASS_X11)
#include <scene/backgroundeffectitem.h>
#endif

#if KWIN_BUILD_X11
#include "utils/xcbutils.h"
#endif

#include <QGuiApplication>
#include <QMatrix4x4>
#include <QScreen>
#include <QTime>
#include <QTimer>
#include <QWindow>
#include <algorithm>
#include <cmath> // for ceil()
#include <cstdlib>

#include <KConfigGroup>
#include <KSharedConfig>

#include <KDecoration3/Decoration>

Q_LOGGING_CATEGORY(KWIN_BLUR, "kwin_effect_blur", QtWarningMsg)

static void ensureResources()
{
    // Must initialize resources manually because the effect is a static lib.
    Q_INIT_RESOURCE(blur);
}

namespace KWin
{

static QMatrix4x4 colorTransformMatrix(qreal saturation, qreal contrast, qreal brightness)
{
    QMatrix4x4 saturationMatrix;
    QMatrix4x4 contrastMatrix;
    QMatrix4x4 brightnessMatrix;

    if (!qFuzzyCompare(saturation, 1.0)) {
        const qreal rval = (1.0 - saturation) * 0.2126;
        const qreal gval = (1.0 - saturation) * 0.7152;
        const qreal bval = (1.0 - saturation) * 0.0722;

        saturationMatrix = QMatrix4x4(rval + saturation, rval, rval, 0.0,
                                      gval, gval + saturation, gval, 0.0,
                                      bval, bval, bval + saturation, 0.0,
                                      0.0, 0.0, 0.0, 1.0);
    }

    if (!qFuzzyCompare(contrast, 1.0)) {
        const float transl = (1.0 - contrast) / 2.0;

        contrastMatrix = QMatrix4x4(contrast, 0.0, 0.0, 0.0,
                                    0.0, contrast, 0.0, 0.0,
                                    0.0, 0.0, contrast, 0.0,
                                    transl, transl, transl, 1.0);
    }

    if (!qFuzzyCompare(brightness, 1.0)) {
        brightnessMatrix.scale(brightness, brightness, brightness);
    }

    return contrastMatrix * saturationMatrix * brightnessMatrix;
}

BlurEffect::BlurEffect()
{
    BlurConfig::instance(effects->config());
    ensureResources();

    // Build this before shader loading so later reconfigure calls cannot index
    // an empty table if a shader fails and the constructor returns early.
    initBlurStrengthValues();

    m_roundedOnscreenPass.shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                                     QStringLiteral(":/effects/glass/generated/onscreen_rounded.vert"),
                                                                                     QStringLiteral(":/effects/glass/generated/onscreen_rounded.frag"));
    if (!m_roundedOnscreenPass.shader) {
        qCWarning(KWIN_BLUR) << "Failed to load onscreen pass shader";
        return;
    } else {
        m_roundedOnscreenPass.mvpMatrixLocation = m_roundedOnscreenPass.shader->uniformLocation("modelViewProjectionMatrix");
        m_roundedOnscreenPass.colorMatrixLocation = m_roundedOnscreenPass.shader->uniformLocation("colorMatrix");
        m_roundedOnscreenPass.useOklabSaturationLocation = m_roundedOnscreenPass.shader->uniformLocation("useOklabSaturation");
        m_roundedOnscreenPass.saturationLocation = m_roundedOnscreenPass.shader->uniformLocation("saturation");
        m_roundedOnscreenPass.offsetLocation = m_roundedOnscreenPass.shader->uniformLocation("offset");
        m_roundedOnscreenPass.halfpixelLocation = m_roundedOnscreenPass.shader->uniformLocation("halfpixel");
        m_roundedOnscreenPass.boxLocation = m_roundedOnscreenPass.shader->uniformLocation("box");
        m_roundedOnscreenPass.cornerRadiusLocation = m_roundedOnscreenPass.shader->uniformLocation("cornerRadius");
        m_roundedOnscreenPass.opacityLocation = m_roundedOnscreenPass.shader->uniformLocation("opacity");
        m_roundedOnscreenPass.texUnitLocation = m_roundedOnscreenPass.shader->uniformLocation("texUnit");
        m_roundedOnscreenPass.blurSizeLocation = m_roundedOnscreenPass.shader->uniformLocation("blurSize");
        m_roundedOnscreenPass.edgeSizePixelsLocation = m_roundedOnscreenPass.shader->uniformLocation("edgeSizePixels");
        m_roundedOnscreenPass.refractionStrengthLocation = m_roundedOnscreenPass.shader->uniformLocation("refractionStrength");
        m_roundedOnscreenPass.refractionNormalPowLocation = m_roundedOnscreenPass.shader->uniformLocation("refractionNormalPow");
        m_roundedOnscreenPass.refractionRGBFringingLocation = m_roundedOnscreenPass.shader->uniformLocation("refractionRGBFringing");
        m_roundedOnscreenPass.refractionOffsetStrengthLocation = m_roundedOnscreenPass.shader->uniformLocation("refractionOffsetStrength");
        m_roundedOnscreenPass.refractionBevelIntensityLocation = m_roundedOnscreenPass.shader->uniformLocation("refractionBevelIntensity");
        m_roundedOnscreenPass.physicallyBasedRefractionLocation = m_roundedOnscreenPass.shader->uniformLocation("physicallyBasedRefraction");
        m_roundedOnscreenPass.bodyRefractionLocation = m_roundedOnscreenPass.shader->uniformLocation("bodyRefraction");
        m_roundedOnscreenPass.bodyRefractionReachLocation = m_roundedOnscreenPass.shader->uniformLocation("bodyRefractionReach");
        m_roundedOnscreenPass.bodyRefractionStrengthLocation = m_roundedOnscreenPass.shader->uniformLocation("bodyRefractionStrength");
        m_roundedOnscreenPass.tintColorLocation = m_roundedOnscreenPass.shader->uniformLocation("tintColor");
        m_roundedOnscreenPass.tintGrayLocation = m_roundedOnscreenPass.shader->uniformLocation("tintGray");
        m_roundedOnscreenPass.tintStrengthLocation = m_roundedOnscreenPass.shader->uniformLocation("tintStrength");
        m_roundedOnscreenPass.autoTintAlphaRangeLocation = m_roundedOnscreenPass.shader->uniformLocation("autoTintAlphaRange");
        m_roundedOnscreenPass.autoTintAlphaLocation = m_roundedOnscreenPass.shader->uniformLocation("autoTintAlpha");
        m_roundedOnscreenPass.glowColorLocation = m_roundedOnscreenPass.shader->uniformLocation("glowColor");
        m_roundedOnscreenPass.glowStrengthLocation = m_roundedOnscreenPass.shader->uniformLocation("glowStrength");
        m_roundedOnscreenPass.edgeLightingLocation = m_roundedOnscreenPass.shader->uniformLocation("edgeLighting");
        m_roundedOnscreenPass.rimGlowLocation = m_roundedOnscreenPass.shader->uniformLocation("rimGlow");
        m_roundedOnscreenPass.glowOffsetLocation = m_roundedOnscreenPass.shader->uniformLocation("glowOffset");
        m_roundedOnscreenPass.rimGlowColorMixLocation = m_roundedOnscreenPass.shader->uniformLocation("rimGlowColorMix");
        m_roundedOnscreenPass.rimSpecularLocation = m_roundedOnscreenPass.shader->uniformLocation("rimSpecular");
        m_roundedOnscreenPass.rimSpecularScaleLocation = m_roundedOnscreenPass.shader->uniformLocation("rimSpecularScale");
        m_roundedOnscreenPass.rimWidthLocation = m_roundedOnscreenPass.shader->uniformLocation("rimWidth");
        m_roundedOnscreenPass.rimAdaptToRefractionLocation = m_roundedOnscreenPass.shader->uniformLocation("rimAdaptToRefraction");
        m_roundedOnscreenPass.rimAdaptMultiplierLocation = m_roundedOnscreenPass.shader->uniformLocation("rimAdaptMultiplier");
    }

    m_downsamplePass.shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                                QStringLiteral(":/effects/glass/generated/vertex.vert"),
                                                                                QStringLiteral(":/effects/glass/generated/downsample.frag"));
    if (!m_downsamplePass.shader) {
        qCWarning(KWIN_BLUR) << "Failed to load downsampling pass shader";
        return;
    } else {
        m_downsamplePass.mvpMatrixLocation = m_downsamplePass.shader->uniformLocation("modelViewProjectionMatrix");
        m_downsamplePass.offsetLocation = m_downsamplePass.shader->uniformLocation("offset");
        m_downsamplePass.halfpixelLocation = m_downsamplePass.shader->uniformLocation("halfpixel");
    }

    m_upsamplePass.shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                              QStringLiteral(":/effects/glass/generated/vertex.vert"),
                                                                              QStringLiteral(":/effects/glass/generated/upsample.frag"));
    if (!m_upsamplePass.shader) {
        qCWarning(KWIN_BLUR) << "Failed to load upsampling pass shader";
        return;
    } else {
        m_upsamplePass.mvpMatrixLocation = m_upsamplePass.shader->uniformLocation("modelViewProjectionMatrix");
        m_upsamplePass.offsetLocation = m_upsamplePass.shader->uniformLocation("offset");
        m_upsamplePass.halfpixelLocation = m_upsamplePass.shader->uniformLocation("halfpixel");
        m_upsamplePass.saturationCompensationLocation = m_upsamplePass.shader->uniformLocation("saturationCompensation");
    }

    m_noisePass.shader = ShaderManager::instance()->generateShaderFromFile(ShaderTrait::MapTexture,
                                                                           QStringLiteral(":/effects/glass/generated/vertex.vert"),
                                                                           QStringLiteral(":/effects/glass/generated/noise.frag"));
    if (!m_noisePass.shader) {
        qCWarning(KWIN_BLUR) << "Failed to load noise pass shader";
        return;
    } else {
        m_noisePass.mvpMatrixLocation = m_noisePass.shader->uniformLocation("modelViewProjectionMatrix");
        m_noisePass.noiseTextureSizeLocation = m_noisePass.shader->uniformLocation("noiseTextureSize");
    }

    reconfigure(ReconfigureAll);

    connect(effects, &EffectsHandler::windowAdded, this, &BlurEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &BlurEffect::slotWindowDeleted);
#ifdef GLASS_X11
    connect(effects, &EffectsHandler::screenRemoved, this, &BlurEffect::slotOutputRemoved);
#else
    connect(effects, &EffectsHandler::viewRemoved, this, &BlurEffect::slotOutputRemoved);
#endif
    // Fetch the blur regions for all windows
    const auto stackingOrder = effects->stackingOrder();
    for (EffectWindow *window : stackingOrder) {
        slotWindowAdded(window);
    }

    m_valid = true;
}

BlurEffect::~BlurEffect()
{
    for (EffectWindow *window : effects->stackingOrder()) {
        if (auto *decoration = window->decoration()) {
            decoration->removeEventFilter(this);
            disconnect(decoration, nullptr, this, nullptr);
            if (decoration->property("egEnabled").toBool()) {
                decoration->setProperty("egShader", false);
                decoration->update();
            }
        }
    }
}

void BlurEffect::initBlurStrengthValues()
{
    // This function creates an array of blur strength values that are evenly distributed

    // The range of the slider on the blur settings UI
    int numOfBlurSteps = 15;
    int remainingSteps = numOfBlurSteps;

    /*
     * Explanation for these numbers:
     *
     * The texture blur amount depends on the downsampling iterations and the offset value.
     * By changing the offset we can alter the blur amount without relying on further downsampling.
     * But there is a minimum and maximum value of offset per downsample iteration before we
     * get artifacts.
     *
     * The minOffset variable is the minimum offset value for an iteration before we
     * get blocky artifacts because of the downsampling.
     *
     * The maxOffset value is the maximum offset value for an iteration before we
     * get diagonal line artifacts because of the nature of the dual kawase blur algorithm.
     *
     * The expandSize value is the minimum value for an iteration before we reach the end
     * of a texture in the shader and sample outside of the area that was copied into the
     * texture from the screen.
     */

    // {minOffset, maxOffset, expandSize}
    blurOffsets.append({1.0, 2.0, 10}); // Down sample size / 2
    blurOffsets.append({2.0, 3.0, 20}); // Down sample size / 4
    blurOffsets.append({2.0, 5.0, 50}); // Down sample size / 8
    blurOffsets.append({3.0, 8.0, 150}); // Down sample size / 16
    // blurOffsets.append({5.0, 10.0, 400}); // Down sample size / 32
    // blurOffsets.append({7.0, ?.0});       // Down sample size / 64

    float offsetSum = 0;

    for (int i = 0; i < blurOffsets.size(); i++) {
        offsetSum += blurOffsets[i].maxOffset - blurOffsets[i].minOffset;
    }

    for (int i = 0; i < blurOffsets.size(); i++) {
        int iterationNumber = std::ceil((blurOffsets[i].maxOffset - blurOffsets[i].minOffset) / offsetSum * numOfBlurSteps);
        remainingSteps -= iterationNumber;

        if (remainingSteps < 0) {
            iterationNumber += remainingSteps;
        }

        float offsetDifference = blurOffsets[i].maxOffset - blurOffsets[i].minOffset;

        for (int j = 1; j <= iterationNumber; j++) {
            // {iteration, offset}
            blurStrengthValues.append({i + 1, blurOffsets[i].minOffset + (offsetDifference / iterationNumber) * j});
        }
    }
}

void BlurEffect::reconfigure(ReconfigureFlags flags)
{
    if (const auto config = BlurConfig::self()->config()) {
        config->reparseConfiguration();
    }
    m_settings.read();

    m_contentBlurSettings = pipelineSettingsForStrength(
        m_settings.general.blurStrength,
        m_settings.general.noiseStrength
    );
    const auto materialConfig = KSharedConfig::openConfig(QStringLiteral("emeraldglassrc"));
    materialConfig->reparseConfiguration();
    const KConfigGroup material(materialConfig, QStringLiteral("Material"));
    m_frameBlurStrength = std::clamp(material.readEntry("BlurStrength", 6), 0, 15);
    const auto kwinConfig = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
    kwinConfig->reparseConfiguration();
    const KConfigGroup standardBlur(kwinConfig, QStringLiteral("Effect-blur"));
    m_frameBackdropSaturation = std::max(0, standardBlur.readEntry("Saturation", 100)) / 100.0f;
    const auto frameBlur = EmeraldGlass::blurProfile(m_frameBlurStrength);
    m_decorationBlurSettings = BlurPipelineSettings{size_t(frameBlur.iterations), frameBlur.offset, frameBlur.expandSize, 0};
    m_dockBlurSettings = pipelineSettingsForStrength(
        m_settings.general.dockBlurStrength,
        m_settings.general.dockNoiseStrength
    );
    // Only our frame is rendered here. Unused client/dock settings must not
    // increase texture allocation or damage expansion during a resize.
    m_maxIterationCount = std::max<size_t>(1, m_decorationBlurSettings.iterationCount);
    m_expandSize = m_decorationBlurSettings.expandSize;
    m_blurRadius = m_settings.general.blurRadius;
    m_upsampleOffset = m_settings.general.upsampleOffset;
    m_dockBlurRadius = m_settings.general.dockBlurRadius;
    m_dockUpsampleOffset = m_settings.general.dockUpsampleOffset;

    // If oklab saturation is enabled, the matrix should have a 
    // saturation value of 1.0 since the saturation is handled by the shader.
    const qreal matrixSaturation = m_settings.general.oklabSaturation ? 1.0 : m_settings.general.saturation;
    m_colorMatrix = colorTransformMatrix(
        matrixSaturation,
        m_settings.general.contrast,
        m_settings.general.brightness
    );
#if PLASMA_VERSION >= 0x060404 && !defined(GLASS_X11)
    for (auto &[window, data] : m_windows) {
        data.blurItem->setPixelsToExpandRepaintsBelowOpaqueRegions(m_expandSize);
    }
#endif

    if (m_valid) {
        const auto stackingOrder = effects->stackingOrder();
        for (EffectWindow *window : stackingOrder) {
            updateBlurRegion(window);
        }
    }

    // Update all windows for the blur to take effect
    effects->addRepaintFull();
}

void BlurEffect::repaintDynamicCorners()
{
    if (m_settings.roundedCorners.dynamicCorners) {
        effects->addRepaintFull();
    }
}

BlurEffect::BlurPipelineSettings BlurEffect::pipelineSettingsForStrength(int blurStrength, int noiseStrength) const
{
    if (blurStrengthValues.isEmpty() || blurOffsets.isEmpty() ||
        blurStrength < 0 || blurStrength >= blurStrengthValues.size()) {
        return BlurPipelineSettings{
            .iterationCount = 1,
            .offset = 1.0f,
            .expandSize = 10,
            .noiseStrength = noiseStrength,
        };
    }

    const BlurValuesStruct &values = blurStrengthValues[blurStrength];

    return BlurPipelineSettings{
        .iterationCount = static_cast<size_t>(values.iteration),
        .offset = values.offset,
        .expandSize = blurOffsets[values.iteration - 1].expandSize,
        .noiseStrength = noiseStrength,
    };
}

void BlurEffect::updateBlurRegion(EffectWindow *w)
{
    auto *decoration = w->decoration();
    BlurRegion frame;
    if (decoration && decoration->property("egEnabled").toBool()) {
        // This region belongs to our material, independently of the region
        // advertised to standard Blur by KDecoration.
        frame = BlurRegion(decoration->property("egFrameRegion").value<QRegion>());
#ifdef GLASS_X11
        frame &= BlurRegion(decoration->rect().toAlignedRect()) - w->contentsRect().toAlignedRect();
#else
        frame &= BlurRegion(Rect(decoration->rect().toAlignedRect())) - Rect(w->contentsRect().toAlignedRect());
#endif
    }

    const auto previous = m_windows.find(w);
    const bool handoff = previous == m_windows.end() ? !frame.isEmpty() : frame.isEmpty();
    const bool changed = previous == m_windows.end() ? !frame.isEmpty() : previous->second.frame != std::optional<BlurRegion>(frame);
    if (!frame.isEmpty()) {
        BlurEffectData &data = m_windows[w];
        data.content.reset();
        data.frame = frame;
        data.colorMatrix = QMatrix4x4();
#if PLASMA_VERSION < 0x060404 || defined(GLASS_X11)
        data.windowEffect = ItemEffect(w->windowItem());
#else
        if (!data.blurItem) {
            data.blurItem = std::make_unique<BackgroundEffectItem>(w->windowItem());
        }
        data.blurItem->setPixelsToExpandRepaintsBelowOpaqueRegions(m_expandSize);
        data.blurItem->setEffectBoundingRect(frame.boundingRect());
#endif
    } else if (auto it = m_windows.find(w); it != m_windows.end()) {
        effects->makeOpenGLContextCurrent();
        m_windows.erase(it);
    }

    if (decoration && decoration->property("egEnabled").isValid()) {
        decoration->setProperty("egShader", !frame.isEmpty());
    }
    if (changed) {
        ++m_regionChanges;
        // Handoff can change the background dependency without resizing the window.
        if (handoff) effects->addRepaintFull();
    }
}

void BlurEffect::slotWindowAdded(EffectWindow *w)
{
    SurfaceInterface *surf = w->surface();

    if (surf) {
        windowBlurChangedConnections[w] = connect(surf, &SurfaceInterface::blurChanged, this, [this, w]() {
            if (w) {
                updateBlurRegion(w);
            }
        });
#if !defined(GLASS_X11) && !defined(GLASS_KWIN_67)
        windowContrastChangedConnections[w] = connect(surf, &SurfaceInterface::contrastChanged, this, [this, w]() {
            if (w) {
                updateBlurRegion(w);
            }
        });
#endif
    }

    windowFrameGeometryChangedConnections[w] = connect(w, &EffectWindow::windowFrameGeometryChanged, this, [this,w]() {
        if (w) {
            updateBlurRegion(w);
            repaintDynamicCorners();
        }
    });
    windowClientGeometryChangedConnections[w] = connect(w->window(), &Window::clientGeometryChanged, this, [this, w]() {
        updateBlurRegion(w);
    });

    if (auto internal = w->internalWindow()) {
        internal->installEventFilter(this);
    }

    setupDecorationConnections(w);
    connect(w, &EffectWindow::windowDecorationChanged, this, [this, w]() {
        setupDecorationConnections(w);
        updateBlurRegion(w);
        // Wayland installs the decoration before committing client geometry.
        // Re-read after commit even when the outer frame stayed unchanged.
        QTimer::singleShot(0, this, [this, window = QPointer<EffectWindow>(w)] {
            if (window) {
                updateBlurRegion(window);
                effects->addRepaintFull();
            }
        });
    });

    updateBlurRegion(w);
    repaintDynamicCorners();
}

void BlurEffect::slotWindowDeleted(EffectWindow *w)
{
    disconnect(m_decorationBlurConnections.take(w));
    if (auto decoration = m_decorations.take(w)) decoration->removeEventFilter(this);
    disconnect(windowClientGeometryChangedConnections.take(w));
    if (auto it = m_windows.find(w); it != m_windows.end()) {
        effects->makeOpenGLContextCurrent();
        m_windows.erase(it);
    }
    if (auto it = windowBlurChangedConnections.find(w); it != windowBlurChangedConnections.end()) {
        disconnect(*it);
        windowBlurChangedConnections.erase(it);
    }
#if !defined(GLASS_X11) && !defined(GLASS_KWIN_67)
    if (auto it = windowContrastChangedConnections.find(w); it != windowContrastChangedConnections.end()) {
        disconnect(*it);
        windowContrastChangedConnections.erase(it);
    }
#endif
    if (auto it = windowFrameGeometryChangedConnections.find(w); it != windowFrameGeometryChangedConnections.end()) {
        disconnect(*it);
        windowFrameGeometryChangedConnections.erase(it);
    }
    repaintDynamicCorners();
}

void BlurEffect::slotOutputRemoved(KWin::BlurOutput *output)
{
    for (auto &[window, data] : m_windows) {
        if (auto it = data.render.find(output); it != data.render.end()) {
            effects->makeOpenGLContextCurrent();
            data.render.erase(it);
        }
    }
}

#if KWIN_BUILD_X11
void BlurEffect::slotPropertyNotify(EffectWindow *w, long atom)
{
    if (w && atom == net_wm_blur_region && net_wm_blur_region != XCB_ATOM_NONE) {
        updateBlurRegion(w);
    }
}
#endif

void BlurEffect::setupDecorationConnections(EffectWindow *w)
{
    disconnect(m_decorationBlurConnections.take(w));
    if (auto previous = m_decorations.take(w)) previous->removeEventFilter(this);
    if (!w->decoration()) {
        return;
    }

    w->decoration()->installEventFilter(this);
    m_decorations[w] = w->decoration();
    m_decorationBlurConnections[w] = connect(w->decoration(), &KDecoration3::Decoration::blurRegionChanged, this, [this, w]() {
        updateBlurRegion(w);
    });
}

bool BlurEffect::eventFilter(QObject *watched, QEvent *event)
{
    if (qobject_cast<KDecoration3::Decoration *>(watched) && event->type() == QEvent::DynamicPropertyChange) {
        const auto name = static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName();
        if (name == "egEnabled" || name == "egFrameRegion") {
            for (auto *w : effects->stackingOrder()) {
                if (w->decoration() == watched) {
                    updateBlurRegion(w);
                    break;
                }
            }
        }
    }
    auto internal = qobject_cast<QWindow *>(watched);
    if (internal && event->type() == QEvent::DynamicPropertyChange) {
        QDynamicPropertyChangeEvent *pe = static_cast<QDynamicPropertyChangeEvent *>(event);
        if (pe->propertyName() == "kwin_blur") {
            if (auto w = effects->findWindow(internal)) {
                updateBlurRegion(w);
            }
        }
    }
    return false;
}

bool BlurEffect::enabledByDefault()
{
    const auto context = effects->openglContext();
    if (!context || context->isSoftwareRenderer()) {
        return false;
    }
    GLPlatform *gl = context->glPlatform();

    if (gl->isIntel() && gl->chipClass() < SandyBridge) {
        return false;
    }
    if (gl->isPanfrost() && gl->chipClass() <= MaliT8XX) {
        return false;
    }
    // The blur effect works, but is painfully slow (FPS < 5) on Mali and VideoCore
    if (gl->isLima() || gl->isVideoCore4() || gl->isVideoCore3D()) {
        return false;
    }
    return true;
}

bool BlurEffect::supported()
{
    return effects->isOpenGLCompositing();
}

bool BlurEffect::decorationSupportsBlurBehind(const EffectWindow *w) const
{
    return w->decoration() && !w->decoration()->blurRegion().isNull();
}

BorderRadius BlurEffect::effectiveWindowCornerRadius(EffectWindow *w, const BorderRadius &declaredCornerRadius, bool *isOverRounded, bool applyDynamicCorners) const
{
    if (isOverRounded) {
        *isOverRounded = false;
    }

    if (!w) {
        return BorderRadius(0.0, 0.0, 0.0, 0.0);
    }

    if ((w->decoration() && w->decoration()->property("egEnabled").toBool()) ||
        m_settings.roundedCorners.useDeclaredCornerRadius) {
        return declaredCornerRadius;
    }

    const bool roundWindowCorners = !w->isFullScreen() &&
        (m_settings.roundedCorners.roundMaximized || w->window()->maximizeMode() != MaximizeFull);

    float topCornerRadius = 0.0;
    float bottomCornerRadius = 0.0;
    if (w->isOnScreenDisplay() || w->isTooltip()) {
        topCornerRadius = m_settings.roundedCorners.windowTopRadius;
        bottomCornerRadius = m_settings.roundedCorners.windowBottomRadius;
    } else if (w->isDock()) {
        topCornerRadius = m_settings.roundedCorners.dockRadius;
        bottomCornerRadius = m_settings.roundedCorners.dockRadius;
    } else if (w->isMenu() || w->isDropdownMenu() || w->isPopupMenu() || w->isPopupWindow()) {
        topCornerRadius = m_settings.roundedCorners.menuRadius;
        bottomCornerRadius = m_settings.roundedCorners.menuRadius;
    } else if (roundWindowCorners) {
        topCornerRadius = m_settings.roundedCorners.windowTopRadius;
        bottomCornerRadius = m_settings.roundedCorners.windowBottomRadius;
    }

    if (!roundWindowCorners) {
        return BorderRadius(0.0, 0.0, 0.0, 0.0);
    }

    if (topCornerRadius <= 0.0f && bottomCornerRadius <= 0.0f) {
        return BorderRadius(0.0, 0.0, 0.0, 0.0);
    }

    const QRectF frame = w->frameGeometry();
    const float winWidth = frame.width();
    const float winHeight = frame.height();
    const bool overRounded = (topCornerRadius + bottomCornerRadius) > winHeight ||
        (topCornerRadius * 2) > winWidth;

    if (isOverRounded) {
        *isOverRounded = overRounded;
    }

    if (overRounded) {
        if (w->isDock()) {
            topCornerRadius = 0;
            bottomCornerRadius = 0;
        } else {
            const float minRadius = std::min(winWidth, winHeight) / 2.0;
            topCornerRadius = minRadius;
            bottomCornerRadius = minRadius;
        }
    }

    return BorderRadius(
        applyDynamicCorners && shouldFlattenCorner(w, Qt::TopLeftCorner) ? 0.0f : topCornerRadius,
        applyDynamicCorners && shouldFlattenCorner(w, Qt::TopRightCorner) ? 0.0f : topCornerRadius,
        applyDynamicCorners && shouldFlattenCorner(w, Qt::BottomRightCorner) ? 0.0f : bottomCornerRadius,
        applyDynamicCorners && shouldFlattenCorner(w, Qt::BottomLeftCorner) ? 0.0f : bottomCornerRadius
    );
}

BlurRegion BlurEffect::roundedContentRegion(const QRect &rect, const BorderRadius &cornerRadius, qreal leftSideWidth, qreal rightSideWidth, qreal topHeight, qreal bottomHeight) const
{
    const QVector4D radius = cornerRadius.toVector();
    auto contentRadius = [](float windowRadius, qreal sideWidth) {
        if (windowRadius <= 0.0f) {
            return 0.0f;
        }
        return std::max(windowRadius * 0.5f, windowRadius - static_cast<float>(sideWidth));
    };

    const int maxRadius = std::max(0, std::min(rect.width(), rect.height()) / 2);
    const int topLeft = leftSideWidth || topHeight ? std::clamp(static_cast<int>(std::round(contentRadius(radius.x(), leftSideWidth))), 0, maxRadius) : 0;
    const int topRight = rightSideWidth || topHeight ? std::clamp(static_cast<int>(std::round(contentRadius(radius.y(), rightSideWidth))), 0, maxRadius) : 0;
    const int bottomLeft = leftSideWidth || bottomHeight ? std::clamp(static_cast<int>(std::round(contentRadius(radius.z(), leftSideWidth))), 0, maxRadius) : 0;
    const int bottomRight = rightSideWidth || bottomHeight ? std::clamp(static_cast<int>(std::round(contentRadius(radius.w(), rightSideWidth))), 0, maxRadius) : 0;

    if (topLeft == 0 && topRight == 0 && bottomRight == 0 && bottomLeft == 0) {
#ifdef GLASS_X11
        return BlurRegion(rect);
#else
        return Region(Rect(rect));
#endif
    }

    auto insetForRadius = [](int radius, double distanceFromEdge) {
        if (radius <= 0 || distanceFromEdge >= radius) {
            return 0;
        }

        const double clampedDistance = std::max(0.0, distanceFromEdge);
        const double y = radius - clampedDistance;
        return static_cast<int>(std::ceil(radius - std::sqrt(std::max(0.0, radius * radius - y * y))));
    };

    BlurRegion region;
    auto addRect = [&region](const QRect &rect) {
#ifdef GLASS_X11
        region += rect;
#else
        region += Rect(rect);
#endif
    };

    int spanY = -1;
    int spanX = 0;
    int spanWidth = 0;
    for (int y = 0; y < rect.height(); ++y) {
        const double distanceFromTop = y + 0.5;
        const double distanceFromBottom = rect.height() - y - 0.5;
        const int leftInset = std::max(insetForRadius(topLeft, distanceFromTop),
                                       insetForRadius(bottomLeft, distanceFromBottom));
        const int rightInset = std::max(insetForRadius(topRight, distanceFromTop),
                                        insetForRadius(bottomRight, distanceFromBottom));
        const int rowWidth = rect.width() - leftInset - rightInset;
        if (rowWidth <= 0) {
            if (spanY >= 0) {
                addRect(QRect(spanX, rect.top() + spanY, spanWidth, y - spanY));
                spanY = -1;
            }
            continue;
        }

        const int rowX = rect.left() + leftInset;
        if (spanY >= 0 && rowX == spanX && rowWidth == spanWidth) {
            continue;
        }

        if (spanY >= 0) {
            addRect(QRect(spanX, rect.top() + spanY, spanWidth, y - spanY));
        }
        spanY = y;
        spanX = rowX;
        spanWidth = rowWidth;
    }

    if (spanY >= 0) {
        addRect(QRect(spanX, rect.top() + spanY, spanWidth, rect.height() - spanY));
    }

    return region;
}

BlurRegion BlurEffect::decorationBlurRegion(const EffectWindow *w) const
{
    if (!decorationSupportsBlurBehind(w)) {
        return BlurRegion();
    }

#ifdef GLASS_X11
    BlurRegion decorationRegion = BlurRegion(w->decoration()->rect().toAlignedRect()) - w->contentsRect().toRect();
#else
    BlurRegion decorationRegion = BlurRegion(Rect(w->decoration()->rect().toAlignedRect())) - w->contentsRect().toRect();
#endif
    //! we return only blurred regions that belong to decoration region
    return decorationRegion.intersected(BlurRegion(w->decoration()->blurRegion()));
}

BlurRegion BlurEffect::contentRegion(EffectWindow *w, const BorderRadius *fallbackCornerRadius) const
{
    // Client content is exclusively handled by standard KWin Blur.
    if (w->decoration() && w->decoration()->property("egEnabled").toBool()) {
        return {};
    }
    BlurRegion region;

    if (auto it = m_windows.find(w); it != m_windows.end()) {
        const std::optional<BlurRegion> &content = it->second.content;
        if (!m_settings.roundedCorners.ignoreContentBlurRegion || w->isDock()) {
            if (content.has_value()) {
                if (content->isEmpty()) {
#ifdef GLASS_X11
                    region = w->contentsRect().toAlignedRect();
#else
                    region = Rect(w->contentsRect().toAlignedRect());
#endif
                } else {
                    region = content->translated(
                            w->contentsRect().x(),
                            w->contentsRect().y()) & w->contentsRect().toAlignedRect();
                }
            }
        } else {
            const BorderRadius declaredCornerRadius = it->second.originalCornerRadius.value_or(w->window()->borderRadius());
            const BorderRadius cornerRadius = fallbackCornerRadius
                ? *fallbackCornerRadius
                : effectiveWindowCornerRadius(w, declaredCornerRadius, nullptr, false);
            const QRectF contentsRect = w->contentsRect();
            const qreal leftSideWidth = std::max<qreal>(0.0, contentsRect.x());
            const qreal rightSideWidth = std::max<qreal>(0.0, w->frameGeometry().width() - contentsRect.x() - contentsRect.width());
            const qreal topHeight = std::max<qreal>(0.0, contentsRect.y());
            const qreal bottomHeight = std::max<qreal>(0.0, w->frameGeometry().height() - contentsRect.y() - contentsRect.height());
            region = roundedContentRegion(w->contentsRect().toRect(),
                                          cornerRadius,
                                          leftSideWidth,
                                          rightSideWidth,
                                          topHeight,
                                          bottomHeight);
        }

    }

    return region;
}

BlurRegion BlurEffect::blurRegion(EffectWindow *w, const BorderRadius *fallbackCornerRadius) const
{
    BlurRegion region = contentRegion(w, fallbackCornerRadius);

    if (auto it = m_windows.find(w); it != m_windows.end()) {
        const std::optional<BlurRegion> &frame = it->second.frame;
        if (frame.has_value()) {
            region += frame.value();
        }
    }

    return region;
}

QRectF BlurEffect::dynamicCornerRect(EffectWindow *w) const
{
    if (w->isDock()) {
        const BlurRegion region = blurRegion(w);
        if (!region.isEmpty()) {
            return QRectF(region.boundingRect()).translated(w->pos());
        }
    }

    return w->frameGeometry();
}

void BlurEffect::updateDecorationMode(EffectWindow *w, int mask)
{
    if (auto *decoration = w->decoration(); decoration && decoration->property("egEnabled").isValid()) {
        // Deformation effects snapshot the native decoration. Keep its full
        // material in that snapshot rather than painting an undeformed screen rectangle.
        const bool transformed = (mask & PAINT_WINDOW_TRANSFORMED) || effects->activeFullScreenEffect();
        decoration->setProperty("egTransformed", transformed);
    }
}

#ifdef GLASS_KWIN_67
void BlurEffect::prePaintScreen(ScreenPrePaintData &data)
#else
void BlurEffect::prePaintScreen(ScreenPrePaintData &data, std::chrono::milliseconds presentTime)
#endif
{
    m_paintedDeviceArea = BlurRegion();
    m_currentDeviceBlur = BlurRegion();
#ifdef GLASS_X11
    m_currentOutput = effects->waylandDisplay() ? data.screen : nullptr;
#else
    m_currentOutput = data.view;
#endif

#ifdef GLASS_KWIN_67
    effects->prePaintScreen(data);
#else
    effects->prePaintScreen(data, presentTime);
#endif
}

#ifdef GLASS_X11
#ifdef GLASS_KWIN_67
void BlurEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data)
#else
void BlurEffect::prePaintWindow(EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
#endif
{
    // this effect relies on prePaintWindow being called in the bottom to top order
#ifdef GLASS_KWIN_67
    effects->prePaintWindow(w, data);
#else
    effects->prePaintWindow(w, data, presentTime);
#endif
    updateDecorationMode(w, data.mask);

    const QRegion oldOpaque = data.opaque;
    if (data.opaque.intersects(m_currentDeviceBlur)) {
        QRegion newOpaque;
        for (const QRect &rect : data.opaque) {
            newOpaque += rect.adjusted(m_expandSize, m_expandSize, -m_expandSize, -m_expandSize);
        }
        data.opaque = newOpaque;
        m_currentDeviceBlur -= newOpaque;
    }

    if ((data.paint - oldOpaque).intersects(m_currentDeviceBlur)) {
        data.paint += m_currentDeviceBlur;
    }

    const QRegion blurArea = blurRegion(w).boundingRect().translated(w->pos().toPoint());
    if (m_paintedDeviceArea.intersects(blurArea) || data.paint.intersects(blurArea)) {
        data.paint += blurArea;
        if (blurArea.intersects(m_currentDeviceBlur)) {
            data.paint += m_currentDeviceBlur;
        }
    }

    m_currentDeviceBlur += blurArea;
    m_paintedDeviceArea -= data.opaque;
    m_paintedDeviceArea += data.paint;
}
#else
#ifdef GLASS_KWIN_67
void BlurEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    effects->prePaintWindow(view, w, data);
    updateDecorationMode(w, data.mask);
    if (!blurRegion(w).isEmpty()) {
        data.setTranslucent();
    }
}
#else
void BlurEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data, std::chrono::milliseconds presentTime)
{
    effects->prePaintWindow(view, w, data, presentTime);
    updateDecorationMode(w, data.mask);

    const Region blurArea = view->mapToDeviceCoordinatesAligned(
        QRectF(blurRegion(w).boundingRect()).translated(w->pos())
    );

    if (!blurArea.isEmpty()) {
        data.deviceOpaque -= blurArea;

        Region expandedBlur = blurArea;
        for (const Rect &rect : blurArea.rects()) {
            expandedBlur += rect.adjusted(-m_expandSize, -m_expandSize, m_expandSize, m_expandSize);
        }

        data.devicePaint += (expandedBlur - data.deviceOpaque);
    }

    if (m_paintedDeviceArea.intersects(blurArea) || data.devicePaint.intersects(blurArea)) {
        data.devicePaint += blurArea;
        if (blurArea.intersects(m_currentDeviceBlur)) {
            data.devicePaint += m_currentDeviceBlur;
        }
    }

    m_currentDeviceBlur += blurArea;
    m_paintedDeviceArea -= data.deviceOpaque;
    m_paintedDeviceArea += data.devicePaint;
}
#endif
#endif

bool BlurEffect::shouldBlur(const EffectWindow *w, int mask, const WindowPaintData &data) const
{
    if (!w->decoration() || !w->decoration()->property("egEnabled").toBool() ||
        w->decoration()->property("egTransformed").toBool()) {
        return false;
    }

    if (effects->activeFullScreenEffect() && !w->data(WindowForceBlurRole).toBool()) {
        return false;
    }

    if (w->isDesktop()) {
        return false;
    }

    const auto windowClass = w->window()->resourceClass();

    // special condition for spectacle
    if (windowClass.contains("spectacle")) {
        const KWin::Layer layer = w->window()->layer();
        if (layer == KWin::Layer::OverlayLayer || layer == KWin::Layer::ActiveLayer) {
            return false;
        }
    }

    bool scaled = !qFuzzyCompare(data.xScale(), 1.0) && !qFuzzyCompare(data.yScale(), 1.0);
    bool translated = data.xTranslation() || data.yTranslation();

    if ((scaled || (translated || (mask & PAINT_WINDOW_TRANSFORMED))) && !w->data(WindowForceBlurRole).toBool()) {
        return false;
    }

    return true;
}

DrawWindowResult BlurEffect::drawWindow(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const BlurRegion &deviceRegion, WindowPaintData &data)
{
    blur(renderTarget, viewport, w, mask, deviceRegion, data);

    // Draw the window over the blurred area
    return effects->drawWindow(renderTarget, viewport, w, mask, deviceRegion, data);
}

GLTexture *BlurEffect::ensureNoiseTexture(int noiseStrength)
{
    if (noiseStrength == 0) {
        return nullptr;
    }

    const qreal scale = std::max(1.0, QGuiApplication::primaryScreen()->logicalDotsPerInch() / 96.0);
    if (!m_noisePass.noiseTexture || m_noisePass.noiseTextureScale != scale || m_noisePass.noiseTextureStength != noiseStrength) {
        // Init randomness based on time
        std::srand((uint)QTime::currentTime().msec());

        QImage noiseImage(QSize(256, 256), QImage::Format_Grayscale8);

        for (int y = 0; y < noiseImage.height(); y++) {
            uint8_t *noiseImageLine = (uint8_t *)noiseImage.scanLine(y);

            for (int x = 0; x < noiseImage.width(); x++) {
                noiseImageLine[x] = std::rand() % noiseStrength;
            }
        }

        noiseImage = noiseImage.scaled(noiseImage.size() * scale);

        m_noisePass.noiseTexture = GLTexture::upload(noiseImage);
        if (!m_noisePass.noiseTexture) {
            return nullptr;
        }
        m_noisePass.noiseTexture->setFilter(GL_NEAREST);
        m_noisePass.noiseTexture->setWrapMode(GL_REPEAT);
        m_noisePass.noiseTextureScale = scale;
        m_noisePass.noiseTextureStength = noiseStrength;
    }

    return m_noisePass.noiseTexture.get();
}

void BlurEffect::blur(const RenderTarget &renderTarget, const RenderViewport &viewport, EffectWindow *w, int mask, const BlurRegion &deviceRegion, WindowPaintData &data)
{
    auto it = m_windows.find(w);
    if (it == m_windows.end()) {
        return;
    }

    BlurEffectData &blurInfo = it->second;
    BlurRenderData &renderInfo = blurInfo.render[m_currentOutput];
    if (!shouldBlur(w, mask, data)) {
        return;
    }

    auto transformShape = [&](BlurRegion shape) {
        shape.translate(w->pos().toPoint());
        if (data.xScale() != 1 || data.yScale() != 1) {
            QPoint pt = shape.boundingRect().topLeft();
            BlurRegion scaledShape;
#ifdef GLASS_X11
            for (const QRect &r : shape) {
#else
            for (const Rect &r : shape.rects()) {
#endif
                const QPointF topLeft(pt.x() + (r.x() - pt.x()) * data.xScale() + data.xTranslation(),
                                      pt.y() + (r.y() - pt.y()) * data.yScale() + data.yTranslation());
                const QPoint bottomRight(std::floor(topLeft.x() + r.width() * data.xScale()) - 1,
                                         std::floor(topLeft.y() + r.height() * data.yScale()) - 1);
                scaledShape += QRect(QPoint(std::floor(topLeft.x()), std::floor(topLeft.y())), bottomRight);
            }
            return scaledShape;
        }
        if (data.xTranslation() || data.yTranslation()) {
            shape.translate(std::round(data.xTranslation()), std::round(data.yTranslation()));
        }
        return shape;
    };

    BorderRadius cornerRadius = w->window()->borderRadius();
    if (!blurInfo.originalCornerRadius.has_value()) {
        blurInfo.originalCornerRadius = cornerRadius;
    }
    bool isOverRounded = false;

    cornerRadius = effectiveWindowCornerRadius(w, cornerRadius, &isOverRounded);

    const BlurRegion effectShape = transformShape(blurRegion(w, &cornerRadius));
    const BlurRegion contentShape = transformShape(contentRegion(w, &cornerRadius));
    const BlurRegion frameShape = effectShape - contentShape;
    const BlurPipelineSettings &contentBlurSettings = w->isDock() ? m_dockBlurSettings : m_contentBlurSettings;
    const float blurRadius = w->isDock() ? m_dockBlurRadius : m_blurRadius;
    const float upsampleOffset = w->isDock() ? m_dockUpsampleOffset : m_upsampleOffset;
    const BlurPipelineSettings &combinedBlurSettings =
        (contentShape.isEmpty() && !frameShape.isEmpty()) ? m_decorationBlurSettings : contentBlurSettings;
    const bool splitBlurSettings = !frameShape.isEmpty() &&
        !contentShape.isEmpty();
    const bool splitTintSettings = m_settings.general.excludeDecorations &&
        !frameShape.isEmpty() &&
        !contentShape.isEmpty();
    const bool splitRenderRegions = splitBlurSettings || splitTintSettings;
    const QRect backgroundRect = effectShape.boundingRect();
#ifdef GLASS_X11
    const QRect scaledBackgroundRect = snapToPixelGrid(scaledRect(backgroundRect, viewport.scale()));
    const QRect deviceBackgroundRect = scaledBackgroundRect;
#else
    const QRect scaledBackgroundRect = RectF(backgroundRect).scaled(viewport.scale()).rounded();
    const QRect deviceBackgroundRect = viewport.mapToDeviceCoordinates(Rect(backgroundRect)).rounded();
#endif
    const auto opacity = w->opacity() * data.opacity();

    // Get the effective shape that will be painted on screen. It's possible that all of it will be clipped.
    auto buildEffectiveShape = [&](const BlurRegion &shape) {
#ifdef GLASS_X11
        QList<QRectF> effectiveShape;
        effectiveShape.reserve(shape.rectCount());
        if (deviceRegion != infiniteRegion()) {
            for (const QRect &clipRect : deviceRegion) {
                const QRectF deviceClipRect = snapToPixelGridF(scaledRect(clipRect, viewport.scale()))
                                                  .translated(-deviceBackgroundRect.topLeft());
                for (const QRect &shapeRect : shape) {
                    const QRectF deviceShapeRect = snapToPixelGridF(scaledRect(shapeRect.translated(-backgroundRect.topLeft()), viewport.scale()));
                    if (const QRectF intersected = deviceClipRect.intersected(deviceShapeRect); !intersected.isEmpty()) {
                        effectiveShape.append(intersected);
                    }
                }
            }
        } else {
            for (const QRect &rect : shape) {
                effectiveShape.append(snapToPixelGridF(scaledRect(rect.translated(-backgroundRect.topLeft()), viewport.scale())));
            }
        }
        return effectiveShape;
#else
        QList<RectF> effectiveShape;
        effectiveShape.reserve(shape.rects().size());
        if (deviceRegion != Region::infinite()) {
            for (const Rect &clipRect : deviceRegion.rects()) {
                const RectF deviceClipRect = clipRect.translated(-deviceBackgroundRect.topLeft());
                for (const Rect &shapeRect : shape.rects()) {
                    const RectF deviceShapeRect = shapeRect.translated(-backgroundRect.topLeft()).scaled(viewport.scale()).rounded();
                    if (const QRectF intersected = deviceClipRect.intersected(deviceShapeRect); !intersected.isEmpty()) {
                        effectiveShape.append(intersected);
                    }
                }
            }
        } else {
            for (const Rect &rect : shape.rects()) {
                effectiveShape.append(rect.translated(-backgroundRect.topLeft()).scaled(viewport.scale()).rounded());
            }
        }
        return effectiveShape;
#endif
    };

    const auto effectiveEffectShape = buildEffectiveShape(effectShape);
    const auto effectiveContentShape = splitRenderRegions ? buildEffectiveShape(contentShape) : effectiveEffectShape;
    const auto effectiveFrameShape = splitRenderRegions ? buildEffectiveShape(frameShape) : decltype(effectiveEffectShape){};

    if (effectiveEffectShape.isEmpty()) {
        return;
    }

    // Maybe reallocate offscreen render targets. Keep in mind that the first one contains
    // original background behind the window, it's not blurred.
    GLenum textureFormat = GL_RGBA8;
    if (renderTarget.texture()) {
        textureFormat = renderTarget.texture()->internalFormat();
    }

    if (renderInfo.framebuffers.size() != (m_maxIterationCount + 1) || renderInfo.textures[0]->size() != backgroundRect.size() || renderInfo.textures[0]->internalFormat() != textureFormat) {
        renderInfo.framebuffers.clear();
        renderInfo.textures.clear();

        glClearColor(0, 0, 0, 0);
        for (size_t i = 0; i <= m_maxIterationCount; ++i) {
            auto texture = GLTexture::allocate(textureFormat, (backgroundRect.size() / (1 << i)).expandedTo(QSize(1, 1)));
            if (!texture) {
                qCWarning(KWIN_BLUR) << "Failed to allocate an offscreen texture";
                return;
            }
            texture->setFilter(GL_LINEAR);
            texture->setWrapMode(GL_CLAMP_TO_EDGE);

            auto framebuffer = std::make_unique<GLFramebuffer>(texture.get());
            if (!framebuffer->valid()) {
                qCWarning(KWIN_BLUR) << "Failed to create an offscreen framebuffer";
                return;
            }
#ifdef GLASS_X11
            GLFramebuffer::pushFramebuffer(framebuffer.get());
            glClear(GL_COLOR_BUFFER_BIT);
            GLFramebuffer::popFramebuffer();
#else
            EglContext::currentContext()->pushFramebuffer(framebuffer.get());
            glClear(GL_COLOR_BUFFER_BIT);
            EglContext::currentContext()->popFramebuffer();
#endif
            renderInfo.textures.push_back(std::move(texture));
            renderInfo.framebuffers.push_back(std::move(framebuffer));
        }
        // Allocation clears the cache. Schedule a complete scene repaint so
        // partial damage during a theme switch cannot leave it half-filled.
        ++m_cacheInitializations;
        if (renderInfo.needsInitialRepaint) {
            renderInfo.needsInitialRepaint = false;
            effects->addRepaintFull();
        }
    }

    // Fetch the pixels behind the shape that is going to be blurred.
#ifdef GLASS_X11
    const QRegion dirtyRegion = deviceRegion & backgroundRect;
    for (const QRect &dirtyRect : dirtyRegion) {
        renderInfo.framebuffers[0]->blitFromRenderTarget(renderTarget, viewport, dirtyRect, dirtyRect.translated(-backgroundRect.topLeft()));
    }
#else
    const Region dirtyRegion = viewport.mapFromDeviceCoordinatesContained(deviceRegion) & backgroundRect;
    for (const Rect &dirtyRect : dirtyRegion.rects()) {
        // Outside dirtyRegion the target may still contain our previous frame.
        // Keep the cached backdrop there instead of feeding our own glass/text back in.
        renderInfo.framebuffers[0]->blitFromRenderTarget(renderTarget, viewport, dirtyRect, dirtyRect.translated(-backgroundRect.topLeft()));
    }
#endif

    // Upload the geometry: the first 6 vertices are used when downsampling and upsampling offscreen,
    // the remaining vertices are used when rendering on the screen.
    GLVertexBuffer *vbo = GLVertexBuffer::streamingBuffer();
    vbo->reset();
    vbo->setAttribLayout(std::span(GLVertexBuffer::GLVertex2DLayout), sizeof(GLVertex2D));

    const int contentVertexCount = effectiveContentShape.size() * 6;
    const int frameVertexCount = effectiveFrameShape.size() * 6;
    const int vertexCount = splitRenderRegions ? (contentVertexCount + frameVertexCount) : contentVertexCount;
    if (auto result = vbo->map<GLVertex2D>(6 + vertexCount)) {
        auto map = *result;

        size_t vboIndex = 0;

        // The geometry that will be blurred offscreen, in logical pixels.
        {
            const QRectF localRect = QRectF(0, 0, backgroundRect.width(), backgroundRect.height());

            const float x0 = localRect.left();
            const float y0 = localRect.top();
            const float x1 = localRect.right();
            const float y1 = localRect.bottom();

            const float u0 = x0 / backgroundRect.width();
            const float v0 = 1.0f - y0 / backgroundRect.height();
            const float u1 = x1 / backgroundRect.width();
            const float v1 = 1.0f - y1 / backgroundRect.height();

            // first triangle
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x0, y0),
                .texcoord = QVector2D(u0, v0),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x1, y1),
                .texcoord = QVector2D(u1, v1),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x0, y1),
                .texcoord = QVector2D(u0, v1),
            };

            // second triangle
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x0, y0),
                .texcoord = QVector2D(u0, v0),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x1, y0),
                .texcoord = QVector2D(u1, v0),
            };
            map[vboIndex++] = GLVertex2D{
                .position = QVector2D(x1, y1),
                .texcoord = QVector2D(u1, v1),
            };
        }

        auto appendScreenGeometry = [&](const auto &shapeRects) {
            for (const auto &rect : shapeRects) {
                const float x0 = rect.left();
                const float y0 = rect.top();
                const float x1 = rect.right();
                const float y1 = rect.bottom();

                const float u0 = x0 / scaledBackgroundRect.width();
                const float v0 = 1.0f - y0 / scaledBackgroundRect.height();
                const float u1 = x1 / scaledBackgroundRect.width();
                const float v1 = 1.0f - y1 / scaledBackgroundRect.height();

                map[vboIndex++] = GLVertex2D{
                    .position = QVector2D(x0, y0),
                    .texcoord = QVector2D(u0, v0),
                };
                map[vboIndex++] = GLVertex2D{
                    .position = QVector2D(x1, y1),
                    .texcoord = QVector2D(u1, v1),
                };
                map[vboIndex++] = GLVertex2D{
                    .position = QVector2D(x0, y1),
                    .texcoord = QVector2D(u0, v1),
                };

                map[vboIndex++] = GLVertex2D{
                    .position = QVector2D(x0, y0),
                    .texcoord = QVector2D(u0, v0),
                };
                map[vboIndex++] = GLVertex2D{
                    .position = QVector2D(x1, y0),
                    .texcoord = QVector2D(u1, v0),
                };
                map[vboIndex++] = GLVertex2D{
                    .position = QVector2D(x1, y1),
                    .texcoord = QVector2D(u1, v1),
                };
            }
        };

        appendScreenGeometry(effectiveContentShape);
        if (splitBlurSettings) {
            appendScreenGeometry(effectiveFrameShape);
        }

        vbo->unmap();
    } else {
        qCWarning(KWIN_BLUR) << "Failed to map vertex buffer";
        return;
    }

    vbo->bindArrays();

    auto runBlurPass = [&](const BlurPipelineSettings &settings) -> GLTexture * {
        if (m_frameBlurStrength == 0) {
            return renderInfo.textures[0].get();
        }
        glDisable(GL_BLEND);
        ShaderManager::instance()->pushShader(m_downsamplePass.shader.get());

        QMatrix4x4 projectionMatrix;
        projectionMatrix.ortho(QRectF(0.0, 0.0, backgroundRect.width(), backgroundRect.height()));

        m_downsamplePass.shader->setUniform(m_downsamplePass.mvpMatrixLocation, projectionMatrix);
        m_downsamplePass.shader->setUniform(m_downsamplePass.offsetLocation, settings.offset * blurRadius);

        for (size_t i = 1; i <= settings.iterationCount; ++i) {
            const auto &read = renderInfo.framebuffers[i - 1];
            const auto &draw = renderInfo.framebuffers[i];

            const QVector2D halfpixel(0.5 / read->colorAttachment()->width(),
                                      0.5 / read->colorAttachment()->height());
            m_downsamplePass.shader->setUniform(m_downsamplePass.halfpixelLocation, halfpixel);

            glActiveTexture(GL_TEXTURE0);
            read->colorAttachment()->bind();

            GLFramebuffer::pushFramebuffer(draw.get());
            vbo->draw(GL_TRIANGLES, 0, 6);
        }

        ShaderManager::instance()->popShader();

        ShaderManager::instance()->pushShader(m_upsamplePass.shader.get());

        m_upsamplePass.shader->setUniform(m_upsamplePass.mvpMatrixLocation, projectionMatrix);
        m_upsamplePass.shader->setUniform(m_upsamplePass.offsetLocation, settings.offset * upsampleOffset);

        for (size_t i = settings.iterationCount; i > 1; --i) {
            GLFramebuffer::popFramebuffer();
            const auto &read = renderInfo.framebuffers[i];

            const QVector2D halfpixel(0.5 / read->colorAttachment()->width(),
                                      0.5 / read->colorAttachment()->height());
            m_upsamplePass.shader->setUniform(m_upsamplePass.halfpixelLocation, halfpixel);
            m_upsamplePass.shader->setUniform(m_upsamplePass.saturationCompensationLocation, 1.0f);

            glActiveTexture(GL_TEXTURE0);
            read->colorAttachment()->bind();

            vbo->draw(GL_TRIANGLES, 0, 6);
        }

        ShaderManager::instance()->popShader();
        GLFramebuffer::popFramebuffer();

        return renderInfo.framebuffers[1]->colorAttachment();
    };

    const QMatrix4x4 &colorMatrix = m_colorMatrix;
    const float modulation = opacity * opacity;

    w->window()->setBorderRadius(cornerRadius);


    ShaderManager::instance()->pushShader(m_roundedOnscreenPass.shader.get());

    QMatrix4x4 projectionMatrix = viewport.projectionMatrix();
    projectionMatrix.translate(scaledBackgroundRect.x(), scaledBackgroundRect.y());

    const QVector2D halfpixel(0.5 / renderInfo.framebuffers[1]->colorAttachment()->width(),
                              0.5 / renderInfo.framebuffers[1]->colorAttachment()->height());

    const QRectF transformedRect = QRectF{
        w->frameGeometry().x() + data.xTranslation(),
        w->frameGeometry().y() + data.yTranslation(),
        w->frameGeometry().width() * data.xScale(),
        w->frameGeometry().height() * data.yScale(),
    };
#ifdef GLASS_X11
    const QRectF nativeBox = snapToPixelGridF(scaledRect(transformedRect, viewport.scale()))
                                 .translated(-scaledBackgroundRect.topLeft());
#else
    const RectF nativeBox = RectF(transformedRect)
                                .scaled(viewport.scale())
                                .rounded()
                                .translated(-scaledBackgroundRect.topLeft());
#endif
    const BorderRadius nativeCornerRadius = cornerRadius.scaled(viewport.scale()).rounded();
    const bool isRefractionExcluded =
        (m_settings.refraction.excludeDocks && w->isDock()) ||
        (m_settings.refraction.excludeTooltips && w->isTooltip()) ||
        (m_settings.refraction.excludeOSD && (w->isNotification() || w->isOnScreenDisplay())) ||
        (m_settings.refraction.excludeMenus && !w->isTooltip() &&
            (w->isMenu() || w->isDropdownMenu() || w->isPopupMenu() || w->isPopupWindow())) ||
        (m_settings.refraction.excludeWindows &&
            !w->isDock() &&
            !w->isTooltip() &&
            !w->isNotification() &&
            !w->isOnScreenDisplay() &&
            !w->isMenu() &&
            !w->isDropdownMenu() &&
            !w->isPopupMenu() &&
            !w->isPopupWindow());

    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.mvpMatrixLocation, projectionMatrix);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.colorMatrixLocation, colorMatrix);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.useOklabSaturationLocation, m_settings.general.oklabSaturation ? 1 : 0);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.saturationLocation, static_cast<float>(m_settings.general.saturation));
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.halfpixelLocation, halfpixel);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.offsetLocation, combinedBlurSettings.offset * upsampleOffset);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.boxLocation, QVector4D(nativeBox.x() + nativeBox.width() * 0.5, nativeBox.y() + nativeBox.height() * 0.5, nativeBox.width() * 0.5, nativeBox.height() * 0.5));
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.cornerRadiusLocation, nativeCornerRadius.toVector());
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.opacityLocation, modulation);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.texUnitLocation, 0);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.blurSizeLocation, QVector2D(nativeBox.width(), nativeBox.height()));
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.edgeSizePixelsLocation, m_settings.refraction.edgeSizePixels);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.refractionStrengthLocation, isRefractionExcluded ? 0.0f : m_settings.refraction.refractionStrength);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.refractionNormalPowLocation, m_settings.refraction.refractionNormalPow);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.refractionRGBFringingLocation, m_settings.refraction.refractionRGBFringing);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.refractionOffsetStrengthLocation, m_settings.refraction.refractionOffsetStrength);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.refractionBevelIntensityLocation, m_settings.refraction.refractionBevelIntensity);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.physicallyBasedRefractionLocation, m_settings.refraction.physicallyBased ? 1 : 0);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.bodyRefractionLocation, m_settings.refraction.bodyRefraction ? 1 : 0);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.bodyRefractionReachLocation, m_settings.refraction.bodyRefractionReach);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.bodyRefractionStrengthLocation, m_settings.refraction.bodyRefractionStrength);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.rimGlowLocation, m_settings.general.rimGlow ? 1 : 0);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.glowOffsetLocation, m_settings.general.glowOffset);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.rimGlowColorMixLocation, m_settings.general.rimGlowColorMix ? 1 : 0);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.rimSpecularLocation, m_settings.general.rimSpecular ? 1 : 0);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.rimSpecularScaleLocation, m_settings.general.rimSpecularScale);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.rimWidthLocation, m_settings.general.rimWidth);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.rimAdaptToRefractionLocation, m_settings.general.rimAdaptToRefraction ? 1 : 0);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.rimAdaptMultiplierLocation, m_settings.general.rimAdaptMultiplier);

    QColor tint(m_settings.general.tintColor);
    QVector3D tintVec(tint.redF(), tint.greenF(), tint.blueF());
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.tintColorLocation, tintVec);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.tintGrayLocation, static_cast<float>(0.299 * tint.redF() + 0.587 * tint.greenF() + 0.114 * tint.blueF()));
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.autoTintAlphaRangeLocation, QVector2D(m_settings.general.autoTintAlphaMin, m_settings.general.autoTintAlphaMax));
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.autoTintAlphaLocation, m_settings.general.autoTintAlpha ? 1 : 0);
    auto tintStrengthForRegion = [&](bool decorationRegion) {
        if (w->isDock() && m_settings.general.excludeDocks) {
            return 0.0f;
        }
        if (w->isTooltip() && m_settings.general.excludeTooltips) {
            return 0.0f;
        }
        if ((w->isNotification() || w->isOnScreenDisplay()) && m_settings.general.excludeOSD) {
            return 0.0f;
        }
        if (m_settings.general.excludeMenus && !w->isTooltip() &&
                (w->isMenu() || w->isDropdownMenu() || w->isPopupMenu() || w->isPopupWindow())
           ) {
            return 0.0f;
        }
        if (decorationRegion && m_settings.general.excludeDecorations) {
            return 0.0f;
        }
        return static_cast<float>(tint.alphaF());
    };

    QColor glow(m_settings.general.glowColor);
    QVector3D glowVec(glow.redF(), glow.greenF(), glow.blueF());
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.glowColorLocation, glowVec);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.glowStrengthLocation, static_cast<float>(glow.alphaF()));
    const bool edgeLightingExcluded = (isOverRounded && w->isDock()) ||
        (m_settings.general.edgeLightingDock && w->isDock()) ||
        (m_settings.general.edgeLightingTooltip && w->isTooltip());
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.edgeLightingLocation, edgeLightingExcluded ? false : m_settings.general.edgeLighting);


    auto drawBlurredRegion = [&](GLTexture *blurredTexture, int vertexOffset, int currentVertexCount, float blurOffset) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.offsetLocation, blurOffset * m_upsampleOffset);
        glActiveTexture(GL_TEXTURE0);
        blurredTexture->bind();
        vbo->draw(GL_TRIANGLES, vertexOffset, currentVertexCount);
    };

    auto drawNoiseRegion = [&](int noiseStrength, int vertexOffset, int currentVertexCount) {
        if (noiseStrength <= 0 || currentVertexCount == 0) {
            return;
        }

        if (GLTexture *noiseTexture = ensureNoiseTexture(noiseStrength)) {
            ShaderManager::instance()->pushShader(m_noisePass.shader.get());

            QMatrix4x4 noiseProjectionMatrix = viewport.projectionMatrix();
            noiseProjectionMatrix.translate(scaledBackgroundRect.x(), scaledBackgroundRect.y());

            m_noisePass.shader->setUniform(m_noisePass.mvpMatrixLocation, noiseProjectionMatrix);
            m_noisePass.shader->setUniform(m_noisePass.noiseTextureSizeLocation, QVector2D(noiseTexture->width(), noiseTexture->height()));

            glActiveTexture(GL_TEXTURE0);
            noiseTexture->bind();
            vbo->draw(GL_TRIANGLES, vertexOffset, currentVertexCount);

            ShaderManager::instance()->popShader();
        }
    };

    const float contentTintStrength = tintStrengthForRegion(contentShape.isEmpty() && !frameShape.isEmpty());
    const float frameTintStrength = tintStrengthForRegion(true);

    // The native decoration owns geometry/input; this effect renders its material
    // from the untouched scene below the window, using the preview's same GLSL.
    auto *emeraldDecoration = w->decoration();
    const bool emerald = emeraldDecoration && emeraldDecoration->property("egEnabled").toBool();
    if (emeraldDecoration && emeraldDecoration->property("egShader").toBool() != emerald) {
        emeraldDecoration->setProperty("egShader", emerald);
    }
    auto *materialShader = m_roundedOnscreenPass.shader.get();
    materialShader->setUniform("emeraldEnabled", 0);
    if (emerald) {
        const float dpr = viewport.scale();
        const QRectF materialFrame(nativeBox.x() / dpr, nativeBox.y() / dpr,
                                   nativeBox.width() / dpr, nativeBox.height() / dpr);
        const qreal decW = std::max(1.0, emeraldDecoration->rect().width());
        const qreal decH = std::max(1.0, emeraldDecoration->rect().height());
        const qreal sx = materialFrame.width() / decW;
        const qreal sy = materialFrame.height() / decH;
        auto localRect = [&](QRectF rect) {
            return QVector4D(materialFrame.x() + rect.x() * sx, materialFrame.y() + rect.y() * sy,
                             rect.width() * sx, rect.height() * sy);
        };
        materialShader->setUniform("kwinMode", 1);
        materialShader->setUniform("opticsDisabled", emeraldDecoration->property("egOptics").toBool() ? 0 : 1);
        materialShader->setUniform("backdropSaturationOffset", m_frameBackdropSaturation - 1.0f);
        materialShader->setUniform("scene", 1);
        materialShader->setUniform("resolution", QVector2D(backgroundRect.width(), backgroundRect.height()));
        materialShader->setUniform("frameRect", QVector4D(materialFrame.x(), materialFrame.y(), materialFrame.width(), materialFrame.height()));
        materialShader->setUniform("contentRect", localRect(emeraldDecoration->property("egContent").toRectF()));

        const auto buttons = emeraldDecoration->property("egButtons").toList();
        const int btnCount = std::min<int>(buttons.size(), 12);
        materialShader->setUniform("buttonCount", btnCount);
        materialShader->setUniform("buttonCloseIndex", emeraldDecoration->property("egCloseButton").toInt());
        for (int i = 0; i < btnCount; ++i) {
            const QByteArray name = QByteArray("buttonRect[") + QByteArray::number(i) + ']';
            materialShader->setUniform(name.constData(), localRect(buttons.value(i).toRectF()));
        }

        const QColor accent = emeraldDecoration->property("egColor").value<QColor>();
        materialShader->setUniform("accent", QVector3D(accent.redF(), accent.greenF(), accent.blueF()));
        const QPointF cursor = effects->cursorPos() - backgroundRect.topLeft();
        materialShader->setUniform("pointer", QVector2D(cursor));
        materialShader->setUniform("refraction", emeraldDecoration->property("egRefraction").toFloat());
        materialShader->setUniform("tint", emeraldDecoration->property("egTint").toFloat());
        materialShader->setUniform("frameOpacity", emeraldDecoration->property("egFrameOpacity").toFloat());
        materialShader->setUniform("backgroundBlurOffset", m_frameBlurStrength > 0 ? m_decorationBlurSettings.offset * upsampleOffset : 0.0f);
        materialShader->setUniform("dispersion", emeraldDecoration->property("egDispersion").toFloat());
        materialShader->setUniform("specularStrength", emeraldDecoration->property("egSpecular").toFloat());
        materialShader->setUniform("hoverGlowStrength", emeraldDecoration->property("egHoverGlow").toFloat());
        materialShader->setUniform("adaptiveContrast", emeraldDecoration->property("egAdaptiveContrast").toFloat());
        materialShader->setUniform("frameRadius", emeraldDecoration->property("egCornerRadius").toFloat());
        materialShader->setUniform("titleHeight", emeraldDecoration->property("egTitleHeight").toFloat());
        materialShader->setUniform("scale", dpr);
        materialShader->setUniform("hoverButton", emeraldDecoration->property("egHover").toInt());
        materialShader->setUniform("pressedButton", emeraldDecoration->property("egPressed").toInt());
    }

    GLTexture *contentBlurredTexture = runBlurPass(splitBlurSettings ? contentBlurSettings : combinedBlurSettings);
    if (emerald) {
        glActiveTexture(GL_TEXTURE1);
        contentBlurredTexture->bind();
        glActiveTexture(GL_TEXTURE0);
    }
    materialShader->setUniform("emeraldEnabled", emerald && contentShape.isEmpty() ? 1 : 0);
    m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.tintStrengthLocation, contentTintStrength);
    drawBlurredRegion(contentBlurredTexture,
                      6,
                      contentVertexCount,
                      splitBlurSettings ? contentBlurSettings.offset : combinedBlurSettings.offset);

    if (splitRenderRegions && frameVertexCount > 0) {
        GLTexture *frameBlurredTexture = splitBlurSettings ? runBlurPass(m_decorationBlurSettings) : contentBlurredTexture;
        materialShader->setUniform("emeraldEnabled", emerald ? 1 : 0);
        m_roundedOnscreenPass.shader->setUniform(m_roundedOnscreenPass.tintStrengthLocation, frameTintStrength);
        drawBlurredRegion(frameBlurredTexture,
                          6 + contentVertexCount,
                          frameVertexCount,
                          splitBlurSettings ? m_decorationBlurSettings.offset : combinedBlurSettings.offset);
    }

    materialShader->setUniform("emeraldEnabled", 0);

    glDisable(GL_BLEND);

    ShaderManager::instance()->popShader();

    if (combinedBlurSettings.noiseStrength > 0 || (splitRenderRegions && m_decorationBlurSettings.noiseStrength > 0)) {
        // Apply an additive noise onto the blurred image. The noise is useful to mask banding
        // artifacts, which often happens due to the smooth color transitions in the blurred image.

        glEnable(GL_BLEND);
        if (opacity < 1.0) {
            glBlendFunc(GL_CONSTANT_ALPHA, GL_ONE);
        } else {
            glBlendFunc(GL_ONE, GL_ONE);
        }

        drawNoiseRegion(splitBlurSettings ? contentBlurSettings.noiseStrength : combinedBlurSettings.noiseStrength,
                        6,
                        contentVertexCount);
        if (splitRenderRegions) {
            drawNoiseRegion(splitBlurSettings ? m_decorationBlurSettings.noiseStrength : combinedBlurSettings.noiseStrength,
                            6 + contentVertexCount,
                            frameVertexCount);
        }

        glDisable(GL_BLEND);
    }

    vbo->unbindArrays();
}

bool BlurEffect::isActive() const
{
    return m_valid && !effects->isScreenLocked();
}

QString BlurEffect::debug(const QString &parameter) const
{
    QString details;
    int owned = 0;
    int shaderFrames = 0;
    int fallbackFrames = 0;
    int transformedFrames = 0;
    int foreignTracked = 0;
    int clientOverlaps = 0;
    int rendered = 0;
    for (auto *w : effects->stackingOrder()) {
        const auto *decoration = w->decoration();
        const bool own = decoration && decoration->property("egFrameRegion").isValid();
        if (own) {
            if (parameter == QLatin1String("frames")) {
                QDebug stream(&details);
                stream.nospace() << "\nclass=" << w->window()->resourceClass()
                    << " frame=" << w->frameGeometry() << " client=" << w->contentsRect()
                    << " material=" << blurRegion(w).boundingRect()
                    << " declared=" << decoration->property("egFrameRegion").value<QRegion>().boundingRect();
            }
            ++owned;
            const bool transformed = decoration->property("egTransformed").toBool();
            shaderFrames += decoration->property("egShader").toBool() && !transformed;
            transformedFrames += transformed;
            fallbackFrames += !decoration->blurRegion().isEmpty();
        }
        if (const auto it = m_windows.find(w); it != m_windows.end()) {
            foreignTracked += !own;
#ifdef GLASS_X11
            clientOverlaps += !(blurRegion(w) & w->contentsRect().toAlignedRect()).isEmpty();
#else
            clientOverlaps += !(blurRegion(w) & Rect(w->contentsRect().toAlignedRect())).isEmpty();
#endif
            for (const auto &[output, render] : it->second.render) {
                rendered += !render.textures.empty();
            }
        }
    }
    return QStringLiteral("owned=%1 shaderFrames=%2 fallbackFrames=%3 foreignTracked=%4 clientOverlaps=%5 rendered=%6 blurStrength=%7 transformedFrames=%8 backdropSaturation=%9 regionChanges=%10 cacheInitializations=%11")
        .arg(owned).arg(shaderFrames).arg(fallbackFrames).arg(foreignTracked)
        .arg(clientOverlaps).arg(rendered).arg(m_frameBlurStrength).arg(transformedFrames).arg(m_frameBackdropSaturation)
        .arg(m_regionChanges).arg(m_cacheInitializations) + details;
}

bool BlurEffect::blocksDirectScanout() const
{
    return false;
}

bool BlurEffect::shouldFlattenCorner(KWin::EffectWindow *w, Qt::Corner corner) const {
    if (!w || !m_settings.roundedCorners.dynamicCorners) {
        return false;
    } else if (m_settings.roundedCorners.dynamicCornersExcludeDocks && w->isDock()) {
        return false;
    } else if (m_settings.roundedCorners.dynamicCornersExcludeTooltips && w->isTooltip()) {
        return false;
    } else if (
        m_settings.roundedCorners.dynamicCornersExcludeMenus &&
        !w->isTooltip() &&
        (w->isMenu() || w->isDropdownMenu() || w->isPopupMenu() || w->isPopupWindow())
    ) {
        return false;
    } else if (
        m_settings.roundedCorners.dynamicCornersExcludeWindows &&
        !w->isTooltip() &&
        !w->isMenu() &&
        !w->isDropdownMenu() &&
        !w->isPopupMenu() &&
        !w->isPopupWindow() &&
        !w->isDock()
    ) {
        return false;
    }

    const QRectF rect = dynamicCornerRect(w);
    const double margin = 1.0; // Tolerance in pixels

    QPointF cornerPos;
    bool isLeft = corner == Qt::TopLeftCorner || corner == Qt::BottomLeftCorner;
    bool isRight = corner == Qt::TopRightCorner || corner == Qt::BottomRightCorner;
    bool isTop = corner == Qt::TopLeftCorner || corner == Qt::TopRightCorner;
    bool isBottom = corner == Qt::BottomLeftCorner || corner == Qt::BottomRightCorner;

    switch (corner) {
        case Qt::TopLeftCorner:     cornerPos = rect.topLeft(); break;
        case Qt::TopRightCorner:    cornerPos = rect.topRight(); break;
        case Qt::BottomLeftCorner:  cornerPos = rect.bottomLeft(); break;
        case Qt::BottomRightCorner: cornerPos = rect.bottomRight(); break;
    }

    const QRectF screenRect = effects->clientArea(KWin::FullScreenArea, w);

    bool touchesDesktopLeft   = isLeft   && std::abs(cornerPos.x() - screenRect.left())   < margin;
    bool touchesDesktopRight  = isRight  && std::abs(cornerPos.x() - screenRect.right())  < margin;
    bool touchesDesktopTop    = isTop    && std::abs(cornerPos.y() - screenRect.top())    < margin;
    bool touchesDesktopBottom = isBottom && std::abs(cornerPos.y() - (screenRect.y() + screenRect.height())) < margin;

    if (touchesDesktopLeft ||
        touchesDesktopRight ||
        touchesDesktopTop ||
        touchesDesktopBottom) return true;

    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        KWin::EffectWindow *other = it->first;
        if (other == w ||
            other->isMinimized() ||
            !other->isManaged() ||
            !other->isOnCurrentDesktop() ||
            !other->isOnCurrentActivity()
        ) continue;

        const QRectF otherRect = dynamicCornerRect(other);

        bool onLeft   = isRight  && std::abs(cornerPos.x() - otherRect.left())   < margin;
        bool onRight  = isLeft   && std::abs(cornerPos.x() - otherRect.right())  < margin;
        bool onTop    = isBottom && std::abs(cornerPos.y() - otherRect.top())    < margin;
        bool onBottom = isTop    && std::abs(cornerPos.y() - otherRect.bottom()) < margin;

        if (onLeft || onRight) {
            if (cornerPos.y() >= (otherRect.top() - margin) && cornerPos.y() <= (otherRect.bottom() + margin)) {
                return true;
            }
        }

        if (onTop || onBottom) {
            if (cornerPos.x() >= (otherRect.left() - margin) && cornerPos.x() <= (otherRect.right() + margin)) {
                return true;
            }
        }
    }

    return false;
}

} // namespace KWin

#include "moc_blur.cpp"
