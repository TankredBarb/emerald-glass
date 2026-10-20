// SPDX-License-Identifier: LGPL-3.0-or-later
#include "materialpreview.h"
#include <QApplication>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv)
{
    QTemporaryDir config;
    if (!config.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    QApplication app(argc, argv);
    EmeraldGlass::MaterialPreview preview(nullptr, QStringLiteral(":/missing-review-shader.frag"));
    preview.resize(700, 180);
    preview.setEffectAvailable(true);
    EmeraldGlass::PreviewFrame frame;
    frame.frame = QRectF(0, 0, 350, 180);
    frame.content = QRectF(8, 39, 334, 133);
    for (int attempt = 0; attempt < 3; ++attempt) {
        if (!preview.renderDecorationFrame(QSizeF(350, 180), 1, frame).isNull()) {
            std::cerr << "Failed shader incorrectly reported a ready card material\n";
            return 1;
        }
        if (preview.grabFramebuffer().isNull()) {
            std::cerr << "KCM lost its CPU preview after a graphics failure\n";
            return 1;
        }
    }
    EmeraldGlass::MaterialConfig cfg;
    cfg.titleAlignment = 1;
    preview.setConfig(cfg);
    const auto betweenButtons = preview.grabFramebuffer();
    cfg.titleAlignment = 2;
    preview.setConfig(cfg);
    if (preview.grabFramebuffer() == betweenButtons) return 1;
    cfg.useSystemButtons = true;
    preview.setConfig(cfg);
    if (preview.grabFramebuffer() == betweenButtons) return 1;
    std::cout << "PASS: graphics failure preserves native card and CPU KCM fallback\n";
}
