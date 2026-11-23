// SPDX-License-Identifier: LGPL-3.0-or-later
#include "../native/materialpreview.h"
#include <QApplication>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>
#include <array>

int runCloseComparison(QApplication &app)
{
    QWidget host;
    host.setWindowTitle(QStringLiteral("Emerald Glass — Close material comparison"));
    auto *layout = new QVBoxLayout(&host);
    layout->addWidget(new QLabel(QStringLiteral("Close: identical settings and background in every row · active on the left, inactive on the right · buttons 27×23")));
    auto cfg = EmeraldGlass::loadMaterialConfig();
    cfg.shaderEffectEnabled = true;
    cfg.useSystemButtons = false;
    const auto args = app.arguments();
    const int presetArg = args.indexOf(QStringLiteral("--preset"));
    const QString initialPreset = presetArg >= 0 && presetArg + 1 < args.size() ? args[presetArg + 1] : cfg.activePreset;
    auto *controls = new QHBoxLayout;
    auto *preset = new QComboBox;
    for (const auto &color : EmeraldGlass::kColorPresets)
        preset->addItem(QString::fromLatin1(color.name), QString::fromLatin1(color.id));
    preset->setCurrentIndex(std::max(0, preset->findData(initialPreset)));
    controls->addWidget(new QLabel(QStringLiteral("Theme")));
    controls->addWidget(preset);
    auto *state = new QComboBox;
    state->addItem(QStringLiteral("Mouse: hover"), -1);
    state->addItem(QStringLiteral("Idle"), 0);
    state->addItem(QStringLiteral("Hover Close"), 1);
    state->addItem(QStringLiteral("Press Close"), 2);
    const int stateArg = args.indexOf(QStringLiteral("--state"));
    const int initialState = stateArg >= 0 && stateArg + 1 < args.size() ? args[stateArg + 1].toInt() : 1;
    state->setCurrentIndex(std::max(0, state->findData(initialState)));
    controls->addWidget(new QLabel(QStringLiteral("State")));
    controls->addWidget(state);
    controls->addStretch();
    layout->addLayout(controls);
    std::array<EmeraldGlass::MaterialPreview *, 4> previews;
    const std::array<const char *, 4> labels = {{
        "0 · Previous — dense fill, subdued highlights",
        "1 · Soft glass — 25% fill, light reflections",
        "2 · Rich glass — 45% fill, light reflections",
        "3 · Selected — colored depth, theme-colored rim and reflections"
    }};
    for (int i = 0; i < int(previews.size()); ++i) {
        layout->addWidget(new QLabel(QString::fromUtf8(labels[i])));
        previews[i] = new EmeraldGlass::MaterialPreview(&host, {}, i);
        previews[i]->setFixedHeight(130);
        previews[i]->setEffectAvailable(true);
        layout->addWidget(previews[i]);
    }
    auto apply = [&] {
        cfg.activePreset = cfg.inactivePreset = preset->currentData().toString();
        cfg.inactiveTint = cfg.tint;
        for (auto *preview : previews) {
            preview->setConfig(cfg);
            preview->setClosePreviewState(state->currentData().toInt());
        }
    };
    QObject::connect(preset, &QComboBox::currentIndexChanged, &host, apply);
    QObject::connect(state, &QComboBox::currentIndexChanged, &host, apply);
    auto addSlider = [&](const QString &name, int maximum, float &setting) {
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(name));
        auto *slider = new QSlider(Qt::Horizontal);
        slider->setRange(0, maximum);
        slider->setValue(qRound(setting * 100));
        row->addWidget(slider);
        auto *number = new QLabel(QString::number(slider->value()));
        number->setMinimumWidth(35);
        row->addWidget(number);
        QObject::connect(slider, &QSlider::valueChanged, &host, [&, number](int value) {
            setting = value / 100.f;
            number->setText(QString::number(value));
            apply();
        });
        layout->addLayout(row);
    };
    addSlider(QStringLiteral("Specular %"), 150, cfg.specular);
    addSlider(QStringLiteral("Hover glow %"), 150, cfg.hoverGlow);
    addSlider(QStringLiteral("Opacity %"), 100, cfg.frameOpacity);
    apply();
    host.resize(1040, 830);
    host.show();
    const int capture = args.indexOf(QStringLiteral("--capture"));
    if (capture >= 0 && capture + 1 < args.size()) {
        QTimer::singleShot(500, &host, [&] {
            EmeraldGlass::PreviewFrame probe;
            probe.frame = QRectF(0, 0, 80, 80);
            for (auto *preview : previews) {
                if (preview->renderDecorationFrame(QSizeF(80, 80), 1, probe).isNull()) {
                    qWarning("Close comparison requires a working OpenGL context; CPU fallback cannot show these variants.");
                    app.exit(1);
                    return;
                }
            }
            app.exit(host.grab().save(args[capture + 1]) ? 0 : 1);
        });
    }
    return app.exec();
}
