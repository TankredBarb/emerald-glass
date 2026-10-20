// SPDX-License-Identifier: LGPL-3.0-or-later
#include "material.h"
#include "materialpreview.h"

#include <KDecoration3/DecoratedWindow>
#include <KDecoration3/Decoration>
#include <KDecoration3/DecorationButton>
#include <KDecoration3/DecorationButtonGroup>
#include <KDecoration3/DecorationSettings>
#include <KDecoration3/DecorationShadow>
#include <KPluginFactory>
#include <QDynamicPropertyChangeEvent>
#include <QHoverEvent>
#include <QMouseEvent>
#include <QPaintDevice>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <memory>

namespace EmeraldGlass {

class Decoration;

class Button : public KDecoration3::DecorationButton {
    Q_OBJECT
public:
    Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent = nullptr);
    static KDecoration3::DecorationButton *create(KDecoration3::DecorationButtonType type,
                                                  KDecoration3::Decoration *decoration,
                                                  QObject *parent);
    void paint(QPainter *p, const QRectF &repaintArea) override;
private:
    struct MaterialKey {
        MaterialConfig config;
        QColor accent;
        QRectF rect;
        QPointF pointer;
        qreal dpr = 1;
        float tint = 0;
        bool hovered = false;
        bool pressed = false;
        bool operator==(const MaterialKey &) const = default;
    };
    MaterialKey m_materialKey;
    QImage m_materialImage;
};

class Decoration : public KDecoration3::Decoration {
    Q_OBJECT
public:
    Decoration(QObject *parent, const QVariantList &args)
        : KDecoration3::Decoration(parent, args)
    {
        const auto *bridge = args.isEmpty() ? nullptr : args.first().toMap().value(QStringLiteral("bridge")).value<QObject *>();
        m_isPreview = bridge && bridge->inherits("KDecoration3::Preview::PreviewBridge");
        setProperty("egPreview", m_isPreview);
    }

    const MaterialConfig &config() const { return m_cfg; }
    QColor currentAccentColor() const { return m_cfg.colorForWindow(window() && window()->isActive()); }
    float currentTintStrength() const { return m_cfg.tintForWindow(window() && window()->isActive()); }
    QPointF pointerPos() const { return m_pointer; }
    bool isUsingShaderMaterial() const
    {
        return m_cfg.shaderEffectEnabled &&
            ((m_isPreview && m_previewEffectEnabled && !m_previewImage.isNull()) ||
             (property("egShader").toBool() && !property("egTransformed").toBool()));
    }

    bool init() override
    {
        setOpaque(false);
        m_leftButtons = new KDecoration3::DecorationButtonGroup(this);
        m_rightButtons = new KDecoration3::DecorationButtonGroup(this);
        m_leftButtons->setSpacing(3.0);
        m_rightButtons->setSpacing(3.0);

        auto *w = window();
        connect(w, &KDecoration3::DecoratedWindow::sizeChanged, this, &Decoration::layout);
        connect(w, &KDecoration3::DecoratedWindow::maximizedChanged, this, &Decoration::layout);
        connect(w, &KDecoration3::DecoratedWindow::shadedChanged, this, &Decoration::layout);
        connect(w, &KDecoration3::DecoratedWindow::scaleChanged, this, [this] {
            invalidateCache();
            update();
        });
        connect(w, &KDecoration3::DecoratedWindow::activeChanged, this, [this] {
            publish();
        });
        connect(w, &KDecoration3::DecoratedWindow::captionChanged, this, [this] {
            update(titleBar());
        });
        connect(w, &KDecoration3::DecoratedWindow::iconChanged, this, [this] {
            update(titleBar());
        });
        connect(this, &KDecoration3::Decoration::bordersChanged, this, &Decoration::layoutButtons);

        auto *s = settings().get();
        connect(s, &KDecoration3::DecorationSettings::reconfigured, this, &Decoration::configure);
        connect(s, &KDecoration3::DecorationSettings::borderSizeChanged, this, &Decoration::layout);
        connect(s, &KDecoration3::DecorationSettings::fontChanged, this, [this] { update(titleBar()); });
        connect(s, &KDecoration3::DecorationSettings::decorationButtonsLeftChanged, this, &Decoration::rebuildButtons);
        connect(s, &KDecoration3::DecorationSettings::decorationButtonsRightChanged, this, &Decoration::rebuildButtons);

        configure();
        return true;
    }

