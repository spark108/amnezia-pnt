#pragma once

#include <QJsonObject>
#include <QObject>
#include <QProcess>
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

public:
    PntController(SecureQSettings *settings, SecureServersRepository *servers, SecureAppSettingsRepository *appSettings,
                  ImportController *importer, ConnectionController *connection, QObject *parent = nullptr);
    ~PntController() override;
    bool hasProfile() const;
    bool busy() const;
    QString status() const;
    QString selectedServer() const;
    QVariantList servers() const;
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
    QString m_action;
    QString m_status;
    QString m_selectedServer;
    QString m_sessionServer;
    bool m_busy = false;
    bool m_sessionStarted = false;
};
