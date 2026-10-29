#include "pntController.h"
#include "core/utils/pntLatency.h"

#include "ui/controllers/systemController.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFutureWatcher>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocale>
#include <QNetworkReply>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>
#include <algorithm>
#ifdef Q_OS_ANDROID
    #include <QJniObject>
#endif

PntController::PntController(SecureQSettings *settings, SecureServersRepository *servers,
                             SecureAppSettingsRepository *appSettings, ImportController *importer,
                             ConnectionController *connection, QObject *parent)
    : QObject(parent),
      m_settings(settings),
      m_servers(servers),
      m_appSettings(appSettings),
      m_importer(importer),
      m_connection(connection)
{
    m_profile = QJsonDocument::fromJson(m_settings->value("Pnt/profile").toByteArray()).object();
    m_currentServer = m_settings->value("Pnt/serverId").toString();
    m_selectedServer = m_settings->value("Pnt/serverName").toString();
    m_vpnAddress = m_settings->value("Pnt/vpnAddress").toString();
    m_serverHost = m_settings->value("Pnt/serverHost").toString();
    m_appSettings->setUseAmneziaDns(false);
    m_appSettings->setPrimaryDns("1.1.1.1");
    m_appSettings->setSecondaryDns("1.0.0.1");
    connect(m_connection, &ConnectionController::bytesChanged, this, [this](quint64 received, quint64 sent) {
        m_received += received;
        m_sent += sent;
    });
    m_statsTimer.setInterval(1000);
    connect(&m_statsTimer, &QTimer::timeout, this, [this]() {
        const auto elapsed = qMax<qint64>(1, m_sampleTime.restart());
        m_receiveSpeed = (m_received >= m_previousReceived ? m_received - m_previousReceived : 0) * 1000 / elapsed;
        m_sendSpeed = (m_sent >= m_previousSent ? m_sent - m_previousSent : 0) * 1000 / elapsed;
        m_previousReceived = m_received;
        m_previousSent = m_sent;
        if (++m_statsTick % 15 == 0)
            refreshStatistics();
        if (m_statsTick % 30 == 0)
            refreshExternalAddress();
        if (m_statsTick % 5 == 0)
            refreshPing();
        emit changed();
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, &PntController::readOutput);
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart)
            fail(tr("Файл транспорта PNT отсутствует или не запускается."));
    });
    connect(&m_process, &QProcess::finished, this, [this](int code, QProcess::ExitStatus) {
        readOutput();
        if (m_action == "connect" && m_sessionStarted) {
            m_connection->closeConnection();
            m_sessionStarted = false;
            if (code != 0)
                m_status = tr("Соединение транспорта PNT прервано.");
        }
        m_busy = false;
        emit changed();
    });
    connect(m_connection, &ConnectionController::connectionStateChanged, this, [this](Vpn::ConnectionState state) {
        if (state == Vpn::Connected) {
            m_sampleTime.start();
            m_statsTimer.start();
            refreshStatistics();
            refreshPing();
            QTimer::singleShot(1000, this, &PntController::refreshExternalAddress);
        } else {
            m_statsTimer.stop();
            m_receiveSpeed = m_sendSpeed = 0;
        }
#ifdef Q_OS_ANDROID
        if (hasProfile() && state == Vpn::Connected) {
            m_sessionStarted = true;
            m_status = tr("Подключено");
            emit changed();
        }
#endif
        if (m_sessionStarted && (state == Vpn::Disconnected || state == Vpn::Error)) {
            m_sessionStarted = false;
            stopAgent();
            m_status = state == Vpn::Error ? tr("Ошибка VPN-соединения.") : tr("Отключено");
            emit changed();
        }
    });
}

PntController::~PntController()
{
    stopAgent();
}
bool PntController::hasProfile() const
{
    return !m_profile.isEmpty();
}
bool PntController::busy() const
{
    return m_busy;
}
QString PntController::status() const
{
    return m_status;
}
QString PntController::selectedServer() const
{
    return m_selectedServer;
}
QVariantList PntController::servers() const
{
    return m_catalog;
}

QString PntController::handshake() const
{
    return m_handshake > 0 ? tr("%1 сек. назад").arg(qMax<qint64>(0, QDateTime::currentSecsSinceEpoch() - m_handshake))
                           : tr("Ожидание");
}