    void paint(QPainter *p, const QRectF &repaintArea) override
    {
        const QRectF bounds = rect();
        if (bounds.isEmpty()) {
            return;
        }

        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);

        const qreal dpr = (p->device() && p->device()->devicePixelRatioF() > 0.0)
            ? p->device()->devicePixelRatioF()
            : (window() ? std::max(window()->scale(), 1.0) : 1.0);

        if (m_isPreview) {
            ensurePreviewFrame(bounds.size(), dpr);
            if (!m_previewImage.isNull()) {
                QPainterPath framePath;
                framePath.addRoundedRect(bounds, radius(), radius());
                QPainterPath hole;
                hole.addRect(content());
                p->save();
                p->setClipPath(framePath.subtracted(hole), Qt::IntersectClip);
                p->drawImage(QPointF(), m_previewImage);
                p->restore();
            }
        }
        if (!isUsingShaderMaterial()) {
            ensureFrameCache(bounds.size(), dpr);
            if (!m_cachedFrame.isNull()) {
                if (!m_stretchedFrame) {
                    p->drawImage(QPointF(0.0, 0.0), m_cachedFrame);
                } else {
                    const qreal cap = m_frameCap, top = m_cfg.titleHeight + cap;
                    const qreal sourceX[] = {0, cap, m_frameCacheSize.width() - cap, m_frameCacheSize.width()};
                    const qreal sourceY[] = {0, top, m_frameCacheSize.height() - cap, m_frameCacheSize.height()};
                    const qreal targetX[] = {0, cap, bounds.width() - cap, bounds.width()};
                    const qreal targetY[] = {0, top, bounds.height() - cap, bounds.height()};
                    for (int y = 0; y < 3; ++y) for (int x = 0; x < 3; ++x) {
                        if (x == 1 && y == 1) continue; // The client hole stays untouched.
                        const QRectF target(targetX[x], targetY[y], targetX[x + 1] - targetX[x], targetY[y + 1] - targetY[y]);
                        if (!target.intersects(repaintArea)) continue;
                        const QRectF source(sourceX[x] * dpr, sourceY[y] * dpr,
                            (sourceX[x + 1] - sourceX[x]) * dpr, (sourceY[y + 1] - sourceY[y]) * dpr);
                        p->drawImage(target, m_cachedFrame, source);
                    }
                }
            }
        }

        // Signature cursor glow stays above both CPU and GPU glass, independent
        // of material opacity, and follows the decoration through deformation.
        if (m_cfg.hoverGlow > 0.01f && m_pointer.x() >= 0.0) {
            QPainterPath framePath;
            framePath.addRoundedRect(bounds, radius(), radius());
            const QRectF holeRect = content();
            if (holeRect.width() > 0.0 && holeRect.height() > 0.0) {
                QPainterPath holePath;
                holePath.addRect(holeRect);
                framePath = framePath.subtracted(holePath);
            }
            QRadialGradient rimGlow(m_pointer, 60.0);
            QColor glowCol = currentAccentColor().lighter(135);
            glowCol.setAlphaF(std::clamp(0.28f * m_cfg.hoverGlow, 0.0f, 0.55f));
            rimGlow.setColorAt(0.0, glowCol);
            glowCol.setAlphaF(0.0f);
            rimGlow.setColorAt(1.0, glowCol);
            p->fillPath(framePath, rimGlow);
        }

        // Render caption text with crisp drop shadow
        QFont titleFont = settings()->font();
        titleFont.setWeight(QFont::DemiBold);
        p->setFont(titleFont);

        const qreal leftLimit = (m_leftButtons && !m_leftButtons->buttons().isEmpty() && m_leftButtons->geometry().width() > 0.0)
            ? (m_leftButtons->geometry().right() + 10.0)
            : 17.0;
        const qreal rightLimit = (m_rightButtons && !m_rightButtons->buttons().isEmpty() && m_rightButtons->geometry().width() > 0.0)
            ? (bounds.width() - m_rightButtons->geometry().left() + 10.0)
            : 17.0;

