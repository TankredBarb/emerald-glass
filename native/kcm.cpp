// SPDX-License-Identifier: LGPL-3.0-or-later
#include "material.h"
#include "materialpreview.h"

#include <KCModule>
#include <KPluginFactory>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QSpinBox>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

namespace EmeraldGlass {

class EmeraldGlassConfigModule : public KCModule {
    Q_OBJECT
public:
    EmeraldGlassConfigModule(QObject *parent, const KPluginMetaData &data)
        : KCModule(parent, data)
    {
        auto *rootLayout = new QVBoxLayout(widget());
        rootLayout->setContentsMargins(12, 12, 12, 12);
        rootLayout->setSpacing(10);

        m_preview = new MaterialPreview(widget());
        rootLayout->addWidget(m_preview);
        m_effectStatus = new QLabel(widget());
        m_effectStatus->setWordWrap(true);
        rootLayout->addWidget(m_effectStatus);

        auto *tabs = new QTabWidget(widget());
        rootLayout->addWidget(tabs, 1);

        // Tab 1: Colors & Material
        auto *colorPage = new QWidget(tabs);
        auto *colorLayout = new QVBoxLayout(colorPage);

        auto *blurGroup = new QGroupBox(QStringLiteral("Border Blur"), colorPage);
        auto *blurForm = new QFormLayout(blurGroup);
        m_blurSlider = makeSliderRow(blurForm, blurGroup, QStringLiteral("Blurriness:"), 0, 15, QStringLiteral(" / 15"));
        m_blurSlider->setObjectName(QStringLiteral("frameBlurStrength"));
        m_blurSlider->setToolTip(QStringLiteral("0 keeps the background sharp; 15 gives maximum blur. Applies only to Emerald Glass borders. The CPU fallback uses standard KWin Blur settings."));
        colorLayout->addWidget(blurGroup);

        auto *colorGroup = new QGroupBox(QStringLiteral("Glass Color Palette"), colorPage);
        auto *colorForm = new QFormLayout(colorGroup);

        m_activePreset = new QComboBox(colorGroup);
        m_inactivePreset = new QComboBox(colorGroup);
        for (const auto &preset : kColorPresets) {
            m_activePreset->addItem(QString::fromUtf8(preset.name), QString::fromLatin1(preset.id));
            m_inactivePreset->addItem(QString::fromUtf8(preset.name), QString::fromLatin1(preset.id));
        }
        m_activePreset->addItem(QStringLiteral("Custom Color..."), QStringLiteral("custom"));
        m_inactivePreset->addItem(QStringLiteral("Custom Color..."), QStringLiteral("custom"));

        m_activeColorBtn = new QPushButton(QStringLiteral("Pick Color..."), colorGroup);
        m_inactiveColorBtn = new QPushButton(QStringLiteral("Pick Color..."), colorGroup);

        auto *activeRow = new QHBoxLayout;
        activeRow->addWidget(m_activePreset, 1);
        activeRow->addWidget(m_activeColorBtn);
        colorForm->addRow(QStringLiteral("Active window:"), activeRow);

        auto *inactiveRow = new QHBoxLayout;
        inactiveRow->addWidget(m_inactivePreset, 1);
        inactiveRow->addWidget(m_inactiveColorBtn);
        colorForm->addRow(QStringLiteral("Inactive window:"), inactiveRow);

        m_activeTitleText = new QComboBox(colorGroup);
        m_inactiveTitleText = new QComboBox(colorGroup);
        m_activeTitleText->setObjectName(QStringLiteral("activeTitleText"));
        m_inactiveTitleText->setObjectName(QStringLiteral("inactiveTitleText"));
        for (auto *combo : {m_activeTitleText, m_inactiveTitleText}) {
            combo->addItem(QStringLiteral("Auto"), 0);
            combo->addItem(QStringLiteral("Light"), 1);
            combo->addItem(QStringLiteral("Dark"), 2);
            combo->setToolTip(QStringLiteral("Auto estimates contrast from the glass color, tint, opacity and contrast settings. It stays stable as windows move."));
            connect(combo, &QComboBox::currentIndexChanged, this, &EmeraldGlassConfigModule::onUiEdited);
        }
        colorForm->addRow(QStringLiteral("Active title text:"), m_activeTitleText);
        colorForm->addRow(QStringLiteral("Inactive title text:"), m_inactiveTitleText);

        m_tintSlider = makeSliderRow(colorForm, colorGroup, QStringLiteral("Active tint strength:"), 0, 200, QStringLiteral("%"));
        m_inactiveTintSlider = makeSliderRow(colorForm, colorGroup, QStringLiteral("Inactive tint strength:"), 0, 200, QStringLiteral("%"));
        m_tintSlider->setObjectName(QStringLiteral("activeTintStrength"));
        m_inactiveTintSlider->setObjectName(QStringLiteral("inactiveTintStrength"));
        m_opacitySlider = makeSliderRow(colorForm, colorGroup, QStringLiteral("Base glass opacity:"), 20, 100, QStringLiteral("%"));
        m_hoverGlowSlider = makeSliderRow(colorForm, colorGroup, QStringLiteral("Cursor halo strength:"), 0, 150, QStringLiteral("%"));
        m_hoverGlowSlider->setObjectName(QStringLiteral("cursorGlowStrength"));

        colorLayout->addWidget(colorGroup);
        colorLayout->addStretch(1);
        tabs->addTab(colorPage, QStringLiteral("Colors && Material"));

        // Tab 2: Liquid Glass Optics
        auto *opticsPage = new QWidget(tabs);
        auto *opticsLayout = new QVBoxLayout(opticsPage);
        auto *opticsGroup = new QGroupBox(QStringLiteral("Emerald & Liquid Glass Optics"), opticsPage);
        m_opticsGroup = opticsGroup;
        m_opticsGroup->setObjectName(QStringLiteral("liquidGlassControls"));
        auto *opticsForm = new QFormLayout(opticsGroup);

        m_refractionSlider = makeSliderRow(opticsForm, opticsGroup, QStringLiteral("Refraction & optical thickness:"), 0, 180, QStringLiteral("%"));
        m_refractionSlider->setObjectName(QStringLiteral("refractionStrength"));
        m_refractionSlider->setToolTip(QStringLiteral("Changes background refraction and the thickness of the reflecting glass edge."));
        m_dispersionSlider = makeSliderRow(opticsForm, opticsGroup, QStringLiteral("Chromatic dispersion (Liquid Glass):"), 0, 100, QStringLiteral("%"));
        m_dispersionSlider->setObjectName(QStringLiteral("chromaticDispersion"));
        m_dispersionSlider->setToolTip(QStringLiteral("Separates colors in the refracted background and edge reflection. Zero refraction disables the separation."));
        m_specularSlider = makeSliderRow(opticsForm, opticsGroup, QStringLiteral("Specular & bevel glare:"), 0, 150, QStringLiteral("%"));
        m_adaptiveSlider = makeSliderRow(opticsForm, opticsGroup, QStringLiteral("Adaptive backdrop contrast:"), 0, 100, QStringLiteral("%"));
        m_adaptiveSlider->setObjectName(QStringLiteral("adaptiveBackdropContrast"));
        m_shaderCheck = new QCheckBox(QStringLiteral("Use Liquid Glass optics"), opticsPage);
        m_shaderCheck->setObjectName(QStringLiteral("useLiquidGlass"));
        opticsLayout->addWidget(m_shaderCheck);

        opticsLayout->addWidget(opticsGroup);
        opticsLayout->addStretch(1);
        tabs->addTab(opticsPage, QStringLiteral("Liquid Glass Optics"));

        // Tab 3: Geometry & Buttons
        auto *geomPage = new QWidget(tabs);
        auto *geomLayout = new QVBoxLayout(geomPage);
        auto *geomGroup = new QGroupBox(QStringLiteral("Frame Geometry & Buttons"), geomPage);
        auto *geomForm = new QFormLayout(geomGroup);

        m_titleHeightSpin = new QSpinBox(geomGroup);
        m_titleHeightSpin->setRange(26, 56);
        m_titleHeightSpin->setSuffix(QStringLiteral(" px"));
        geomForm->addRow(QStringLiteral("Titlebar height:"), m_titleHeightSpin);

        m_borderWidthSpin = new QSpinBox(geomGroup);
        m_borderWidthSpin->setRange(0, 20);
        m_borderWidthSpin->setSuffix(QStringLiteral(" px"));
        geomForm->addRow(QStringLiteral("Frame border width:"), m_borderWidthSpin);

        m_cornerRadiusSpin = new QSpinBox(geomGroup);
        m_cornerRadiusSpin->setRange(0, 18);
        m_cornerRadiusSpin->setSuffix(QStringLiteral(" px"));
        geomForm->addRow(QStringLiteral("Corner radius:"), m_cornerRadiusSpin);

        m_titleAlignCombo = new QComboBox(geomGroup);
        m_titleAlignCombo->addItem(QStringLiteral("Left aligned (Classic Emerald)"), 0);
        m_titleAlignCombo->addItem(QStringLiteral("Centered between buttons"), 1);
        m_titleAlignCombo->addItem(QStringLiteral("Centered across window"), 2);
        geomForm->addRow(QStringLiteral("Title alignment:"), m_titleAlignCombo);

        m_useSystemButtonsCheck = new QCheckBox(QStringLiteral("Use KWin system titlebar button layout (left and right)"), geomGroup);
        geomForm->addRow(QString(), m_useSystemButtonsCheck);

        geomLayout->addWidget(geomGroup);
        geomLayout->addStretch(1);
        tabs->addTab(geomPage, QStringLiteral("Geometry && Buttons"));

        // Wire signals
        connect(m_activePreset, &QComboBox::currentIndexChanged, this, &EmeraldGlassConfigModule::onUiEdited);
        connect(m_inactivePreset, &QComboBox::currentIndexChanged, this, &EmeraldGlassConfigModule::onUiEdited);
        connect(m_activeColorBtn, &QPushButton::clicked, this, [this] {
            const QColor c = QColorDialog::getColor(m_working.activeCustomColor, widget(), QStringLiteral("Select Active Window Color"));
            if (c.isValid()) {
                m_working.activeCustomColor = c;
                m_activePreset->setCurrentIndex(m_activePreset->findData(QStringLiteral("custom")));
                onUiEdited();
            }
        });
        connect(m_inactiveColorBtn, &QPushButton::clicked, this, [this] {
            const QColor c = QColorDialog::getColor(m_working.inactiveCustomColor, widget(), QStringLiteral("Select Inactive Window Color"));
            if (c.isValid()) {
                m_working.inactiveCustomColor = c;
                m_inactivePreset->setCurrentIndex(m_inactivePreset->findData(QStringLiteral("custom")));
                onUiEdited();
            }
        });

        for (auto *s : {m_tintSlider, m_inactiveTintSlider, m_opacitySlider,
                        m_refractionSlider, m_blurSlider, m_dispersionSlider, m_specularSlider,
                        m_adaptiveSlider, m_hoverGlowSlider}) {
            connect(s, &QSlider::valueChanged, this, &EmeraldGlassConfigModule::onUiEdited);
        }
        for (auto *sp : {m_titleHeightSpin, m_borderWidthSpin, m_cornerRadiusSpin}) {
            connect(sp, &QSpinBox::valueChanged, this, &EmeraldGlassConfigModule::onUiEdited);
        }
        connect(m_titleAlignCombo, &QComboBox::currentIndexChanged, this, &EmeraldGlassConfigModule::onUiEdited);
        connect(m_useSystemButtonsCheck, &QCheckBox::toggled, this, &EmeraldGlassConfigModule::onUiEdited);
        connect(m_shaderCheck, &QCheckBox::toggled, this, &EmeraldGlassConfigModule::onUiEdited);
        updateOpticsAvailability();
        refreshEffectState();
        auto *effectPoll = new QTimer(this);
        effectPoll->setInterval(1000);
        connect(effectPoll, &QTimer::timeout, this, &EmeraldGlassConfigModule::refreshEffectState);
        effectPoll->start();
    }

