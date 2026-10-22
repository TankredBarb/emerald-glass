// SPDX-License-Identifier: LGPL-3.0-or-later
#include "weatherbackend.h"
#include <QQmlExtensionPlugin>
#include <qqml.h>

class WeatherPlugin : public QQmlExtensionPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID QQmlExtensionInterface_iid)
public:
    void registerTypes(const char *uri) override
    {
        qmlRegisterType<WeatherBackend>(uri, 1, 0, "WeatherBackend");
    }
};
#include "plugin.moc"