        const QRectF availableTitle(leftLimit, 0.0, std::max(0.0, bounds.width() - leftLimit - rightLimit), borderTop() - 1.0);
        if (availableTitle.width() > 4.0) {
            const QFontMetricsF fm(titleFont);
            const QString caption = fm.elidedText(window()->caption(), Qt::ElideRight, availableTitle.width());
            Qt::Alignment align = Qt::AlignVCenter | Qt::AlignLeft;
            QRectF drawRect = availableTitle;

            if (m_cfg.titleAlignment == 1) {
                align = Qt::AlignVCenter | Qt::AlignHCenter;
            } else if (m_cfg.titleAlignment == 2) {
                const qreal textW = fm.horizontalAdvance(caption);
                const qreal centeredLeft = (bounds.width() - textW) * 0.5;
                if (centeredLeft >= leftLimit && centeredLeft + textW <= bounds.width() - rightLimit) {
                    drawRect = QRectF(0.0, 0.0, bounds.width(), borderTop() - 1.0);
                    align = Qt::AlignVCenter | Qt::AlignHCenter;
                } else {
                    align = Qt::AlignVCenter | Qt::AlignHCenter;
                }
            }

            // Keep the chosen ink stable while deformation temporarily uses
            // the native material instead of the optical shader.
            const bool opticalTitle = m_cfg.shaderEffectEnabled &&
                (m_isPreview ? m_previewEffectEnabled : property("egShader").toBool());
            const QColor titleColor = m_cfg.titleColorForWindow(window()->isActive(), opticalTitle);
            p->setPen(titleShadowColor(titleColor));
            p->drawText(drawRect.translated(0.0, 1.0), int(align), caption);
            p->setPen(titleColor);
            p->drawText(drawRect, int(align), caption);
        }

        if (m_leftButtons) {
            m_leftButtons->paint(p, repaintArea);
        }
        if (m_rightButtons) {
            m_rightButtons->paint(p, repaintArea);
        }

        p->restore();
    }

    void publish()
    {
        QVariantList geometry;
        int hover = -1;
        int pressed = -1;
        int closeIdx = -1;

        auto collectButtons = [&](KDecoration3::DecorationButtonGroup *group) {
            if (!group) {
                return;
            }
            for (auto *btn : group->buttons()) {
                if (!btn->isVisible() || btn->type() == KDecoration3::DecorationButtonType::Spacer) {
                    continue;
                }
                const int idx = int(geometry.size());
                if (idx >= 12) {
                    break;
                }
                geometry.append(btn->geometry());
                if (btn->isHovered()) {
                    hover = idx;
                }
                if (btn->isPressed()) {
                    pressed = idx;
                }
                if (btn->type() == KDecoration3::DecorationButtonType::Close) {
                    closeIdx = idx;
                }
            }
        };

        collectButtons(m_leftButtons);
        collectButtons(m_rightButtons);

        setProperty("egEnabled", true);
        setProperty("egOptics", m_cfg.shaderEffectEnabled);
        setProperty("egButtons", geometry);
        setProperty("egCloseButton", closeIdx);
        setProperty("egHover", hover);
        setProperty("egPressed", pressed);
        setProperty("egPointer", m_pointer);
        setProperty("egContent", content());
        setProperty("egColor", currentAccentColor());
        setProperty("egRefraction", m_cfg.refraction);
        setProperty("egTint", currentTintStrength());
        setProperty("egFrameOpacity", m_cfg.frameOpacity);
        setProperty("egDispersion", m_cfg.dispersion);
        setProperty("egSpecular", m_cfg.specular);
        setProperty("egHoverGlow", m_cfg.hoverGlow);
        setProperty("egAdaptiveContrast", m_cfg.adaptiveContrast);
        setProperty("egCornerRadius", radius());
        setProperty("egTitleHeight", qreal(m_cfg.titleHeight));
        update();
    }

protected:
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::DynamicPropertyChange) {
            const auto name = static_cast<QDynamicPropertyChangeEvent *>(event)->propertyName();
            if (name == "egShader" || name == "egTransformed") {
                layoutButtons();
            }
        }
        return KDecoration3::Decoration::event(event);
    }

    void hoverEnterEvent(QHoverEvent *event) override
    {
        KDecoration3::Decoration::hoverEnterEvent(event);
        m_pointer = event->position();
        publish();
    }

    void hoverMoveEvent(QHoverEvent *event) override
    {
        KDecoration3::Decoration::hoverMoveEvent(event);
        const QPointF newPos = event->position();
        if ((newPos - m_pointer).manhattanLength() >= 1.0) {
            const QRectF oldGlow(m_pointer - QPointF(60.0, 60.0), QSizeF(120.0, 120.0));
            m_pointer = newPos;
            setProperty("egPointer", m_pointer);
            const QRectF newGlow(m_pointer - QPointF(60.0, 60.0), QSizeF(120.0, 120.0));
            update(oldGlow.united(newGlow).intersected(rect()));
        }
    }

    void hoverLeaveEvent(QHoverEvent *event) override
    {
        KDecoration3::Decoration::hoverLeaveEvent(event);
        m_pointer = QPointF(-100.0, -100.0);
        publish();
    }