    void load() override
    {
        m_saved = loadMaterialConfig();
        m_working = m_saved;
        syncWidgetsFromConfig(m_working);
        setNeedsSave(false);
    }

    void save() override
    {
        m_WorkingFromWidgets();
        saveMaterialConfig(m_working);
        m_saved = m_working;
        setNeedsSave(false);

        // Notify KWin to reload window decorations and effect configuration immediately
        QDBusConnection::sessionBus().send(
            QDBusMessage::createSignal(QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QStringLiteral("reloadConfig")));
        QDBusMessage effectMsg = QDBusMessage::createMethodCall(
            QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Effects"),
            QStringLiteral("org.kde.kwin.Effects"),
            QStringLiteral("reconfigureEffect"));
        effectMsg << QStringLiteral("emeraldglass");
        QDBusConnection::sessionBus().send(effectMsg);
    }

    void defaults() override
    {
        m_working = MaterialConfig{};
        syncWidgetsFromConfig(m_working);
        setNeedsSave(m_working != m_saved);
    }

private:
    void updateOpticsAvailability()
    {
        const bool enabled = m_effectLoaded && m_shaderCheck->isChecked();
        m_preview->setEffectAvailable(m_effectLoaded);
        m_opticsGroup->setEnabled(enabled);
        m_blurSlider->parentWidget()->setEnabled(m_effectLoaded);
        m_shaderCheck->setEnabled(m_effectLoaded);
        m_effectStatus->setText(QStringLiteral("Enable Emerald Glass in Desktop Effects to adjust border blur and Liquid Glass optics."));
        m_effectStatus->setVisible(!m_effectLoaded);
    }

