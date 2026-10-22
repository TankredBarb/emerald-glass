// SPDX-License-Identifier: LGPL-3.0-or-later
#include "weatherbackend.h"
#include "weatherdata.h"
#include <QJsonArray>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QTest>
#include <QNetworkReply>
#include <cstring>
#include <QUrlQuery>

class ForecastReply : public QNetworkReply {
public:
    explicit ForecastReply(QObject *parent) : QNetworkReply(parent) { open(QIODevice::ReadOnly); }
    void abort() override { finish({}, QNetworkReply::OperationCanceledError); }
    void finish(QByteArray body, QNetworkReply::NetworkError error = QNetworkReply::NoError,
                const QByteArray &expires = {}, const QByteArray &retryAfter = {})
    {
        m_body = std::move(body);
        if (!expires.isEmpty()) setRawHeader("Expires", expires);
        if (!retryAfter.isEmpty()) setRawHeader("Retry-After", retryAfter);
        if (error != QNetworkReply::NoError) setError(error, "Test network failure");
        setFinished(true);
        Q_EMIT finished();
    }
    qint64 bytesAvailable() const override { return m_body.size() - m_offset + QNetworkReply::bytesAvailable(); }
protected:
    qint64 readData(char *buffer, qint64 size) override
    {
        const auto count = std::min<qint64>(size, m_body.size() - m_offset);
        if (count <= 0) return -1;
        std::memcpy(buffer, m_body.constData() + m_offset, count);
        m_offset += count;
        return count;
    }
private:
    QByteArray m_body;
    qint64 m_offset = 0;
};
class TestBackend : public WeatherBackend {
public:
    int requests = 0;
    QPointer<ForecastReply> reply;
protected:
    QNetworkReply *requestForecast() override { ++requests; reply = new ForecastReply(this); return reply; }
};