QString PntController::ping() const
{
    const auto format = [](qint64 value) { return value < 0 ? QString::fromUtf8("—") : QString::number(value) + "ms"; };
    return format(m_serverPing) + "/" + format(m_internetPing);
}

void PntController::refreshPing()
{
    if (!m_connection->isConnected() || m_pingPending)
        return;
    m_pingPending = true;
    const auto generation = m_generation;
    const auto host = m_serverHost;
    auto watcher = new QFutureWatcher<QPair<qint64, qint64>>(this);
    connect(watcher, &QFutureWatcher<QPair<qint64, qint64>>::finished, this, [this, watcher, generation]() {
        m_pingPending = false;
        const auto result = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation || !m_connection->isConnected())
            return;
        m_serverPing = result.first;
        m_internetPing = result.second;
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run(
            [host]() { return QPair<qint64, qint64> { PntLatency::measure(host), PntLatency::measure("1.1.1.1") }; }));
}
QString PntController::connectionMode() const
{
    auto mode = m_connectionMode;
#ifdef Q_OS_ANDROID
    const auto current =
            QJniObject::callStaticObjectMethod("org/amnezia/vpn/PntTransport", "connectionMode", "()Ljava/lang/String;")
                    .toString();
    if (!current.isEmpty())
        mode = current;
#endif
    return mode == "relay" ? tr("Транспорт") : mode == "direct" ? tr("Прямое") : tr("Определение…");
}
QString PntController::received() const
{
    return QLocale().formattedDataSize(m_received);
}
QString PntController::sent() const
{
    return QLocale().formattedDataSize(m_sent);
}
QString PntController::receiveSpeed() const
{
    return QLocale().formattedDataSize(m_receiveSpeed) + tr("/с");
}
QString PntController::sendSpeed() const
{
    return QLocale().formattedDataSize(m_sendSpeed) + tr("/с");
}

void PntController::refreshExternalAddress()
{
    if (!m_connection->isConnected() || m_ipPending)
        return;
    m_ipPending = true;
    m_network.clearConnectionCache();
    QNetworkRequest request(QUrl("https://api.ipify.org"));
    request.setTransferTimeout(8000);
    auto reply = m_network.get(request);
    const auto generation = m_generation;
    connect(reply, &QNetworkReply::finished, this, [this, reply, generation]() {
        m_ipPending = false;
        const auto text = QString::fromUtf8(reply->readAll()).trimmed();
        QHostAddress address;
        if (generation == m_generation && m_connection->isConnected()) {
            m_externalAddress = reply->error() == QNetworkReply::NoError && address.setAddress(text) ? address.toString()
                                                                                                     : tr("Недоступен");
            emit changed();
        }
        reply->deleteLater();
    });
}

void PntController::refreshStatistics()
{
#ifdef Q_OS_ANDROID
    if (!m_connection->isConnected() || m_currentServer.isEmpty() || m_statsPending)
        return;
    m_statsPending = true;
    const auto generation = m_generation;
    const auto payload = QString::fromUtf8(
            QJsonDocument(QJsonObject { { "action", "stats" }, { "profile", m_profile }, { "server", m_currentServer } })
                    .toJson(QJsonDocument::Compact));
    auto watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, generation]() {
        m_statsPending = false;
        const auto result = QJsonDocument::fromJson(watcher->result().toUtf8()).object();
        watcher->deleteLater();
        if (generation != m_generation || !m_connection->isConnected())
            return;
        if (result.contains("handshake"))
            m_handshake = result.value("handshake").toInteger();
        emit changed();
    });
    watcher->setFuture(QtConcurrent::run([payload]() {
        const auto input = QJniObject::fromString(payload);
        return QJniObject::callStaticObjectMethod("org/amnezia/vpn/PntTransport", "request",
                                                  "(Ljava/lang/String;)Ljava/lang/String;", input.object<jstring>())
                .toString();
    }));
#endif
}

void PntController::importProfile(const QUrl &url)
{
    if (hasProfile() || m_busy)
        return;
    QByteArray data;
    const auto fileName = url.isLocalFile() ? url.toLocalFile() : url.toString();
    if (!SystemController::readFile(fileName, data) || data.size() > 1048576) {
        fail(tr("Не удалось прочитать профиль PNT."));
        return;
    }
    QJsonParseError error;
    m_pendingProfile = QJsonDocument::fromJson(data, &error).object();
    if (error.error != QJsonParseError::NoError || m_pendingProfile.isEmpty()) {
        fail(tr("Некорректный профиль PNT."));
        return;
    }
    start("import");
}

