#include "pntUpdateController.h"

#include "version.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QStandardPaths>
#include <QTimer>
#include <QUuid>

#ifdef Q_OS_WIN
    #include <qt_windows.h>
    #include <shellapi.h>
#elif defined(Q_OS_ANDROID)
    #include <QCoreApplication>
    #include <QJniEnvironment>
    #include <QJniObject>
#endif

PntUpdateController::PntUpdateController(QObject *parent) : QObject(parent)
{
    QTimer::singleShot(3000, this, &PntUpdateController::check);
    auto timer = new QTimer(this);
    timer->setInterval(6 * 60 * 60 * 1000);
    connect(timer, &QTimer::timeout, this, &PntUpdateController::check);
    timer->start();
}

QString PntUpdateController::currentVersion() const
{
    return QStringLiteral(APP_VERSION);
}

QString PntUpdateController::assetName() const
{
#if defined(Q_OS_WIN)
    return "PntVPN-windows-x64.zip";
#elif defined(Q_OS_ANDROID) && defined(Q_PROCESSOR_ARM_64)
    return "PntVPN-android-arm64.apk";
#else
    return { };
#endif
}

void PntUpdateController::fail(const QString &message)
{
    m_busy = false;
    m_status = message;
    if (m_file) {
        m_file->cancelWriting();
        m_file.reset();
    }
    emit changed();
}

void PntUpdateController::check()
{
    if (m_busy || m_ready)
        return;
    if (assetName().isEmpty()) {
        fail(tr("Для этой платформы нет пакета обновления."));
        return;
    }
    m_busy = true;
    m_status = tr("Проверка обновлений…");
    emit changed();
    QNetworkRequest request(QUrl("https://api.github.com/repos/spark108/amnezia-pnt/releases/latest"));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("User-Agent", "PntVPN/" + currentVersion().toLatin1());
    request.setTransferTimeout(15000);
    auto reply = m_network.get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (httpStatus == 404) {
            m_release.reset();
            fail(tr("Релизов пока нет."));
            return;
        }
        if (reply->error() != QNetworkReply::NoError) {
            fail(httpStatus == 403 || httpStatus == 429
                         ? tr("Лимит GitHub. Повторите проверку позже.")
                         : tr("Не удалось проверить обновления: %1").arg(reply->errorString()));
            return;
        }
        const auto data = reply->readAll();
        const auto release = data.size() <= 1024 * 1024
                ? PntRelease::fromJson(QJsonDocument::fromJson(data).object(), assetName())
                : std::nullopt;
        if (!release) {
            fail(tr("В релизе нет подходящего пакета с SHA-256."));
            return;
        }
        m_release = release->newerThan(currentVersion()) ? release : std::nullopt;
        m_busy = false;
        m_status = m_release ? tr("Доступна версия %1").arg(m_release->version) : tr("Установлена последняя версия.");
        emit changed();
    });
}

void PntUpdateController::download()
{
    if (m_busy || m_ready || !m_release)
        return;
    const auto directory = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + "/updates";
    if (!QDir().mkpath(directory)) {
        fail(tr("Не удалось создать каталог обновлений."));
        return;
    }
    m_path = directory + "/" + assetName();
    m_file = std::make_unique<QSaveFile>(m_path);
    if (!m_file->open(QIODevice::WriteOnly)) {
        fail(tr("Не удалось сохранить обновление: %1").arg(m_file->errorString()));
        return;
    }
    m_received = 0;
    m_progress = 0;
    m_hash.reset();
    m_busy = true;
    m_status = tr("Загрузка обновления…");
    emit changed();
    QNetworkRequest request(m_release->url);
    request.setTransferTimeout(30000);
    auto reply = m_network.get(request);
    reply->setReadBufferSize(256 * 1024);
    const auto consume = [this, reply]() {
        if (!m_file)
            return;
        const auto bytes = reply->readAll();
        m_received += bytes.size();
        if (m_received > m_release->size || m_file->write(bytes) != bytes.size()) {
            fail(tr("Ошибка записи или неверный размер обновления."));
            reply->abort();
            return;
        }
        m_hash.addData(bytes);
        m_progress = int(m_received * 100 / m_release->size);
        emit changed();
    };
    connect(reply, &QNetworkReply::readyRead, this, consume);
    connect(reply, &QNetworkReply::finished, this, [this, reply, consume]() {
        reply->deleteLater();
        if (!m_file)
            return;
        consume();
        if (!m_file)
            return;
        if (reply->error() != QNetworkReply::NoError) {
            fail(tr("Ошибка загрузки: %1").arg(reply->errorString()));
            return;
        }
        if (m_received != m_release->size || m_hash.result() != m_release->sha256) {
            fail(tr("Проверка SHA-256 не пройдена. Загрузите обновление заново."));
            return;
        }
        if (!m_file->commit()) {
            fail(tr("Не удалось сохранить загруженное обновление."));
            return;
        }
        m_file.reset();
        m_ready = true;
        m_busy = false;
        m_progress = 100;
        m_status = tr("Обновление проверено и готово к установке.");
        emit changed();
    });
}

