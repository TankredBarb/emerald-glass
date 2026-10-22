// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QTimer>
#include <QUrl>
#include <QQmlEngine>
#include <KLocalizedQmlContext>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQuickView view;
    auto args = app.arguments();
    const bool sensorsMode = args.removeAll(QStringLiteral("--sensor-settings")) > 0;
    const bool settingsMode = args.removeAll(QStringLiteral("--settings")) > 0;
    const bool textMode = args.removeAll(QStringLiteral("--text-settings")) > 0;
    view.setTitle(QStringLiteral("Emerald System Monitor — Preview"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    KLocalization::setupLocalizedContext(view.engine());
    view.rootContext()->setContextProperty(QStringLiteral("monitorUi"),
        QUrl::fromLocalFile(QStringLiteral(MONITOR_PACKAGE_UI)).toString());
    view.rootContext()->setContextProperty(QStringLiteral("previewWallpaper"),
        args.size() > 1 ? QUrl::fromLocalFile(args.at(1)) : QUrl());
    view.setSource(QUrl::fromLocalFile(sensorsMode ? QStringLiteral(MONITOR_PACKAGE_UI "/ConfigSensors.qml")
        : textMode
        ? QStringLiteral(MONITOR_PACKAGE_UI "/ConfigText.qml")
        : settingsMode ? QStringLiteral(MONITOR_PACKAGE_UI "/ConfigEffects.qml")
                       : QStringLiteral(MONITOR_PREVIEW_QML)));
    if (view.status() == QQuickView::Error) return 1;
    if (settingsMode || textMode || sensorsMode) view.resize(620, 660);
    view.show();
    if (args.size() > 2) {
        QTimer::singleShot(6500, &app, [&] {
            const bool saved = view.grabWindow().save(args.at(2));
            app.exit(saved ? 0 : 2);
        });
    }
    return app.exec();
}
