// SPDX-License-Identifier: LGPL-3.0-or-later
#include "../native/materialpreview.h"
#include <QApplication>
#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

int runSheetComparison(QApplication &app)
{
    QWidget host;
    host.setWindowTitle(QStringLiteral("Liquid Glass — previous / sheet material"));
    auto *layout = new QVBoxLayout(&host);
    layout->addWidget(new QLabel(QStringLiteral("Previous material — active / inactive")));
    auto *current = new EmeraldGlass::MaterialPreview(&host, QStringLiteral(PREVIEW_SHADER_DIR "/experiments/channel-glass.frag"));
    layout->addWidget(current);
    layout->addWidget(new QLabel(QStringLiteral("Sheet material — active / inactive")));
    auto *sheet = new EmeraldGlass::MaterialPreview(&host);
    layout->addWidget(sheet);
    EmeraldGlass::MaterialConfig cfg;
    cfg.activePreset = QStringLiteral("emerald");
    cfg.inactivePreset = QStringLiteral("peach_rose");
    cfg.refraction = 1; cfg.dispersion = .2f; cfg.specular = .65f;
    cfg.tint = cfg.inactiveTint = .35f; cfg.frameOpacity = .35f; cfg.blurStrength = 2;
    auto apply = [&] { current->setConfig(cfg); sheet->setConfig(cfg); };
    current->setEffectAvailable(true); sheet->setEffectAvailable(true);
    auto addSlider = [&](const QString &name, int maximum, int value, auto change) {
        auto *row = new QHBoxLayout;
        auto *label = new QLabel(name); label->setMinimumWidth(150); row->addWidget(label);
        auto *slider = new QSlider(Qt::Horizontal); slider->setRange(0,maximum); slider->setValue(value); row->addWidget(slider);
        auto *number = new QLabel(QString::number(value)); number->setMinimumWidth(35); row->addWidget(number);
        QObject::connect(slider,&QSlider::valueChanged,&host,[&,number,change](int v) {
            number->setText(QString::number(v)); change(v); apply();
        });
        layout->addLayout(row);
    };
    addSlider(QStringLiteral("Refraction %"),180,100,[&](int v){ cfg.refraction=v/100.f; });
    addSlider(QStringLiteral("Color separation %"),100,20,[&](int v){ cfg.dispersion=v/100.f; });
    addSlider(QStringLiteral("Edge light %"),150,65,[&](int v){ cfg.specular=v/100.f; });
    addSlider(QStringLiteral("Base opacity %"),100,35,[&](int v){ cfg.frameOpacity=v/100.f; });
    addSlider(QStringLiteral("Tint %"),200,35,[&](int v){ cfg.tint=cfg.inactiveTint=v/100.f; });
    addSlider(QStringLiteral("Blurriness"),15,2,[&](int v){ cfg.blurStrength=v; });
    auto *optics = new QCheckBox(QStringLiteral("Liquid Glass optics")); optics->setChecked(true); layout->addWidget(optics);
    QObject::connect(optics,&QCheckBox::toggled,&host,[&](bool v){ cfg.shaderEffectEnabled=v; apply(); });
    apply(); host.resize(1040,650); host.show();
    const auto args=app.arguments(); const int capture=args.indexOf(QStringLiteral("--capture"));
    if(capture>=0 && capture+1<args.size()) {
        QTimer::singleShot(350,&host,[&] {
            if(current->grabFramebuffer().isNull() || sheet->grabFramebuffer().isNull()) { app.exit(1); return; }
            app.exit(host.grab().save(args[capture+1]) ? 0 : 1);
        });
    }
    return app.exec();
}