private:
    qreal radius() const
    {
        return (window() && window()->isMaximized()) ? 0.0 : qreal(m_cfg.cornerRadius);
    }

    QRectF content() const
    {
        return rect().adjusted(borderLeft(), borderTop(), -borderRight(), -borderBottom());
    }

    void ensurePreviewFrame(const QSizeF &size, qreal dpr)
    {
        PreviewFrame frame;
        frame.frame = rect();
        frame.content = content();
        frame.active = window()->isActive();
        frame.radius = radius();
        frame.pointer = m_pointer;
        frame.closeButton = property("egCloseButton").toInt();
        frame.hoverButton = property("egHover").toInt();
        frame.pressedButton = property("egPressed").toInt();
        for (const auto &button : property("egButtons").toList()) frame.buttons.append(button.toRectF());
        if (m_previewPending || (m_previewAttempted && m_previewSize == size && m_previewDpr == dpr &&
            m_previewConfig == m_cfg && m_previewFrame == frame)) return;
        m_previewPending = true;
        // QQuickPaintedItem may paint on the render thread while the GUI thread
        // waits. Build the private GL image asynchronously on the GUI thread;
        // do not create widgets/contexts in paint() or block waiting for the GUI.
        QMetaObject::invokeMethod(this, [this, size, dpr, frame, cfg = m_cfg, effectEnabled = m_previewEffectEnabled] {
            if (!m_previewRenderer) m_previewRenderer = std::make_unique<MaterialPreview>();
            auto renderConfig = cfg;
            renderConfig.shaderEffectEnabled &= effectEnabled;
            m_previewRenderer->setConfig(renderConfig);
            m_previewRenderer->setEffectAvailable(true);
            m_previewImage = m_previewRenderer->renderDecorationFrame(size, dpr, frame);
            m_previewSize = size;
            m_previewDpr = dpr;
            m_previewFrame = frame;
            m_previewConfig = cfg;
            m_previewAttempted = true;
            m_previewPending = false;
            setProperty("egPreviewMaterial", !m_previewImage.isNull());
            update();
        }, Qt::QueuedConnection);
    }

    void invalidateCache()
    {
        m_cachedFrame = QImage();
        m_previewImage = QImage();
        m_previewAttempted = false;
    }

    void ensureFrameCache(const QSizeF &logicalSize, qreal dpr)
    {
        // Away from the corners the SDF and title lighting are invariant along
        // each edge. Cache those strips once instead of rerendering on resize.
        const qreal cap = std::max(m_cfg.titleHeight, m_cfg.borderWidth + m_cfg.cornerRadius) + 4;
        const bool stretch = logicalSize.width() >= cap * 2 + 1 && logicalSize.height() >= m_cfg.titleHeight + cap * 2 + 1;
        auto samePixelPhase = [dpr](qreal base, qreal target) {
            const qreal difference = (target - base) * dpr;
            return base + (difference - std::floor(difference)) / dpr;
        };
        const QSizeF cacheSize = stretch ? QSizeF(samePixelPhase(cap * 2 + 1, logicalSize.width()),
            samePixelPhase(m_cfg.titleHeight + cap * 2 + 1, logicalSize.height())) : logicalSize;
        const QRectF cacheRect(QPointF(), cacheSize);
        const QRectF cacheContent = cacheRect.adjusted(borderLeft(), borderTop(), -borderRight(), -borderBottom());
        const QSize pixelSize(std::max(1, qRound(cacheSize.width() * dpr)),
                              std::max(1, qRound(cacheSize.height() * dpr)));
        const bool active = window() && window()->isActive();
        const bool maximized = window() && window()->isMaximized();

        if (!m_cachedFrame.isNull() &&
            m_cachedPixelSize == pixelSize &&
            qFuzzyCompare(m_cachedDpr, dpr) &&
            m_cachedActive == active &&
            m_cachedMaximized == maximized &&
            m_stretchedFrame == stretch && m_cachedContent == cacheContent) {
            return;
        }

        m_cachedPixelSize = pixelSize;
        m_cachedDpr = dpr;
        m_cachedActive = active;
        m_cachedMaximized = maximized;
        m_cachedContent = cacheContent;
        m_stretchedFrame = stretch;
        m_frameCap = cap;
        m_frameCacheSize = cacheSize;

        m_cachedFrame = renderFrameGlassImage(
            pixelSize,
            dpr,
            cacheRect,
            m_cachedContent,
            currentAccentColor(),
            currentTintStrength(),
            float(radius()),
            m_cfg);
        m_cachedFrame.setDevicePixelRatio(dpr);
    }

    void configure()
    {
        m_cfg = loadMaterialConfig();
        if (m_isPreview) {
            const auto config = KSharedConfig::openConfig(QStringLiteral("kwinrc"));
            config->reparseConfiguration();
            m_previewEffectEnabled = KConfigGroup(config, QStringLiteral("Plugins")).readEntry("emeraldglassEnabled", false);
        }
        invalidateCache();
        rebuildButtons();
        layout();
        makeShadow();
    }

    void rebuildButtons()
    {
        if (!m_leftButtons || !m_rightButtons) {
            return;
        }

        for (auto *btn : m_leftButtons->buttons()) {
            m_leftButtons->removeButton(btn);
            btn->deleteLater();
        }
        for (auto *btn : m_rightButtons->buttons()) {
            m_rightButtons->removeButton(btn);
            btn->deleteLater();
        }

        auto addGroupButtons = [&](KDecoration3::DecorationButtonGroup *group,
                                   const QList<KDecoration3::DecorationButtonType> &types) {
            for (const auto type : types) {
                if (auto *btn = Button::create(type, this, group)) {
                    connect(btn, &KDecoration3::DecorationButton::hoveredChanged, this, &Decoration::publish);
                    connect(btn, &KDecoration3::DecorationButton::pressedChanged, this, &Decoration::publish);
                    connect(btn, &KDecoration3::DecorationButton::visibilityChanged, this, &Decoration::layoutButtons);
                    group->addButton(btn);
                }
            }
        };

        if (m_cfg.useSystemButtons && settings()) {
            addGroupButtons(m_leftButtons, settings()->decorationButtonsLeft());
            addGroupButtons(m_rightButtons, settings()->decorationButtonsRight());
        } else {
            addGroupButtons(m_rightButtons, {
                KDecoration3::DecorationButtonType::Minimize,
                KDecoration3::DecorationButtonType::Maximize,
                KDecoration3::DecorationButtonType::Close,
            });
        }
        layoutButtons();
    }

    void layout()
    {
        const bool maximized = window() && window()->isMaximized();
        const bool shaded = window() && window()->isShaded();
        const qreal side = maximized ? 0.0 : qreal(m_cfg.borderWidth);
        const qreal bottom = (maximized || shaded) ? 0.0 : qreal(m_cfg.borderWidth);
        const qreal top = qreal(m_cfg.titleHeight);

        setState([=, this](KDecoration3::DecorationState *state) {
            state->setBorders(QMarginsF(side, top, side, bottom));
            state->setBorderRadius(KDecoration3::BorderRadius(radius()));
        });
        setTitleBar(QRectF(0.0, 0.0, rect().width(), top));
        layoutButtons();
    }

    void layoutButtons()
    {
        const qreal topH = borderTop() > 0.0 ? borderTop() : qreal(m_cfg.titleHeight);
        const qreal btnY = std::max(2.0, std::floor((topH - 23.0) * 0.5));

        if (m_leftButtons) {
            m_leftButtons->setPos(QPointF(14.0, btnY));
        }
        if (m_rightButtons) {
            const qreal rightWidth = m_rightButtons->geometry().width();
            m_rightButtons->setPos(QPointF(std::max(14.0, rect().right() - rightWidth - 17.0), btnY));
        }

        QPainterPath outer;
        outer.addRoundedRect(rect(), radius(), radius());
        const QRectF c = content();
        if (c.width() > 0.0 && c.height() > 0.0) {
            QPainterPath hole;
            hole.addRect(c);
            outer = outer.subtracted(hole);
        }
        const QRegion frameRegion(outer.toFillPolygon().toPolygon());
        setProperty("egFrameRegion", frameRegion);
        // Standard Blur handles the CPU fallback only. The shader effect
        // consumes egFrameRegion directly, without blurring the frame twice.
        setBlurRegion(property("egShader").toBool() && !property("egTransformed").toBool() ? QRegion() : frameRegion);
        publish();
    }

    void makeShadow()
    {
        constexpr int pad = 16;
        constexpr int inside = 24;
        const QImage image = renderShadowImage(pad, inside, float(m_cfg.cornerRadius));
        auto result = std::make_shared<KDecoration3::DecorationShadow>();
        result->setShadow(image);
        result->setPadding(QMarginsF(pad, pad, pad, pad));
        result->setInnerShadowRect(QRectF(pad, pad, inside, inside));
        setShadow(result);
    }

    bool m_isPreview = false;
    bool m_previewEffectEnabled = false;
    bool m_previewPending = false;
    bool m_previewAttempted = false;
    std::unique_ptr<MaterialPreview> m_previewRenderer;
    QImage m_previewImage;
    QSizeF m_previewSize;
    qreal m_previewDpr = 1;
    MaterialConfig m_previewConfig;
    PreviewFrame m_previewFrame;
    MaterialConfig m_cfg;
    KDecoration3::DecorationButtonGroup *m_leftButtons = nullptr;
    KDecoration3::DecorationButtonGroup *m_rightButtons = nullptr;
    QPointF m_pointer{-100.0, -100.0};

    QImage m_cachedFrame;
    QSize m_cachedPixelSize;
    qreal m_cachedDpr = 1.0;
    bool m_cachedActive = false;
    bool m_cachedMaximized = false;
    QRectF m_cachedContent;
    bool m_stretchedFrame = false;
    qreal m_frameCap = 0;
    QSizeF m_frameCacheSize;
};

