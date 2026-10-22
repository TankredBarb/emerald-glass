// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include <QDateTime>
#include <QJsonObject>
#include <QTimeZone>
#include <QVariant>

namespace WeatherData {
struct Snapshot {
    QVariantMap current;
    QVariantList hours;
    QVariantList days;
    QDateTime forecastTime;
    bool valid() const { return forecastTime.isValid() && current.contains(QStringLiteral("temperatureText")); }
};
Snapshot parseOpenMeteo(const QJsonObject &json, const QDateTime &now, const QTimeZone &zone);
Snapshot parseMetNorway(const QJsonObject &json, const QDateTime &now, const QTimeZone &zone);
QVariantList parseLocations(const QJsonObject &json);
}
