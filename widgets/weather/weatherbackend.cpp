// SPDX-License-Identifier: LGPL-3.0-or-later
#include "weatherbackend.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFileInfo>
#include <QLocale>
#include <QNetworkReply>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrlQuery>
#include <cmath>
#include <algorithm>

namespace {
QDateTime httpDate(const QByteArray &header)
{
    auto date = QDateTime::fromString(QString::fromLatin1(header), Qt::RFC2822Date);
    if (!date.isValid()) {
        // HTTP uses the literal GMT suffix, which Qt's RFC2822 parser rejects.
        date = QLocale::c().toDateTime(QString::fromLatin1(header), "ddd, dd MMM yyyy HH:mm:ss 'GMT'");
        date.setTimeZone(QTimeZone::UTC);
    }
    return date.toUTC();
}
}

WeatherBackend::WeatherBackend(QObject *parent) : QObject(parent)
{
    m_network.setTransferTimeout(20000);
    m_refreshTimer.setSingleShot(true);
    m_refreshTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_refreshTimer, &QTimer::timeout, this, &WeatherBackend::checkRefresh);
    connect(&m_clockTimer, &QTimer::timeout, this, [this] {
        checkRefresh(); // Also catches overdue updates after suspend or a wall-clock change.
        Q_EMIT changed();
    });
    m_clockTimer.start(30000);
}

WeatherBackend::~WeatherBackend()
{
    // Replies belong to m_network; disconnect before its member destructor runs.
    if (m_reply) disconnect(m_reply, nullptr, this, nullptr);
    if (m_searchReply) disconnect(m_searchReply, nullptr, this, nullptr);
}

bool WeatherBackend::stale() const
{
    return hasData() && (!m_error.isEmpty() || m_updated.secsTo(QDateTime::currentDateTimeUtc()) > m_refreshMinutes * 60 + 60);
}
bool WeatherBackend::canRefresh() const
{
    return m_configured && !m_loading && QDateTime::currentDateTimeUtc() >= m_nextAllowed;
}
QString WeatherBackend::updateText() const
{
    return m_updated.isValid() ? tr("Updated %1").arg(m_updated.toTimeZone(m_zone).toString("dd.MM HH:mm")) : QString();
}
QString WeatherBackend::forecastText() const
{
    return hasData() ? tr("Forecast for %1").arg(m_data.forecastTime.toTimeZone(m_zone).toString("dd.MM HH:mm")) : QString();
}
QString WeatherBackend::nextRefreshText() const
{
    return m_nextAllowed.isValid() ? m_nextAllowed.toTimeZone(m_zone).toString("HH:mm") : QString();
}
QDateTime WeatherBackend::automaticRefreshTime() const
{
    const auto due = m_retryAt.isValid() ? m_retryAt
        : (m_updated.isValid() ? m_updated.addSecs(m_refreshMinutes * 60) : QDateTime::fromSecsSinceEpoch(0));
    return m_nextAllowed.isValid() && m_nextAllowed > due ? m_nextAllowed : due;
}
QString WeatherBackend::automaticRefreshText() const
{
    return m_configured && !m_loading ? tr("Auto refresh %1").arg(automaticRefreshTime().toTimeZone(m_zone).toString("HH:mm")) : QString();
}
void WeatherBackend::scheduleRefresh()
{
    m_refreshTimer.stop();
    if (!m_configured || m_loading) return;
    const auto delay = QDateTime::currentDateTimeUtc().msecsTo(automaticRefreshTime());
    m_refreshTimer.start(int(std::clamp<qint64>(delay, 1, 24 * 3600 * 1000)));
}
void WeatherBackend::checkRefresh()
{
    if (!m_configured || m_loading) return;
    if (QDateTime::currentDateTimeUtc() >= automaticRefreshTime()) refresh();
    else scheduleRefresh();
}
QNetworkReply *WeatherBackend::requestForecast()
{
    return m_network.get(request(forecastUrl(m_provider, m_latitude, m_longitude)));
}

QString WeatherBackend::providerName() const
{
    return m_provider == 0 ? QStringLiteral("Open-Meteo") : QStringLiteral("MET Norway");
}
QString WeatherBackend::providerUrl() const
{
    return m_provider == 0 ? QStringLiteral("https://open-meteo.com/") : QStringLiteral("https://api.met.no/");
}

