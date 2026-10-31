#pragma once

#include <QElapsedTimer>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QUrl>
#include <QVariantList>

#include "connectionController.h"
#include "core/repositories/secureServersRepository.h"
#include "secureQSettings.h"
#include "selfhosted/importController.h"

class PntController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool hasProfile READ hasProfile NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString selectedServer READ selectedServer NOTIFY changed)
    Q_PROPERTY(QVariantList servers READ servers NOTIFY changed)
    Q_PROPERTY(QString vpnAddress READ vpnAddress NOTIFY changed)
    Q_PROPERTY(QString externalAddress READ externalAddress NOTIFY changed)
    Q_PROPERTY(QString handshake READ handshake NOTIFY changed)
    Q_PROPERTY(QString connectionMode READ connectionMode NOTIFY changed)
    Q_PROPERTY(QString ping READ ping NOTIFY changed)
    Q_PROPERTY(QString received READ received NOTIFY changed)
    Q_PROPERTY(QString sent READ sent NOTIFY changed)
    Q_PROPERTY(QString receiveSpeed READ receiveSpeed NOTIFY changed)
    Q_PROPERTY(QString sendSpeed READ sendSpeed NOTIFY changed)

public:
    PntController(SecureQSettings *settings, SecureServersRepository *servers, SecureAppSettingsRepository *appSettings,
                  ImportController *importer, ConnectionController *connection, QObject *parent = nullptr);
    ~PntController() override;
    bool hasProfile() const;
    bool busy() const;
    QString status() const;
    QString selectedServer() const;
    QVariantList servers() const;
    QString vpnAddress() const
    {
        return m_vpnAddress;
    }
    QString externalAddress() const
    {
        return m_externalAddress;
    }
    QString handshake() const;
    QString connectionMode() const;
    QString ping() const;
    QString received() const;
    QString sent() const;
    QString receiveSpeed() const;
    QString sendSpeed() const;
    Q_INVOKABLE void importProfile(const QUrl &file);
    Q_INVOKABLE void loadServers();
    Q_INVOKABLE void connectServer(const QString &id);
    Q_INVOKABLE void disconnectVpn();
    Q_INVOKABLE void resetProfile();

signals:
    void changed();
    void serversReady();

private:
    void start(const QString &action, const QString &server = { });
    void readOutput();
    void acceptResult(const QJsonObject &result);
    void stopAgent();
    void fail(const QString &message);
    void refreshStatistics();
    void refreshExternalAddress();
    void refreshPing();
    void refreshCatalogPings();

    SecureQSettings *m_settings;
    SecureServersRepository *m_servers;
    SecureAppSettingsRepository *m_appSettings;
    ImportController *m_importer;
    ConnectionController *m_connection;
    QProcess m_process;
    QByteArray m_output;
    QJsonObject m_profile;
    QJsonObject m_pendingProfile;
    QVariantList m_catalog;
    quint64 m_catalogGeneration = 0;
    QString m_action;
    QString m_status;
    QString m_selectedServer;
    QString m_sessionServer;
    bool m_busy = false;
    bool m_sessionStarted = false;
    quint64 m_generation = 0;
    QString m_currentServer;
    QString m_connectionMode;
    QString m_vpnAddress;
    QString m_externalAddress;
    qint64 m_handshake = 0;
    quint64 m_received = 0, m_sent = 0, m_previousReceived = 0, m_previousSent = 0;
    quint64 m_receiveSpeed = 0, m_sendSpeed = 0;
    QElapsedTimer m_sampleTime;
    QTimer m_statsTimer;
    QNetworkAccessManager m_network;
    int m_statsTick = 0;
    bool m_statsPending = false;
    bool m_ipPending = false;
    bool m_pingPending = false;
    QString m_serverHost;
    qint64 m_serverPing = -1, m_internetPing = -1;
};