KDecoration3::DecorationButton *Button::create(KDecoration3::DecorationButtonType type,
                                               KDecoration3::Decoration *decoration,
                                               QObject *parent)
{
    auto *deco = static_cast<Decoration *>(decoration);
    if (!deco || !deco->window()) {
        return nullptr;
    }
    auto *btn = new Button(type, deco, parent);
    auto *w = deco->window();
    switch (type) {
    case KDecoration3::DecorationButtonType::Close:
        btn->setVisible(w->isCloseable());
        btn->setEnabled(w->isCloseable());
        connect(w, &KDecoration3::DecoratedWindow::closeableChanged, btn, [btn](bool v) {
            btn->setVisible(v);
            btn->setEnabled(v);
        });
        break;
    case KDecoration3::DecorationButtonType::Maximize:
        btn->setVisible(w->isMaximizeable());
        btn->setEnabled(w->isMaximizeable());
        btn->setCheckable(true);
        btn->setChecked(w->isMaximized());
        btn->setAcceptedButtons(Qt::LeftButton | Qt::MiddleButton | Qt::RightButton);
        connect(w, &KDecoration3::DecoratedWindow::maximizeableChanged, btn, [btn](bool v) {
            btn->setVisible(v);
            btn->setEnabled(v);
        });
        connect(w, &KDecoration3::DecoratedWindow::maximizedChanged, btn, &KDecoration3::DecorationButton::setChecked);
        break;
    case KDecoration3::DecorationButtonType::Minimize:
        btn->setVisible(w->isMinimizeable());
        btn->setEnabled(w->isMinimizeable());
        connect(w, &KDecoration3::DecoratedWindow::minimizeableChanged, btn, [btn](bool v) {
            btn->setVisible(v);
            btn->setEnabled(v);
        });
        break;
    case KDecoration3::DecorationButtonType::ContextHelp:
        btn->setVisible(w->providesContextHelp());
        connect(w, &KDecoration3::DecoratedWindow::providesContextHelpChanged, btn, &KDecoration3::DecorationButton::setVisible);
        break;
    case KDecoration3::DecorationButtonType::ApplicationMenu:
        btn->setVisible(w->hasApplicationMenu());
        connect(w, &KDecoration3::DecoratedWindow::hasApplicationMenuChanged, btn, &KDecoration3::DecorationButton::setVisible);
        break;
    case KDecoration3::DecorationButtonType::OnAllDesktops:
        btn->setVisible(deco->settings()->isOnAllDesktopsAvailable());
        btn->setCheckable(true);
        btn->setChecked(w->isOnAllDesktops());
        connect(deco->settings().get(), &KDecoration3::DecorationSettings::onAllDesktopsAvailableChanged,
                btn, &KDecoration3::DecorationButton::setVisible);
        connect(w, &KDecoration3::DecoratedWindow::onAllDesktopsChanged, btn, &KDecoration3::DecorationButton::setChecked);
        break;
    case KDecoration3::DecorationButtonType::KeepAbove:
        btn->setCheckable(true);
        btn->setChecked(w->isKeepAbove());
        connect(w, &KDecoration3::DecoratedWindow::keepAboveChanged, btn, &KDecoration3::DecorationButton::setChecked);
        break;
    case KDecoration3::DecorationButtonType::KeepBelow:
        btn->setCheckable(true);
        btn->setChecked(w->isKeepBelow());
        connect(w, &KDecoration3::DecoratedWindow::keepBelowChanged, btn, &KDecoration3::DecorationButton::setChecked);
        break;
    case KDecoration3::DecorationButtonType::Shade:
        btn->setVisible(w->isShadeable());
        btn->setEnabled(w->isShadeable());
        btn->setCheckable(true);
        btn->setChecked(w->isShaded());
        connect(w, &KDecoration3::DecoratedWindow::shadeableChanged, btn, [btn](bool v) {
            btn->setVisible(v);
            btn->setEnabled(v);
        });
        connect(w, &KDecoration3::DecoratedWindow::shadedChanged, btn, &KDecoration3::DecorationButton::setChecked);
        break;
    case KDecoration3::DecorationButtonType::ExcludeFromCapture:
        btn->setCheckable(true);
        btn->setChecked(w->isExcludedFromCapture());
        btn->setVisible(deco->settings()->isAlwaysShowExcludeFromCapture() || w->isExcludedFromCapture());
        connect(w, &KDecoration3::DecoratedWindow::excludeFromCaptureChanged, btn, [btn, deco](bool v) {
            btn->setChecked(v);
            btn->setVisible(deco->settings()->isAlwaysShowExcludeFromCapture() || v);
        });
        break;
    default:
        break;
    }
    return btn;
}