QNetworkRequest WeatherBackend::request(const QUrl &url) const
{
    QNetworkRequest result(url);
    result.setHeader(QNetworkRequest::UserAgentHeader,
        QStringLiteral("EmeraldGlassWeather/0.2 (https://github.com/TankredBarb/emerald-glass)"));
    result.setRawHeader("Accept", "application/json");
    result.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    return result;
}

QUrl WeatherBackend::forecastUrl(int provider, double latitude, double longitude)
{
    QUrl url(provider == 0 ? QStringLiteral("https://api.open-meteo.com/v1/forecast")
                          : QStringLiteral("https://api.met.no/weatherapi/locationforecast/2.0/compact"));
    QUrlQuery query;
    query.addQueryItem(provider == 0 ? "latitude" : "lat", QString::number(latitude, 'f', 4));
    query.addQueryItem(provider == 0 ? "longitude" : "lon", QString::number(longitude, 'f', 4));
    if (provider == 0) {
        query.addQueryItem("current", "temperature_2m,apparent_temperature,relative_humidity_2m,weather_code,is_day,wind_speed_10m,pressure_msl");
        query.addQueryItem("hourly", "temperature_2m,weather_code,is_day");
        query.addQueryItem("daily", "weather_code,temperature_2m_max,temperature_2m_min,precipitation_sum,wind_speed_10m_max");
        query.addQueryItem("wind_speed_unit", "ms");
        query.addQueryItem("timeformat", "unixtime");
        query.addQueryItem("timezone", "auto");
        query.addQueryItem("forecast_days", "16");
    }
    url.setQuery(query);
    return url;
}

void WeatherBackend::configure(int provider, double latitude, double longitude, const QString &timeZone, int refreshMinutes)
{
    const QTimeZone zone(timeZone.toUtf8());
    if (provider < 0 || provider > 1 || !std::isfinite(latitude) || !std::isfinite(longitude) ||
        latitude < -90 || latitude > 90 || longitude < -180 || longitude > 180 || !zone.isValid()) {
        ++m_generation;
        if (m_reply) m_reply->abort();
        m_refreshTimer.stop();
        m_configured = false;
        m_loading = false;
        m_data = {};
        m_updated = {};
        m_nextAllowed = {};
        m_retryAt = {};
        m_error = tr("Choose valid coordinates and a time zone in the widget settings.");
        Q_EMIT changed();
        return;
    }
    refreshMinutes = std::clamp(refreshMinutes, 15, 120);
    const bool sameLocation = m_configured && provider == m_provider && latitude == m_latitude &&
        longitude == m_longitude && zone == m_zone;
    m_refreshMinutes = refreshMinutes;
    if (sameLocation) {
        checkRefresh();
        Q_EMIT changed();
        return;
    }
    ++m_generation;
    if (m_reply) m_reply->abort();
    m_reply = nullptr;
    m_provider = provider;
    m_latitude = latitude;
    m_longitude = longitude;
    m_zone = zone;
    m_configured = true;
    m_loading = false;
    m_error.clear();
    m_data = {};
    m_updated = {};
    m_nextAllowed = {};
    m_retryAt = {};
    loadCache();
    Q_EMIT changed();
    checkRefresh();
}

void WeatherBackend::refresh()
{
    if (!canRefresh()) {
        scheduleRefresh();
        return;
    }
    m_refreshTimer.stop();
    m_loading = true;
    m_error.clear();
    Q_EMIT changed();
    const auto generation = m_generation;
    auto *reply = requestForecast();
    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (generation != m_generation) return;
        m_reply = nullptr;
        m_loading = false;
        const auto now = QDateTime::currentDateTimeUtc();
        m_nextAllowed = now.addSecs(60);
        const auto expires = httpDate(reply->rawHeader("Expires"));
        if (expires > m_nextAllowed) m_nextAllowed = expires;
        const auto retryHeader = reply->rawHeader("Retry-After");
        bool secondsValid = false;
        const auto seconds = retryHeader.toLongLong(&secondsValid);
        const auto retryAfter = secondsValid ? now.addSecs(std::max<qint64>(0, seconds))
            : httpDate(retryHeader);
        if (retryAfter > m_nextAllowed) m_nextAllowed = retryAfter;
        m_retryAt = now.addSecs(5 * 60);
        if (reply->error() != QNetworkReply::NoError) {
            m_error = tr("Weather request failed: %1").arg(reply->errorString());
        } else {
            const auto json = QJsonDocument::fromJson(reply->readAll());
            auto data = m_provider == 0 ? WeatherData::parseOpenMeteo(json.object(), now, m_zone)
                                        : WeatherData::parseMetNorway(json.object(), now, m_zone);
            if (!data.valid()) {
                m_error = tr("The provider returned an incomplete forecast.");
            } else {
                m_data = std::move(data);
                m_updated = now;
                m_retryAt = {};
            }
        }
        saveCache();
        scheduleRefresh();
        Q_EMIT changed();
    });
}