    void refreshEffectState()
    {
        if (m_effectQueryPending) {
            return;
        }
        m_effectQueryPending = true;
        auto message = QDBusMessage::createMethodCall(QStringLiteral("org.kde.KWin"),
            QStringLiteral("/Effects"), QStringLiteral("org.kde.kwin.Effects"), QStringLiteral("isEffectLoaded"));
        message << QStringLiteral("emeraldglass");
        auto *watcher = new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(message, 1000), this);
        connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, watcher] {
            const QDBusPendingReply<bool> reply = *watcher;
            m_effectLoaded = !reply.isError() && reply.value();
            m_effectQueryPending = false;
            updateOpticsAvailability();
            watcher->deleteLater();
        });
    }

    QSlider *makeSliderRow(QFormLayout *form, QWidget *parent, const QString &label, int minVal, int maxVal, const QString &suffix)
    {
        auto *row = new QHBoxLayout;
        auto *slider = new QSlider(Qt::Horizontal, parent);
        slider->setRange(minVal, maxVal);
        auto *valLabel = new QLabel(parent);
        valLabel->setMinimumWidth(46);
        valLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        valLabel->setText(QString::number(slider->value()) + suffix);
        connect(slider, &QSlider::valueChanged, valLabel, [valLabel, suffix](int v) {
            valLabel->setText(QString::number(v) + suffix);
        });
        row->addWidget(slider, 1);
        row->addWidget(valLabel);
        form->addRow(label, row);
        return slider;
    }

    void updateButtonSwatches()
    {
        auto applySwatch = [](QPushButton *btn, const QColor &c) {
            btn->setStyleSheet(QStringLiteral("QPushButton { background-color: %1; color: #101814; font-weight: bold; border-radius: 4px; padding: 4px 10px; }")
                                   .arg(c.name()));
        };
        applySwatch(m_activeColorBtn, m_working.colorForWindow(true));
        applySwatch(m_inactiveColorBtn, m_working.colorForWindow(false));
    }

    void syncWidgetsFromConfig(const MaterialConfig &cfg)
    {
        m_updating = true;
        const int actIdx = m_activePreset->findData(cfg.activePreset);
        m_activePreset->setCurrentIndex(actIdx >= 0 ? actIdx : 0);
        const int inactIdx = m_inactivePreset->findData(cfg.inactivePreset);
        m_inactivePreset->setCurrentIndex(inactIdx >= 0 ? inactIdx : 1);

        m_activeTitleText->setCurrentIndex(m_activeTitleText->findData(cfg.activeTitleText));
        m_inactiveTitleText->setCurrentIndex(m_inactiveTitleText->findData(cfg.inactiveTitleText));
        m_tintSlider->setValue(qRound(cfg.tint * 100.0f));
        m_inactiveTintSlider->setValue(qRound(cfg.inactiveTint * 100.0f));
        m_opacitySlider->setValue(qRound(cfg.frameOpacity * 100.0f));
        m_refractionSlider->setValue(qRound(cfg.refraction * 100.0f));
        m_blurSlider->setValue(cfg.blurStrength);
        m_dispersionSlider->setValue(qRound(cfg.dispersion * 100.0f));
        m_specularSlider->setValue(qRound(cfg.specular * 100.0f));
        m_adaptiveSlider->setValue(qRound(cfg.adaptiveContrast * 100.0f));
        m_hoverGlowSlider->setValue(qRound(cfg.hoverGlow * 100.0f));
        m_shaderCheck->setChecked(cfg.shaderEffectEnabled);

        m_titleHeightSpin->setValue(cfg.titleHeight);
        m_borderWidthSpin->setValue(cfg.borderWidth);
        m_cornerRadiusSpin->setValue(cfg.cornerRadius);
        m_titleAlignCombo->setCurrentIndex(std::clamp(cfg.titleAlignment, 0, 2));
        m_useSystemButtonsCheck->setChecked(cfg.useSystemButtons);

        updateButtonSwatches();
        m_preview->setConfig(cfg);
        m_updating = false;
        updateOpticsAvailability();
    }

    void m_WorkingFromWidgets()
    {
        m_working.activePreset = m_activePreset->currentData().toString();
        m_working.inactivePreset = m_inactivePreset->currentData().toString();
        m_working.activeTitleText = m_activeTitleText->currentData().toInt();
        m_working.inactiveTitleText = m_inactiveTitleText->currentData().toInt();
        m_working.tint = m_tintSlider->value() / 100.0f;
        m_working.inactiveTint = m_inactiveTintSlider->value() / 100.0f;
        m_working.frameOpacity = m_opacitySlider->value() / 100.0f;
        m_working.refraction = m_refractionSlider->value() / 100.0f;
        m_working.blurStrength = m_blurSlider->value();
        m_working.dispersion = m_dispersionSlider->value() / 100.0f;
        m_working.specular = m_specularSlider->value() / 100.0f;
        m_working.adaptiveContrast = m_adaptiveSlider->value() / 100.0f;
        m_working.hoverGlow = m_hoverGlowSlider->value() / 100.0f;
        m_working.shaderEffectEnabled = m_shaderCheck->isChecked();
        m_working.titleHeight = m_titleHeightSpin->value();
        m_working.borderWidth = m_borderWidthSpin->value();
        m_working.cornerRadius = m_cornerRadiusSpin->value();
        m_working.titleAlignment = m_titleAlignCombo->currentData().toInt();
        m_working.useSystemButtons = m_useSystemButtonsCheck->isChecked();
    }

    void onUiEdited()
    {
        if (m_updating) {
            return;
        }
        m_WorkingFromWidgets();
        updateButtonSwatches();
        m_preview->setConfig(m_working);
        updateOpticsAvailability();
        setNeedsSave(m_working != m_saved);
    }

    MaterialConfig m_saved;
    MaterialConfig m_working;
    bool m_updating = false;
    bool m_effectLoaded = false;
    bool m_effectQueryPending = false;
    QGroupBox *m_opticsGroup = nullptr;
    QLabel *m_effectStatus = nullptr;

    MaterialPreview *m_preview = nullptr;
    QComboBox *m_activePreset = nullptr;
    QComboBox *m_inactivePreset = nullptr;
    QPushButton *m_activeColorBtn = nullptr;
    QPushButton *m_inactiveColorBtn = nullptr;
    QComboBox *m_activeTitleText = nullptr;
    QComboBox *m_inactiveTitleText = nullptr;
    QSlider *m_tintSlider = nullptr;
    QSlider *m_inactiveTintSlider = nullptr;
    QSlider *m_opacitySlider = nullptr;
    QSlider *m_refractionSlider = nullptr;
    QSlider *m_blurSlider = nullptr;
    QSlider *m_dispersionSlider = nullptr;
    QSlider *m_specularSlider = nullptr;
    QSlider *m_adaptiveSlider = nullptr;
    QSlider *m_hoverGlowSlider = nullptr;
    QCheckBox *m_shaderCheck = nullptr;
    QSpinBox *m_titleHeightSpin = nullptr;
    QSpinBox *m_borderWidthSpin = nullptr;
    QSpinBox *m_cornerRadiusSpin = nullptr;
    QComboBox *m_titleAlignCombo = nullptr;
    QCheckBox *m_useSystemButtonsCheck = nullptr;
};

K_PLUGIN_CLASS_WITH_JSON(EmeraldGlassConfigModule, "kcm.json")

} // namespace EmeraldGlass

#include "kcm.moc"
