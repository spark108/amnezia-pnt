#pragma once

#include <QCryptographicHash>
#include <QNetworkAccessManager>
#include <QObject>
#include <QSaveFile>
#include <memory>

#include "core/utils/pntRelease.h"

class PntUpdateController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool ready READ ready NOTIFY changed)
    Q_PROPERTY(int progress READ progress NOTIFY changed)

public:
    explicit PntUpdateController(QObject *parent = nullptr);
    QString currentVersion() const;
    QString latestVersion() const
    {
        return m_release ? m_release->version : QString();
    }
    QString status() const
    {
        return m_status;
    }
    bool busy() const
    {
        return m_busy;
    }
    bool available() const
    {
        return m_release.has_value();
    }
    bool ready() const
    {
        return m_ready;
    }
    int progress() const
    {
        return m_progress;
    }
    void setVpnActive(bool active)
    {
        m_vpnActive = active;
    }

    Q_INVOKABLE void check();
    Q_INVOKABLE void download();
    Q_INVOKABLE void install();

signals:
    void changed();

private:
    void fail(const QString &message);
    QString assetName() const;

    QNetworkAccessManager m_network;
    std::optional<PntRelease> m_release;
    std::unique_ptr<QSaveFile> m_file;
    QCryptographicHash m_hash { QCryptographicHash::Sha256 };
    QString m_path;
    QString m_status;
    qint64 m_received = 0;
    int m_progress = 0;
    bool m_busy = false;
    bool m_ready = false;
    bool m_vpnActive = false;
};