void PntController::loadServers()
{
    if (!hasProfile() || m_busy || m_sessionStarted)
        return;
    m_status = tr("Получение доступных серверов…");
    start("catalog");
}

void PntController::connectServer(const QString &id)
{
    if (m_busy || m_sessionStarted)
        return;
    const auto found = std::find_if(m_catalog.cbegin(), m_catalog.cend(),
                                    [&id](const QVariant &item) { return item.toMap().value("id").toString() == id; });
    if (found == m_catalog.cend())
        return;
    m_selectedServer = found->toMap().value("name").toString();
    m_serverHost = found->toMap().value("ip").toString();
    m_settings->setValue("Pnt/serverHost", m_serverHost);
    m_serverPing = m_internetPing = -1;
    m_currentServer = id;
    m_connectionMode = "direct";
    m_settings->setValue("Pnt/serverId", id);
    m_settings->setValue("Pnt/serverName", m_selectedServer);
    m_handshake = 0;
    m_received = m_sent = m_previousReceived = m_previousSent = 0;
    m_receiveSpeed = m_sendSpeed = 0;
    m_statsTick = 0;
    m_externalAddress.clear();
    m_vpnAddress.clear();
    m_status = tr("Подготовка VPN-соединения…");
    start("connect", id);
}

void PntController::disconnectVpn()
{
    m_sessionStarted = false;
    m_connection->closeConnection();
    stopAgent();
    m_busy = false;
    m_status = tr("Отключено");
    emit changed();
}

void PntController::resetProfile()
{
    if (m_busy || m_sessionStarted)
        return;
    m_settings->remove("Pnt/profile");
    m_settings->remove("Pnt/serverId");
    m_settings->remove("Pnt/serverName");
    m_settings->remove("Pnt/vpnAddress");
    m_profile = { };
    m_catalog.clear();
    m_selectedServer.clear();
    m_currentServer.clear();
    m_vpnAddress.clear();
    m_externalAddress.clear();
    if (!m_sessionServer.isEmpty())
        m_servers->removeServer(m_sessionServer);
    m_sessionServer.clear();
    m_status.clear();
    emit changed();
}

void PntController::start(const QString &action, const QString &server)
{
    stopAgent();
    m_action = action;
    m_output.clear();
    m_busy = true;
    QJsonObject request { { "action", action }, { "profile", action == "import" ? m_pendingProfile : m_profile } };
    if (!server.isEmpty())
        request["server"] = server;
#ifdef Q_OS_ANDROID
    const auto generation = ++m_generation;
    const auto payload = QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact));
    auto watcher = new QFutureWatcher<QString>(this);
    connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, generation]() {
        const auto response = watcher->result();
        watcher->deleteLater();
        if (generation != m_generation)
            return;
        const auto result = QJsonDocument::fromJson(response.toUtf8()).object();
        if (result.isEmpty())
            fail(tr("Не удалось выполнить запрос PNT на Android."));
        else
            acceptResult(result);
    });
    watcher->setFuture(QtConcurrent::run([payload]() {
        const auto input = QJniObject::fromString(payload);
        return QJniObject::callStaticObjectMethod("org/amnezia/vpn/PntTransport", "request",
                                                  "(Ljava/lang/String;)Ljava/lang/String;", input.object<jstring>())
                .toString();
    }));
    emit changed();
    return;
#endif
    m_process.setProgram(QDir(QCoreApplication::applicationDirPath()).filePath("pnt-transport.exe"));
    m_process.setArguments({ "_vpn-client" });
    m_process.start();
    if (!m_process.waitForStarted(3000)) {
        fail(tr("Не удалось запустить транспорт PNT."));
        return;
    }
    m_process.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
    emit changed();
}

void PntController::readOutput()
{
    m_output += m_process.readAllStandardOutput();
    while (m_output.contains('\n')) {
        const auto end = m_output.indexOf('\n');
        const auto line = m_output.left(end);
        m_output.remove(0, end + 1);
        const auto result = QJsonDocument::fromJson(line).object();
        if (!result.isEmpty())
            acceptResult(result);
    }
}

