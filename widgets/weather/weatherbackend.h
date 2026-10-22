// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once
#include "weatherdata.h"
#include <QNetworkAccessManager>
#include <QPointer>
#include <QTimer>
#include <QUrl>

class QNetworkReply;
class WeatherBackend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantList hours READ hours NOTIFY changed)
    Q_PROPERTY(QVariantList days READ days NOTIFY changed)
    Q_PROPERTY(bool hasData READ hasData NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool stale READ stale NOTIFY changed)
    Q_PROPERTY(bool canRefresh READ canRefresh NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
    Q_PROPERTY(QString updateText READ updateText NOTIFY changed)
    Q_PROPERTY(QString forecastText READ forecastText NOTIFY changed)
    Q_PROPERTY(QString nextRefreshText READ nextRefreshText NOTIFY changed)
    Q_PROPERTY(QString automaticRefreshText READ automaticRefreshText NOTIFY changed)
    Q_PROPERTY(QString providerName READ providerName NOTIFY changed)
    Q_PROPERTY(QString providerUrl READ providerUrl NOTIFY changed)
    Q_PROPERTY(QVariantList locations READ locations NOTIFY searchChanged)
    Q_PROPERTY(bool searching READ searching NOTIFY searchChanged)
    Q_PROPERTY(QString searchError READ searchError NOTIFY searchChanged)
public:
    explicit WeatherBackend(QObject *parent = nullptr);
    ~WeatherBackend() override;
    QVariantMap current() const { return m_data.current; }
    QVariantList hours() const { return m_data.hours; }
    QVariantList days() const { return m_data.days; }
    bool hasData() const { return m_data.valid(); }
    bool loading() const { return m_loading; }
    bool stale() const;
    bool canRefresh() const;
    QString error() const { return m_error; }
    QString updateText() const;
    QString forecastText() const;
    QString nextRefreshText() const;
    QString automaticRefreshText() const;
    QString providerName() const;
    QString providerUrl() const;
    QVariantList locations() const { return m_locations; }
    bool searching() const { return m_searching; }
    QString searchError() const { return m_searchError; }
    Q_INVOKABLE void configure(int provider, double latitude, double longitude, const QString &timeZone, int refreshMinutes);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void searchLocations(const QString &query);
    static QUrl forecastUrl(int provider, double latitude, double longitude);
Q_SIGNALS:
    void changed();
    void searchChanged();
protected:
    virtual QNetworkReply *requestForecast();
private:
    friend class WeatherTest;
    void scheduleRefresh();
    void checkRefresh();
    QDateTime automaticRefreshTime() const;
    void loadCache();
    void saveCache() const;
    QString cachePath() const;
    QNetworkRequest request(const QUrl &url) const;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QPointer<QNetworkReply> m_searchReply;
    QTimer m_refreshTimer;
    QTimer m_clockTimer;
    WeatherData::Snapshot m_data;
    QDateTime m_updated;
    QDateTime m_nextAllowed;
    QDateTime m_retryAt;
    QTimeZone m_zone = QTimeZone("Europe/Kyiv");
    int m_provider = 0;
    double m_latitude = 0;
    double m_longitude = 0;
    int m_refreshMinutes = 30;
    bool m_configured = false;
    bool m_loading = false;
    bool m_searching = false;
    QString m_error;
    QString m_searchError;
    QVariantList m_locations;
    quint64 m_generation = 0;
    quint64 m_searchGeneration = 0;
};
