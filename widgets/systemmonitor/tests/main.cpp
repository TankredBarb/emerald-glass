// SPDX-License-Identifier: LGPL-3.0-or-later
#include <QtQuickTest/quicktest.h>
#include <QFile>
#include <QFont>
#include <QQmlContext>
#include <QQmlEngine>
#include <QTemporaryDir>
#include <QStandardPaths>
#include <KConfigLoader>
#include <KLocalizedQmlContext>

class TextTestSetup : public QObject {
    Q_OBJECT
public:
    Q_INVOKABLE QVariantMap roundTripSettings(const QStringList &ids, int columns, const QString &labels)
    {
        QTemporaryDir directory;
        const auto path = directory.filePath(QStringLiteral("sensors"));
        {
            QFile schema(QStringLiteral(MONITOR_CONFIG_XML));
            KConfigLoader config(path, &schema);
            config.findItem("Sensors", "sensorIds")->setProperty(ids);
            config.findItem("Sensors", "sensorColumns")->setProperty(columns);
            config.findItem("Sensors", "sensorLabels")->setProperty(labels);
            config.save();
        }
        QFile schema(QStringLiteral(MONITOR_CONFIG_XML));
        KConfigLoader restored(path, &schema);
        return {{"ids", restored.findItem("Sensors", "sensorIds")->property()},
                {"columns", restored.findItem("Sensors", "sensorColumns")->property()},
                {"labels", restored.findItem("Sensors", "sensorLabels")->property()}};
    }
public Q_SLOTS:
    void qmlEngineAvailable(QQmlEngine *engine)
    {
        engine->rootContext()->setContextProperty(QStringLiteral("sensorConfig"), this);
        engine->rootContext()->setContextProperty(QStringLiteral("desktopOverlayUrl"),
            QUrl::fromLocalFile(QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                QStringLiteral("plasma/plasmoids/org.kde.desktopcontainment/contents/ui/ConfigOverlay.qml"))));
        KLocalization::setupLocalizedContext(engine);
        QFile file(QStringLiteral(MONITOR_CONFIG_XML));
        if (!file.exists()) qFatal("Cannot read the widget configuration schema");
        QTemporaryDir directory;
        KConfigLoader config(directory.filePath(QStringLiteral("defaults")), &file);
        QVariantMap values;
        for (const auto *item : config.items()) values.insert(item->name(), item->property());
        engine->rootContext()->setContextProperty(QStringLiteral("schemaValues"), values);
    }
};

QUICK_TEST_MAIN_WITH_SETUP(emerald_monitor, TextTestSetup)
#include "main.moc"