void PntController::acceptResult(const QJsonObject &result)
{
    if (result.contains("mode"))
        m_connectionMode = result.value("mode").toString();
    if (result.value("status").toString() == "statistics") {
        m_handshake = result.value("handshake").toInteger();
        emit changed();
        return;
    }
    if (result.contains("error")) {
        fail(result.value("error").toString());
        return;
    }
    if (result.contains("status")) {
        if (result.value("status").toString() == "routing") {
            m_status = tr("Маршрут PNT готов, проверка VPN-соединения…");
            emit changed();
            return;
        }
        m_status = result.value("mode").toString() == "direct" ? tr("Подключено напрямую") : tr("Подключено через PNT");
        emit changed();
        return;
    }
    if (m_action == "import") {
        m_profile = m_pendingProfile;
        m_pendingProfile = { };
        m_settings->setValue("Pnt/profile", QJsonDocument(m_profile).toJson(QJsonDocument::Compact));
        if (m_settings->value("Pnt/profile").toByteArray() != QJsonDocument(m_profile).toJson(QJsonDocument::Compact)) {
            m_profile = { };
            fail(tr("Не удалось сохранить профиль в защищённом хранилище устройства."));
            return;
        }
        m_status = tr("Профиль PNT импортирован.");
    } else if (m_action == "catalog") {
        m_catalog.clear();
        const auto nodes = result.value("nodes").toArray();
        for (const auto &item : result.value("servers").toArray()) {
            auto server = item.toObject();
            for (const auto &node : nodes) {
                if (node.toObject().value("id") == server.value("id")) {
                    server["ip"] = node.toObject().value("ip");
                    break;
                }
            }
            m_catalog.append(server.toVariantMap());
        }
        m_status = m_catalog.isEmpty() ? tr("Для этого профиля нет доступных VPN-серверов.") : tr("Выберите VPN-сервер");
        emit serversReady();
    } else if (m_action == "connect" && result.contains("config")) {
        m_serverHost = result.value("host").toString();
        m_settings->setValue("Pnt/serverHost", m_serverHost);
        const auto match =
                QRegularExpression("(?m)^Address\\s*=\\s*([^\\r\\n]+)").match(result.value("config").toString());
        m_vpnAddress = match.hasMatch() ? match.captured(1).trimmed() : QString();
        m_settings->setValue("Pnt/vpnAddress", m_vpnAddress);
        auto imported = m_importer->extractConfigFromData(result.value("config").toString(), "pnt.conf");
        if (imported.config.isEmpty() || imported.errorCode != amnezia::ErrorCode::NoError) {
            fail(tr("VPN-сервер вернул неподдерживаемую конфигурацию."));
            return;
        }
        if (!m_sessionServer.isEmpty())
            m_servers->removeServer(m_sessionServer);
        auto bypass = result.value("entrypoints").toArray();
        bypass.append(result.value("host"));
        QJsonObject metadata { { "server", result.value("server") }, { "controlAddresses", bypass } };
#ifdef Q_OS_ANDROID
        metadata["profile"] = m_profile;
#endif
        imported.config["pnt"] = metadata;
        m_sessionServer = m_servers->addServer({ }, imported.config, serverConfigUtils::Native);
        m_servers->setDefaultServer(m_sessionServer);
        m_sessionStarted = true;
        const auto error = m_connection->openConnection(m_sessionServer);
        if (error != amnezia::ErrorCode::NoError) {
            fail(tr("Не удалось запустить VPN. Проверьте установку службы PNT VPN."));
            return;
        }
        m_status = tr("Проверка прямого VPN-соединения…");
    }
    m_busy = false;
    emit changed();
}

void PntController::stopAgent()
{
    ++m_generation;
#ifdef Q_OS_ANDROID
    return;
#endif
    if (m_process.state() == QProcess::NotRunning)
        return;
    m_process.write("stop\n");
    if (!m_process.waitForFinished(2000)) {
        m_process.kill();
        m_process.waitForFinished(2000);
    }
}

void PntController::fail(const QString &message)
{
    m_status = message;
    m_busy = false;
    if (m_sessionStarted) {
        m_sessionStarted = false;
        m_connection->closeConnection();
    }
    m_process.kill();
    emit changed();
}