void WeatherBackend::searchLocations(const QString &query)
{
    ++m_searchGeneration;
    if (m_searchReply) m_searchReply->abort();
    m_searchReply = nullptr;
    m_searchError.clear();
    m_locations.clear();
    if (query.trimmed().size() < 2) {
        m_searching = false;
        m_searchError = tr("Enter at least two characters.");
        Q_EMIT searchChanged();
        return;
    }
    m_searching = true;
    Q_EMIT searchChanged();
    QUrl url(QStringLiteral("https://geocoding-api.open-meteo.com/v1/search"));
    QUrlQuery parameters;
    parameters.addQueryItem("name", query.trimmed());
    parameters.addQueryItem("count", "10");
    parameters.addQueryItem("language", QLocale().name().left(2));
    url.setQuery(parameters);
    const auto generation = m_searchGeneration;
    auto *reply = m_network.get(request(url));
    m_searchReply = reply;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation] {
        reply->deleteLater();
        if (generation != m_searchGeneration) return;
        m_searchReply = nullptr;
        m_searching = false;
        if (reply->error() != QNetworkReply::NoError) {
            m_searchError = tr("Location search failed: %1").arg(reply->errorString());
        } else {
            const auto json = QJsonDocument::fromJson(reply->readAll());
            m_locations = WeatherData::parseLocations(json.object());
            if (m_locations.isEmpty()) m_searchError = tr("No locations found. Try another spelling or enter coordinates.");
        }
        Q_EMIT searchChanged();
    });
}

QString WeatherBackend::cachePath() const
{
    const auto identity = QStringLiteral("%1/%2/%3/%4").arg(m_provider).arg(m_latitude, 0, 'f', 4)
        .arg(m_longitude, 0, 'f', 4).arg(QString::fromUtf8(m_zone.id()));
    const auto key = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex();
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/emerald-glass/weather/") + QString::fromLatin1(key) + QStringLiteral(".json");
}
void WeatherBackend::saveCache() const
{
    const auto path = cachePath();
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) return;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;
    const QJsonObject json{{"version", 3}, {"updated", m_updated.toString(Qt::ISODateWithMs)},
        {"nextAllowed", m_nextAllowed.toString(Qt::ISODateWithMs)}, {"retryAt", m_retryAt.toString(Qt::ISODateWithMs)},
        {"error", m_error}, {"forecastTime", m_data.forecastTime.toString(Qt::ISODateWithMs)},
        {"current", QJsonObject::fromVariantMap(m_data.current)}, {"hours", QJsonArray::fromVariantList(m_data.hours)},
        {"days", QJsonArray::fromVariantList(m_data.days)}};
    file.write(QJsonDocument(json).toJson(QJsonDocument::Compact));
    file.commit();
}
void WeatherBackend::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto json = QJsonDocument::fromJson(file.readAll()).object();
    if (json.value("version").toInt() != 3) return;
    // Request restrictions survive even without a usable forecast snapshot.
    m_nextAllowed = QDateTime::fromString(json.value("nextAllowed").toString(), Qt::ISODate);
    m_retryAt = QDateTime::fromString(json.value("retryAt").toString(), Qt::ISODate);
    m_error = json.value("error").toString();
    const auto updated = QDateTime::fromString(json.value("updated").toString(), Qt::ISODate);
    const auto age = updated.secsTo(QDateTime::currentDateTimeUtc());
    if (!updated.isValid() || age < 0 || age > 24 * 3600) return;
    WeatherData::Snapshot data;
    data.current = json.value("current").toObject().toVariantMap();
    data.hours = json.value("hours").toArray().toVariantList();
    data.days = json.value("days").toArray().toVariantList();
    data.forecastTime = QDateTime::fromString(json.value("forecastTime").toString(), Qt::ISODate);
    if (!data.valid()) return;
    m_data = std::move(data);
    m_updated = updated;
}