class WeatherTest : public QObject {
    Q_OBJECT
    QTemporaryDir m_cache;
    QByteArray m_oldCache;
    static QByteArray forecast()
    {
        return QJsonDocument(QJsonObject{{"current", QJsonObject{
            {"time", QDateTime::currentDateTimeUtc().toSecsSinceEpoch()}, {"temperature_2m", 0}}}}).toJson();
    }
private Q_SLOTS:
    void initTestCase()
    {
        QVERIFY(m_cache.isValid());
        m_oldCache = qgetenv("XDG_CACHE_HOME");
        qputenv("XDG_CACHE_HOME", m_cache.path().toUtf8());
    }
    void init() { QDir(m_cache.path() + "/emerald-glass").removeRecursively(); }
    void cleanupTestCase()
    {
        if (m_oldCache.isNull()) qunsetenv("XDG_CACHE_HOME");
        else qputenv("XDG_CACHE_HOME", m_oldCache);
    }
    void automaticRefreshUsesRealTimerAndRetriesAfterFailure()
    {
        TestBackend backend;
        backend.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        QCOMPARE(backend.requests, 1);
        backend.reply->finish(forecast());
        QVERIFY(backend.hasData());
        QVERIFY(backend.m_refreshTimer.isActive());
        QVERIFY(backend.m_refreshTimer.remainingTime() > 29 * 60000);
        // Simulate elapsed time: the real timer must initiate the next request.
        backend.m_updated = QDateTime::currentDateTimeUtc().addSecs(-31 * 60);
        backend.m_nextAllowed = QDateTime::currentDateTimeUtc().addSecs(-1);
        backend.m_refreshTimer.start(1);
        QTRY_COMPARE(backend.requests, 2);
        backend.reply->finish({}, QNetworkReply::TemporaryNetworkFailureError);
        QVERIFY(backend.hasData());
        QVERIFY(backend.stale());
        QVERIFY(backend.m_refreshTimer.remainingTime() > 4 * 60000);
        QVERIFY(backend.m_refreshTimer.remainingTime() <= 5 * 60000);
        backend.m_retryAt = QDateTime::currentDateTimeUtc().addSecs(-1);
        backend.m_nextAllowed = QDateTime::currentDateTimeUtc().addSecs(-1);
        backend.checkRefresh(); // Same path as the resume/wall-clock watchdog.
        QCOMPARE(backend.requests, 3);
        backend.reply->finish(forecast());
        QVERIFY(!backend.stale());
        QVERIFY(backend.error().isEmpty());
    }
    void expiresDefersUntilAllowedInsteadOfSkippingWholeInterval()
    {
        TestBackend backend;
        backend.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 15);
        const auto expires = QDateTime::currentDateTimeUtc().addSecs(20 * 60);
        backend.reply->finish(forecast(), QNetworkReply::NoError, QLocale::c().toString(expires, "ddd, dd MMM yyyy HH:mm:ss 'GMT'").toLatin1());
        QCOMPARE(backend.automaticRefreshTime().toSecsSinceEpoch(), expires.toSecsSinceEpoch());
        backend.m_updated = QDateTime::currentDateTimeUtc().addSecs(-16 * 60);
        backend.checkRefresh();
        QCOMPARE(backend.requests, 1);
        QVERIFY(backend.m_refreshTimer.isActive());
        backend.m_nextAllowed = QDateTime::currentDateTimeUtc().addSecs(-1);
        backend.checkRefresh();
        QCOMPARE(backend.requests, 2);
    }
    void retryAfterOnErrorAndShorterIntervalAreHonored()
    {
        TestBackend backend;
        backend.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        backend.reply->finish({}, QNetworkReply::ServiceUnavailableError, {}, "900");
        QVERIFY(backend.m_refreshTimer.remainingTime() > 14 * 60000);
        backend.m_nextAllowed = QDateTime::currentDateTimeUtc().addSecs(-1);
        backend.m_retryAt = QDateTime::currentDateTimeUtc().addSecs(-1);
        backend.checkRefresh();
        backend.reply->finish(forecast());
        backend.m_updated = QDateTime::currentDateTimeUtc().addSecs(-20 * 60);
        backend.m_nextAllowed = QDateTime::currentDateTimeUtc().addSecs(-1);
        backend.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 15);
        QCOMPARE(backend.requests, 3);
    }
    void retryAfterHttpDateIsHonored()
    {
        TestBackend backend;
        backend.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        const auto retry = QDateTime::currentDateTimeUtc().addSecs(20 * 60);
        const auto header = QLocale::c().toString(retry, "ddd, dd MMM yyyy HH:mm:ss 'GMT'").toLatin1();
        backend.reply->finish({}, QNetworkReply::ServiceUnavailableError, {}, header);
        QCOMPARE(backend.automaticRefreshTime().toSecsSinceEpoch(), retry.toSecsSinceEpoch());
    }
    void cachedForecastKeepsOriginalRefreshDeadline()
    {
        TestBackend first;
        first.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        first.reply->finish(forecast());
        first.m_updated = QDateTime::currentDateTimeUtc().addSecs(-20 * 60);
        first.saveCache();
        TestBackend restored;
        restored.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        QVERIFY(restored.hasData());
        QCOMPARE(restored.requests, 0);
        QVERIFY(restored.m_refreshTimer.remainingTime() > 9 * 60000);
        QVERIFY(restored.m_refreshTimer.remainingTime() <= 10 * 60000);
        restored.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        QVERIFY(restored.m_refreshTimer.remainingTime() <= 10 * 60000);
        QCOMPARE(restored.requests, 0);
    }
    void retryAfterAndErrorSurviveRestart()
    {
        TestBackend first;
        first.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        first.reply->finish(forecast());
        first.m_nextAllowed = QDateTime::currentDateTimeUtc().addSecs(-1);
        first.refresh();
        first.reply->finish({}, QNetworkReply::ServiceUnavailableError, {}, "3600");
        TestBackend restored;
        restored.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        QCOMPARE(restored.requests, 0);
        QVERIFY(restored.hasData());
        QVERIFY(!restored.canRefresh());
        QVERIFY(restored.stale());
        QCOMPARE(restored.error(), first.error());
        QCOMPARE(restored.automaticRefreshTime(), first.automaticRefreshTime());
    }
    void retrySurvivesRestartWithoutForecast()
    {
        TestBackend first;
        first.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        first.reply->finish({}, QNetworkReply::TemporaryNetworkFailureError);
        TestBackend restored;
        restored.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        QCOMPARE(restored.requests, 0);
        QVERIFY(!restored.hasData());
        QVERIFY(!restored.loading());
        QCOMPARE(restored.automaticRefreshTime(), first.automaticRefreshTime());
        QVERIFY(!restored.error().isEmpty());
    }
    void expiredForecastDoesNotDiscardProviderRestriction()
    {
        TestBackend first;
        first.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        first.reply->finish(forecast());
        first.m_updated = QDateTime::currentDateTimeUtc().addDays(-2);
        first.m_nextAllowed = QDateTime::currentDateTimeUtc().addSecs(3600);
        first.saveCache();
        TestBackend restored;
        restored.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        QCOMPARE(restored.requests, 0);
        QVERIFY(!restored.hasData());
        QVERIFY(!restored.canRefresh());
        QCOMPARE(restored.m_nextAllowed, first.m_nextAllowed);
    }
    void rejectsIncompleteForecasts()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        QVERIFY(!WeatherData::parseOpenMeteo({}, now, QTimeZone("Europe/Kyiv")).valid());
        QVERIFY(!WeatherData::parseMetNorway({}, now, QTimeZone("Europe/Kyiv")).valid());
    }
    void bothProvidersAcceptCoordinatesOutsideStationLists()
    {
        const auto open = WeatherBackend::forecastUrl(0, 46.48572, 30.74383);
        const auto met = WeatherBackend::forecastUrl(1, 46.48572, 30.74383);
        QCOMPARE(QUrlQuery(open).queryItemValue("latitude"), QStringLiteral("46.4857"));
        QCOMPARE(QUrlQuery(met).queryItemValue("lat"), QStringLiteral("46.4857"));
        QCOMPARE(QUrlQuery(open).queryItemValue("forecast_days"), QStringLiteral("16"));
        QCOMPARE(open.scheme(), QStringLiteral("https"));
        QCOMPARE(met.scheme(), QStringLiteral("https"));
    }
    void rejectsInvalidConfigurationWithoutNetwork()
    {
        WeatherBackend backend;
        backend.configure(0, 100, 30, "Europe/Kyiv", 30);
        QVERIFY(!backend.loading());
        QVERIFY(!backend.hasData());
        QVERIFY(!backend.error().isEmpty());
        backend.configure(0, 46, 30, "Not/AZone", 30);
        QVERIFY(!backend.canRefresh());
    }
    void invalidConfigurationClearsPreviousFetchState()
    {
        TestBackend backend;
        backend.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        backend.reply->finish(forecast());
        QVERIFY(!backend.updateText().isEmpty());
        QVERIFY(!backend.nextRefreshText().isEmpty());
        backend.configure(0, 100, 30, "Europe/Kyiv", 30);
        QVERIFY(!backend.hasData());
        QVERIFY(backend.updateText().isEmpty());
        QVERIFY(backend.forecastText().isEmpty());
        QVERIFY(backend.nextRefreshText().isEmpty());
        QVERIFY(backend.automaticRefreshText().isEmpty());
        QVERIFY(!backend.canRefresh());
        QVERIFY(!backend.loading());
    }
    void openMeteoKeepsZeroAndMissingValuesDistinct()
    {
        const auto now = QDateTime::fromString("2026-10-21T16:00:00Z", Qt::ISODate);
        const auto json = QJsonDocument::fromJson(R"({
            "current":{"time":1792598400,"temperature_2m":0,"weather_code":0,"is_day":0},
            "hourly":{"time":[1792594800,1792598400,1792602000,1792609200],"temperature_2m":[-1,0,1,2]},
            "daily":{"time":[1792530000],"temperature_2m_min":[0],"temperature_2m_max":[5]}
        })").object();
        const auto result = WeatherData::parseOpenMeteo(json, now, QTimeZone("Europe/Kyiv"));
        QVERIFY(result.valid());
        QCOMPARE(result.current.value("temperatureText").toString(), QStringLiteral("0 °C"));
        QCOMPARE(result.current.value("windText").toString(), QStringLiteral("—"));
        QCOMPARE(result.current.value("icon").toString(), QStringLiteral("weather-clear-night"));
        QCOMPARE(result.hours.size(), 2);
        QCOMPARE(result.hours.first().toMap().value("timeText").toString(), QStringLiteral("19:00"));
        QCOMPARE(result.hours.last().toMap().value("timeText").toString(), QStringLiteral("22:00"));
        QCOMPARE(result.days.size(), 1);
        QCOMPARE(result.days.first().toMap().value("rainText").toString(), QStringLiteral("—"));
    }
    void metNorwayDoesNotDoubleCountOverlappingPrecipitation()
    {
        const auto now = QDateTime::fromString("2026-10-21T09:00:00Z", Qt::ISODate);
        QJsonArray series;
        for (int hour : {9, 12, 15}) {
            series.append(QJsonObject{{"time", QString("2026-10-21T%1:00:00Z").arg(hour, 2, 10, QChar('0'))},
                {"data", QJsonObject{
                    {"instant", QJsonObject{{"details", QJsonObject{{"air_temperature", hour - 9}, {"wind_speed", 0}}}}},
                    {"next_6_hours", QJsonObject{{"summary", QJsonObject{{"symbol_code", "rain"}}},
                        {"details", QJsonObject{{"precipitation_amount", 2}}}}}}}});
        }
        const auto result = WeatherData::parseMetNorway(QJsonObject{{"properties", QJsonObject{{"timeseries", series}}}}, now, QTimeZone("Europe/Kyiv"));
        QVERIFY(result.valid());
        QCOMPARE(result.hours.size(), 3);
        QCOMPARE(result.hours.first().toMap().value("timeText").toString(), QStringLiteral("12:00"));
        QCOMPARE(result.current.value("temperatureText").toString(), QStringLiteral("0 °C"));
        QVERIFY(result.current.value("feelsText").toString().isEmpty());
        QCOMPARE(result.current.value("humidityText").toString(), QStringLiteral("—"));
        QCOMPARE(result.days.size(), 1);
        QCOMPARE(result.days.first().toMap().value("highText").toString(), QStringLiteral("6°"));
        QCOMPARE(result.days.first().toMap().value("rainText").toString(), QLocale().toString(4.0, 'f', 1) + " mm");
    }
    void bothProvidersRetainExtendedDaysAndWind()
    {
        const auto now = QDateTime::fromString("2026-10-21T00:00:00Z", Qt::ISODate);
        QJsonArray dates, lows, highs, winds, series;
        for (int i = 0; i < 16; ++i) {
            dates.append(now.addDays(i).toSecsSinceEpoch());
            lows.append(0); highs.append(10); winds.append(3);
            if (i < 10) series.append(QJsonObject{{"time", now.addDays(i).toString(Qt::ISODate)},
                {"data", QJsonObject{{"instant", QJsonObject{{"details", QJsonObject{{"air_temperature", 0}, {"wind_speed", 3}}}}}}}});
        }
        const QJsonObject open{{"current", QJsonObject{{"time", now.toSecsSinceEpoch()}, {"temperature_2m", 0}}},
            {"daily", QJsonObject{{"time", dates}, {"temperature_2m_min", lows}, {"temperature_2m_max", highs}, {"wind_speed_10m_max", winds}}}};
        const auto parsed = WeatherData::parseOpenMeteo(open, now, QTimeZone("Europe/Kyiv"));
        QCOMPARE(parsed.days.size(), 16);
        QVERIFY(parsed.days.last().toMap().value("dateText").toString().contains("05.11"));
        QCOMPARE(parsed.days.last().toMap().value("windText").toString(), QLocale().toString(3.0, 'f', 1) + " m/s");
        const auto met = WeatherData::parseMetNorway(QJsonObject{{"properties", QJsonObject{{"timeseries", series}}}}, now, QTimeZone("Europe/Kyiv"));
        QCOMPARE(met.days.size(), 10);
        QCOMPARE(met.days.last().toMap().value("windText").toString(), QLocale().toString(3.0, 'f', 1) + " m/s");
    }
    void openMeteoDailyDatesRemainUniqueAcrossDstChange()
    {
        const auto now = QDateTime::fromString("2026-10-24T00:00:00Z", Qt::ISODate);
        const auto start = QDateTime::fromString("2026-10-23T21:00:00Z", Qt::ISODate);
        QJsonArray dates;
        for (int i = 0; i < 3; ++i) dates.append(start.addDays(i).toSecsSinceEpoch());
        const QJsonObject json{{"utc_offset_seconds", 10800}, {"daily", QJsonObject{
            {"time", dates}, {"temperature_2m_min", QJsonArray{0, 0, 0}}, {"temperature_2m_max", QJsonArray{10, 10, 10}}}}};
        const auto parsed = WeatherData::parseOpenMeteo(json, now, QTimeZone("Europe/Kyiv"));
        QCOMPARE(parsed.days.size(), 3);
        QVERIFY(parsed.days[0].toMap().value("dateText").toString().contains("24.10"));
        QVERIFY(parsed.days[1].toMap().value("dateText").toString().contains("25.10"));
        QVERIFY(parsed.days[2].toMap().value("dateText").toString().contains("26.10"));
    }
    void cacheIsSeparatedByProviderAndCoordinates()
    {
        QTemporaryDir cache;
        QVERIFY(cache.isValid());
        const auto oldCache = qgetenv("XDG_CACHE_HOME");
        qputenv("XDG_CACHE_HOME", cache.path().toUtf8());
        const auto now = QDateTime::currentDateTimeUtc();
        const QString directory = cache.path() + "/emerald-glass/weather";
        QVERIFY(QDir().mkpath(directory));
        for (int i = 0; i < 3; ++i) {
            const auto identity = QString("%1/%2/30.7438/Europe/Kyiv").arg(i == 1 ? 1 : 0).arg(i == 2 ? "45.0000" : "46.4857");
            const auto key = QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex();
            QFile file(directory + '/' + QString::fromLatin1(key) + ".json");
            QVERIFY(file.open(QIODevice::WriteOnly));
            const QJsonObject json{{"version", 3}, {"updated", now.toString(Qt::ISODate)},
                {"nextAllowed", now.addSecs(600).toString(Qt::ISODate)},
                {"forecastTime", now.toString(Qt::ISODate)},
                {"current", QJsonObject{{"temperatureText", QString::number(i)}}}};
            file.write(QJsonDocument(json).toJson());
        }
        WeatherBackend backend;
        backend.configure(0, 46.48572, 30.74383, "Europe/Kyiv", 30);
        QVERIFY(backend.hasData());
        QVERIFY(!backend.loading());
        QVERIFY(!backend.canRefresh());
        QCOMPARE(backend.current().value("temperatureText").toString(), QStringLiteral("0"));
        backend.configure(1, 46.48572, 30.74383, "Europe/Kyiv", 30);
        QCOMPARE(backend.current().value("temperatureText").toString(), QStringLiteral("1"));
        backend.configure(0, 45, 30.74383, "Europe/Kyiv", 30);
        QCOMPARE(backend.current().value("temperatureText").toString(), QStringLiteral("2"));
        backend.configure(0, 100, 30, "Europe/Kyiv", 30);
        QVERIFY(!backend.hasData());
        if (oldCache.isNull()) qunsetenv("XDG_CACHE_HOME");
        else qputenv("XDG_CACHE_HOME", oldCache);
    }
    void geocodingRetainsCountryRegionAndCoordinates()
    {
        const auto json = QJsonDocument::fromJson(R"({"results":[{"name":"Odesa","admin1":"Odesa Oblast","country":"Ukraine","latitude":46.48572,"longitude":30.74383,"timezone":"Europe/Kyiv"}]})").object();
        const auto locations = WeatherData::parseLocations(json);
        QCOMPARE(locations.size(), 1);
        const auto location = locations.first().toMap();
        QVERIFY(location.value("label").toString().contains("Ukraine"));
        QCOMPARE(location.value("timeZone").toString(), QStringLiteral("Europe/Kyiv"));
    }
};
QTEST_GUILESS_MAIN(WeatherTest)
#include "weather_data_test.moc"