Button::Button(KDecoration3::DecorationButtonType type, Decoration *decoration, QObject *parent)
    : KDecoration3::DecorationButton(type, decoration, parent ? parent : decoration)
{
    setAcceptedButtons(Qt::LeftButton);
    if (type == KDecoration3::DecorationButtonType::Spacer) {
        setGeometry(QRectF(0.0, 0.0, 10.0, 23.0));
    } else {
        setGeometry(QRectF(0.0, 0.0, 27.0, 23.0));
    }
}

void Button::paint(QPainter *p, const QRectF &repaintArea)
{
    if (!isVisible() || type() == KDecoration3::DecorationButtonType::Spacer || !geometry().adjusted(-1, -1, 1, 1).intersects(repaintArea)) {
        return;
    }

    auto *deco = static_cast<Decoration *>(decoration());
    if (!deco) {
        return;
    }

    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);

    const qreal dpr = (p->device() && p->device()->devicePixelRatioF() > 0.0)
        ? p->device()->devicePixelRatioF()
        : (deco->window() ? std::max(deco->window()->scale(), 1.0) : 1.0);

    if (!deco->isUsingShaderMaterial()) {
        const QPointF ptr = isHovered()
            ? (deco->pointerPos().x() >= 0.0 ? deco->pointerPos() : geometry().center())
            : QPointF(-100.0, -100.0);
        const QRect aligned = geometry().adjusted(-1, -1, 1, 1).toAlignedRect();
        const MaterialKey key{deco->config(), deco->currentAccentColor(),
            geometry().translated(-aligned.topLeft()),
            isHovered() ? ptr - aligned.topLeft() : QPointF(-100, -100),
            dpr, deco->currentTintStrength(), isHovered(), isPressed()};
        if (m_materialImage.isNull() || m_materialKey != key) {
            m_materialKey = key;
            m_materialImage = QImage(QSize(std::ceil(aligned.width() * dpr), std::ceil(aligned.height() * dpr)), QImage::Format_ARGB32_Premultiplied);
            m_materialImage.setDevicePixelRatio(dpr);
            m_materialImage.fill(Qt::transparent);
            QPainter lens(&m_materialImage);
            paintButtonGlass(&lens, key.rect, dpr, key.accent, key.tint,
                type() == KDecoration3::DecorationButtonType::Close,
                key.hovered, key.pressed, key.pointer, key.config);
        }
        p->drawImage(aligned.topLeft(), m_materialImage);
    }

    QPointF c = geometry().center();
    if (isPressed()) {
        c.ry() += 1.0;
    }

    const QColor glyphColor = isEnabled()
        ? (deco->window()->isActive() ? QColor("#effff6") : QColor("#d9e8e0"))
        : QColor("#7c9184");

    p->setPen(QPen(glyphColor, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p->setBrush(Qt::NoBrush);

    switch (type()) {
    case KDecoration3::DecorationButtonType::Minimize:
        p->drawLine(c + QPointF(-4.0, 3.0), c + QPointF(4.0, 3.0));
        break;
    case KDecoration3::DecorationButtonType::Maximize:
        if (deco->window()->isMaximized()) {
            p->drawRect(QRectF(c + QPointF(-2.0, -4.0), QSizeF(6.0, 6.0)));
            p->fillRect(QRectF(c + QPointF(-4.0, -2.0), QSizeF(6.0, 6.0)), QColor(15, 30, 22, 210));
            p->drawRect(QRectF(c + QPointF(-4.0, -2.0), QSizeF(6.0, 6.0)));
        } else {
            p->drawRect(QRectF(c - QPointF(4.0, 3.5), QSizeF(8.0, 7.0)));
        }
        break;
    case KDecoration3::DecorationButtonType::Close:
        p->drawLine(c + QPointF(-3.0, -3.0), c + QPointF(3.0, 3.0));
        p->drawLine(c + QPointF(-3.0, 3.0), c + QPointF(3.0, -3.0));
        break;
    case KDecoration3::DecorationButtonType::Menu: {
        const QIcon icon = deco->window()->icon();
        if (!icon.isNull()) {
            const QRectF iconRect(c - QPointF(10.0, 10.0), QSizeF(20.0, 20.0));
            const QPixmap pixmap = icon.pixmap(QSize(20, 20), dpr);
            p->setRenderHint(QPainter::SmoothPixmapTransform, true);
            p->drawPixmap(iconRect, pixmap, QRectF(pixmap.rect()));
        } else {
            p->drawEllipse(c, 3.5, 3.5);
        }
        break;
    }
    case KDecoration3::DecorationButtonType::ApplicationMenu:
        p->drawLine(c + QPointF(-4.0, -3.0), c + QPointF(4.0, -3.0));
        p->drawLine(c + QPointF(-4.0, 0.0), c + QPointF(4.0, 0.0));
        p->drawLine(c + QPointF(-4.0, 3.0), c + QPointF(4.0, 3.0));
        break;
    case KDecoration3::DecorationButtonType::OnAllDesktops:
        if (isChecked()) {
            p->setBrush(glyphColor);
            p->drawEllipse(c, 3.2, 3.2);
        } else {
            p->drawEllipse(c, 3.2, 3.2);
            p->drawPoint(c);
        }
        break;
    case KDecoration3::DecorationButtonType::KeepAbove: {
        QPainterPath chevron;
        chevron.moveTo(c + QPointF(-4.0, 2.0));
        chevron.lineTo(c + QPointF(0.0, -2.5));
        chevron.lineTo(c + QPointF(4.0, 2.0));
        p->drawPath(chevron);
        if (isChecked()) {
            p->drawLine(c + QPointF(-4.0, -4.0), c + QPointF(4.0, -4.0));
        }
        break;
    }
    case KDecoration3::DecorationButtonType::KeepBelow: {
        QPainterPath chevron;
        chevron.moveTo(c + QPointF(-4.0, -2.0));
        chevron.lineTo(c + QPointF(0.0, 2.5));
        chevron.lineTo(c + QPointF(4.0, -2.0));
        p->drawPath(chevron);
        if (isChecked()) {
            p->drawLine(c + QPointF(-4.0, 4.0), c + QPointF(4.0, 4.0));
        }
        break;
    }
    case KDecoration3::DecorationButtonType::Shade:
        p->drawLine(c + QPointF(-4.0, -3.0), c + QPointF(4.0, -3.0));
        if (!isChecked()) {
            p->drawRect(QRectF(c + QPointF(-4.0, -1.0), QSizeF(8.0, 4.5)));
        }
        break;
    case KDecoration3::DecorationButtonType::ContextHelp: {
        QFont f = deco->settings()->font();
        f.setBold(true);
        f.setPixelSize(11);
        p->setFont(f);
        p->drawText(geometry(), Qt::AlignCenter, QStringLiteral("?"));
        break;
    }
    case KDecoration3::DecorationButtonType::ExcludeFromCapture:
        p->drawEllipse(c, 3.5, 2.5);
        if (isChecked()) {
            p->drawLine(c + QPointF(-4.0, 3.5), c + QPointF(4.0, -3.5));
        }
        break;
    default:
        break;
    }

    p->restore();
}

} // namespace EmeraldGlass

K_PLUGIN_FACTORY_WITH_JSON(EmeraldGlassFactory, "decoration.json", registerPlugin<EmeraldGlass::Decoration>();)
#include "decoration.moc"
