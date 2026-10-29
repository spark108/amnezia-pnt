#include "pntLatency.h"

#include <QHostAddress>
#include <QProcess>
#include <QRegularExpression>

#ifdef Q_OS_WIN
    #include <winsock2.h>

    #include <windows.h>

    #include <iphlpapi.h>

    #include <icmpapi.h>
#endif

qint64 PntLatency::measure(const QString &ipv4)
{
    const QHostAddress address(ipv4);
    if (address.protocol() != QAbstractSocket::IPv4Protocol)
        return -1;
#ifdef Q_OS_WIN
    const auto handle = IcmpCreateFile();
    if (handle == INVALID_HANDLE_VALUE)
        return -1;
    const QByteArray payload(32, 'p');
    QByteArray buffer(sizeof(ICMP_ECHO_REPLY) + payload.size() + 8, 0);
    const auto count = IcmpSendEcho(handle, htonl(address.toIPv4Address()), const_cast<char *>(payload.constData()),
                                    WORD(payload.size()), nullptr, buffer.data(), DWORD(buffer.size()), 2000);
    const auto reply = reinterpret_cast<const ICMP_ECHO_REPLY *>(buffer.constData());
    const qint64 result = count > 0 && reply->Status == IP_SUCCESS ? qint64(reply->RoundTripTime) : -1;
    IcmpCloseHandle(handle);
    return result;
#else
    QProcess process;
    #ifdef Q_OS_ANDROID
    process.start("/system/bin/ping", { "-n", "-c", "1", "-W", "2", ipv4 });
    #else
    process.start("ping", { "-n", "-c", "1", "-W", "2", ipv4 });
    #endif
    if (!process.waitForFinished(3000)) {
        process.kill();
        process.waitForFinished(1000);
        return -1;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0)
        return -1;
    const auto match = QRegularExpression("time[=<]([0-9]+(?:\\.[0-9]+)?)")
                               .match(QString::fromLatin1(process.readAllStandardOutput()));
    return match.hasMatch() ? qRound64(match.captured(1).toDouble()) : -1;
#endif
}
