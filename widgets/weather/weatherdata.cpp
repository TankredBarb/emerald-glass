// SPDX-License-Identifier: LGPL-3.0-or-later
#include "weatherdata.h"
#include <QJsonArray>
#include <QLocale>
#include <QMap>
#include <QObject>
#include <algorithm>
#include <cmath>
#include <limits>

namespace {
QJsonValue at(const QJsonArray &array, qsizetype index)
{
    return index >= 0 && index < array.size() ? array[index] : QJsonValue();
}
QString number(const QJsonValue &value, const QString &suffix, int precision = 0)
{
    if (!value.isDouble() || !std::isfinite(value.toDouble())) return QStringLiteral("—");
    return QLocale().toString(value.toDouble(), 'f', precision) + suffix;
}
struct Condition { QString text; QString icon; int severity = 0; };
Condition wmo(int code, bool day = true)
{
    if (code == 0) return {QObject::tr("Clear"), day ? "weather-clear" : "weather-clear-night", 0};
    if (code == 1 || code == 2) return {QObject::tr("Partly cloudy"), day ? "weather-few-clouds" : "weather-few-clouds-night", 1};
    if (code == 3) return {QObject::tr("Overcast"), "weather-overcast", 2};
    if (code == 45 || code == 48) return {QObject::tr("Fog"), "weather-fog", 3};
    if (code >= 51 && code <= 57) return {QObject::tr("Drizzle"), "weather-showers-scattered", 4};
    if ((code >= 61 && code <= 67) || (code >= 80 && code <= 82)) return {QObject::tr("Rain"), "weather-showers", 4};
    if ((code >= 71 && code <= 77) || code == 85 || code == 86) return {QObject::tr("Snow"), "weather-snow", 5};
    if (code >= 95 && code <= 99) return {QObject::tr("Thunderstorm"), "weather-storm", 6};
    return {QObject::tr("Unknown conditions"), "weather-none-available", -1};
}
Condition metCondition(const QString &symbol)
{
    if (symbol.contains("thunder")) return wmo(95);
    if (symbol.contains("snow") || symbol.contains("sleet")) return wmo(73);
    if (symbol.contains("rain")) return wmo(63);
    if (symbol.contains("fog")) return wmo(45);
    if (symbol.startsWith("partlycloudy") || symbol.startsWith("fair")) return wmo(2, !symbol.endsWith("_night"));
    if (symbol.startsWith("cloudy")) return wmo(3);
    if (symbol.startsWith("clearsky")) return wmo(0, !symbol.endsWith("_night"));
    return wmo(-1);
}
QVariantMap forecastRow(const QDateTime &time, const QJsonValue &temperature, const Condition &condition)
{
    return {{"timeText", time.toString("HH:mm")}, {"temperatureText", number(temperature, QStringLiteral("°"))},
            {"icon", condition.icon}, {"condition", condition.text}};
}
void setCurrent(WeatherData::Snapshot &result, const QJsonValue &temperature, const QJsonValue &wind,
                const QJsonValue &humidity, const QJsonValue &pressure, const Condition &condition)
{
    if (!temperature.isDouble() || !std::isfinite(temperature.toDouble())) return;
    result.current = {{"temperatureText", number(temperature, QStringLiteral(" °C"))},
                      {"windText", number(wind, QObject::tr(" m/s"), 1)},
                      {"humidityText", number(humidity, QStringLiteral("%"))},
                      {"pressureText", number(pressure, QObject::tr(" hPa"))},
                      {"condition", condition.text}, {"icon", condition.icon}, {"feelsText", QString()}};
}
QDateTime unixTime(const QJsonValue &value, const QTimeZone &zone)
{
    return value.isDouble() ? QDateTime::fromSecsSinceEpoch(value.toInteger(), zone) : QDateTime();
}
}

WeatherData::Snapshot WeatherData::parseOpenMeteo(const QJsonObject &json, const QDateTime &now, const QTimeZone &zone)
{
    Snapshot result;
    const auto current = json.value("current").toObject();
    result.forecastTime = unixTime(current.value("time"), zone);
    setCurrent(result, current.value("temperature_2m"), current.value("wind_speed_10m"),
               current.value("relative_humidity_2m"), current.value("pressure_msl"),
               wmo(current.value("weather_code").toInt(-1), current.value("is_day").toInt(1) != 0));
    if (current.value("apparent_temperature").isDouble())
        result.current["feelsText"] = number(current.value("apparent_temperature"), QStringLiteral(" °C"));
    const auto hourly = json.value("hourly").toObject();
    const auto times = hourly.value("time").toArray();
    const auto temperatures = hourly.value("temperature_2m").toArray();
    const auto codes = hourly.value("weather_code").toArray();
    const auto daylight = hourly.value("is_day").toArray();
    QDateTime last;
    for (qsizetype i = 0; i < times.size() && result.hours.size() < 6; ++i) {
        const auto time = unixTime(times[i], zone);
        if (!time.isValid() || time < now || (last.isValid() && last.secsTo(time) < 3 * 3600)) continue;
        if (i >= temperatures.size() || !temperatures[i].isDouble()) continue;
        result.hours.append(forecastRow(time, temperatures[i], wmo(at(codes, i).toInt(-1), at(daylight, i).toInt(1) != 0)));
        last = time;
    }
    const auto daily = json.value("daily").toObject();
    const auto dates = daily.value("time").toArray();
    const auto highs = daily.value("temperature_2m_max").toArray();
    const auto lows = daily.value("temperature_2m_min").toArray();
    const auto dailyCodes = daily.value("weather_code").toArray();
    const auto rain = daily.value("precipitation_sum").toArray();
    const auto wind = daily.value("wind_speed_10m_max").toArray();
    for (qsizetype i = 0; i < dates.size() && result.days.size() < 16; ++i) {
        // Open-Meteo daily Unix timestamps use the response's fixed UTC offset,
        // including across DST boundaries; applying a changing zone duplicates dates.
        const int offset = json.value("utc_offset_seconds").toInt(zone.offsetFromUtc(now));
        const auto date = unixTime(dates[i], QTimeZone::UTC).addSecs(offset).date();
        if (!date.isValid() || date < now.toTimeZone(zone).date()) continue;
        if (i >= highs.size() || i >= lows.size() || !highs[i].isDouble() || !lows[i].isDouble()) continue;
        const auto condition = wmo(at(dailyCodes, i).toInt(-1));
        result.days.append(QVariantMap{{"dateText", QLocale().toString(date, "ddd dd.MM")}, {"dayText", QLocale().toString(date, "ddd")}, {"icon", condition.icon},
            {"condition", condition.text}, {"highText", number(highs[i], QStringLiteral("°"))},
            {"lowText", number(lows[i], QStringLiteral("°"))},
            {"rainText", number(at(rain, i), QObject::tr(" mm"), 1)},
            {"windText", number(at(wind, i), QObject::tr(" m/s"), 1)}});
    }
    return result;
}