void PntUpdateController::install()
{
    if (!m_ready || m_busy || !m_release)
        return;
    if (m_vpnActive) {
        fail(tr("Перед установкой отключите VPN."));
        return;
    }
    QFile package(m_path);
    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!package.open(QIODevice::ReadOnly) || !hash.addData(&package) || hash.result() != m_release->sha256) {
        m_ready = false;
        fail(tr("Пакет обновления изменён. Загрузите его заново."));
        return;
    }
    package.close();
#if defined(Q_OS_WIN)
    const auto scriptPath =
            QFileInfo(m_path).absolutePath() + "/install-" + QUuid::createUuid().toString(QUuid::WithoutBraces) + ".ps1";
    if (!QFile::copy(":/pnt/update-windows.ps1", scriptPath)) {
        fail(tr("Не удалось подготовить установщик."));
        return;
    }
    const auto quote = [](const QString &value) { return '"' + QDir::toNativeSeparators(value) + '"'; };
    const QString arguments = "-NoProfile -ExecutionPolicy Bypass -File " + quote(scriptPath) + " -Archive "
            + quote(m_path) + " -Destination " + quote(QCoreApplication::applicationDirPath()) + " -ExpectedHash "
            + QString::fromLatin1(m_release->sha256.toHex()) + " -ParentProcessId "
            + QString::number(QCoreApplication::applicationPid());
    const QString executable = qEnvironmentVariable("SystemRoot") + "/System32/WindowsPowerShell/v1.0/powershell.exe";
    SHELLEXECUTEINFOW info { };
    info.cbSize = sizeof(info);
    info.fMask = SEE_MASK_NOCLOSEPROCESS;
    info.lpVerb = L"runas";
    info.lpFile = reinterpret_cast<LPCWSTR>(executable.utf16());
    info.lpParameters = reinterpret_cast<LPCWSTR>(arguments.utf16());
    info.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteExW(&info)) {
        fail(tr("Установка отменена или не удалось запустить установщик."));
        return;
    }
    if (info.hProcess)
        CloseHandle(info.hProcess);
    QCoreApplication::quit();
#elif defined(Q_OS_ANDROID)
    const auto context = QNativeInterface::QAndroidApplication::context();
    const auto path = QJniObject::fromString(m_path);
    const auto result =
            QJniObject::callStaticObjectMethod("org/amnezia/vpn/PntAppUpdater", "install",
                                               "(Landroid/content/Context;Ljava/lang/String;)Ljava/lang/String;",
                                               context.object(), path.object<jstring>());
    QJniEnvironment environment;
    if (environment.checkAndClearExceptions() || !result.isValid()) {
        fail(tr("Не удалось открыть установщик Android."));
        return;
    }
    const auto error = result.toString();
    if (error == "permission")
        m_status = tr("Разрешите установку из PNT VPN в настройках Android, вернитесь и нажмите «Установить» снова.");
    else if (!error.isEmpty())
        m_status = tr("Не удалось открыть установщик Android: %1").arg(error);
    else
        m_status = tr("Подтвердите обновление в установщике Android.");
    emit changed();
#else
    fail(tr("Установка на этой платформе не поддерживается."));
#endif
}
