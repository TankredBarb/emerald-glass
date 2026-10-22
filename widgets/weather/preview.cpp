// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QGuiApplication>
#include <QQmlContext>
#include <QQuickView>
#include <QTimer>
#include <QUrl>

int main(int argc, char **argv)
{
    QGuiApplication app(argc, argv);
    QQuickView view;
    view.setTitle(QStringLiteral("Emerald Weather — Preview"));
    view.setResizeMode(QQuickView::SizeRootObjectToView);
    auto args = app.arguments();
    const bool extended = args.removeAll(QStringLiteral("--extended")) > 0;
    const bool met = args.removeAll(QStringLiteral("--met")) > 0;
    const bool location = args.removeAll(QStringLiteral("--location-settings")) > 0;
    const bool text = args.removeAll(QStringLiteral("--text-settings")) > 0;
    const bool effects = args.removeAll(QStringLiteral("--effects-settings")) > 0;
    view.rootContext()->setContextProperty(QStringLiteral("previewWallpaper"), args.size() > 1 ? QUrl::fromLocalFile(args.at(1)) : QUrl());
    view.rootContext()->setContextProperty(QStringLiteral("previewProvider"), met ? 1 : 0);
    view.rootContext()->setContextProperty(QStringLiteral("previewExpanded"), extended);
    // Keep the local backend import relative to the packaged UI directory.
    const QString ui = QStringLiteral(WEATHER_UI);
    const QString source = location ? ui + "/ConfigLocation.qml" : text ? ui + "/ConfigText.qml"
        : effects ? ui + "/ConfigEffects.qml" : ui + "/Preview.qml";
    view.setSource(QUrl::fromLocalFile(source));
    if (view.status() == QQuickView::Error) return 1;
    if (location || text || effects) view.resize(620, 660);
    view.show();
    if (args.size() > 2) {
        QTimer::singleShot(12000, &app, [&] { app.exit(view.grabWindow().save(args.at(2)) ? 0 : 2); });
    }
    return app.exec();
}