WeatherData::Snapshot WeatherData::parseMetNorway(const QJsonObject &json, const QDateTime &now, const QTimeZone &zone)
{
    Snapshot result;
    const auto series = json.value("properties").toObject().value("timeseries").toArray();
    struct Day {
        double low = std::numeric_limits<double>::infinity();
        double high = -std::numeric_limits<double>::infinity();
        double rain = 0;
        bool hasRain = false;
        double wind = -1;
        Condition condition = wmo(-1);
    };
    QMap<QDate, Day> days;
    QDateTime lastHour;
    QDateTime rainUntil;
    for (const auto &entry : series) {
        const auto item = entry.toObject();
        const auto time = QDateTime::fromString(item.value("time").toString(), Qt::ISODate).toTimeZone(zone);
        if (!time.isValid() || time < now.addSecs(-3600)) continue;
        const auto data = item.value("data").toObject();
        const auto instant = data.value("instant").toObject().value("details").toObject();
        const auto temperature = instant.value("air_temperature");
        if (!temperature.isDouble() || !std::isfinite(temperature.toDouble())) continue;
        const bool shortPeriod = data.contains("next_1_hours");
        const int periodHours = shortPeriod ? 1 : (data.contains("next_6_hours") ? 6 : 12);
        const auto period = data.value(QStringLiteral("next_%1_hours").arg(periodHours)).toObject();
        const auto condition = metCondition(period.value("summary").toObject().value("symbol_code").toString());
        if (!result.valid()) {
            result.forecastTime = time;
            setCurrent(result, temperature, instant.value("wind_speed"), instant.value("relative_humidity"),
                       instant.value("air_pressure_at_sea_level"), condition);
        }
        if (time >= now && result.hours.size() < 6 && (!lastHour.isValid() || lastHour.secsTo(time) >= 3 * 3600)) {
            result.hours.append(forecastRow(time, temperature, condition));
            lastHour = time;
        }
        const auto date = time.date();
        if (date < now.toTimeZone(zone).date() || date > now.toTimeZone(zone).date().addDays(10)) continue;
        auto &day = days[date];
        day.low = std::min(day.low, temperature.toDouble());
        day.high = std::max(day.high, temperature.toDouble());
        if (instant.value("wind_speed").isDouble()) day.wind = std::max(day.wind, instant.value("wind_speed").toDouble());
        if (condition.severity > day.condition.severity) day.condition = condition;
        const auto rain = period.value("details").toObject().value("precipitation_amount");
        if (rain.isDouble() && (!rainUntil.isValid() || time >= rainUntil)) {
            day.rain += rain.toDouble();
            day.hasRain = true;
            rainUntil = time.addSecs(periodHours * 3600);
        }
    }
    for (auto it = days.cbegin(); it != days.cend(); ++it) {
        const auto &day = it.value();
        result.days.append(QVariantMap{{"dateText", QLocale().toString(it.key(), "ddd dd.MM")}, {"dayText", QLocale().toString(it.key(), "ddd")}, {"icon", day.condition.icon},
            {"condition", day.condition.text}, {"highText", number(day.high, QStringLiteral("°"))},
            {"lowText", number(day.low, QStringLiteral("°"))},
            {"rainText", day.hasRain ? number(day.rain, QObject::tr(" mm"), 1) : QStringLiteral("—")},
            {"windText", day.wind >= 0 ? number(day.wind, QObject::tr(" m/s"), 1) : QStringLiteral("—")}});
    }
    return result;
}

QVariantList WeatherData::parseLocations(const QJsonObject &json)
{
    QVariantList results;
    for (const auto &value : json.value("results").toArray()) {
        const auto item = value.toObject();
        if (!item.value("latitude").isDouble() || !item.value("longitude").isDouble() || item.value("name").toString().isEmpty()) continue;
        QStringList label{item.value("name").toString()};
        const auto region = item.value("admin1").toString();
        if (!region.isEmpty() && region != label.first()) label.append(region);
        if (!item.value("country").toString().isEmpty()) label.append(item.value("country").toString());
        results.append(QVariantMap{{"label", label.join(QStringLiteral(", "))}, {"name", item.value("name").toString()},
            {"latitude", item.value("latitude").toDouble()}, {"longitude", item.value("longitude").toDouble()},
            {"timeZone", item.value("timezone").toString()}});
    }
    return results;
}
