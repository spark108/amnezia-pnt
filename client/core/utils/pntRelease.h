#pragma once

#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QVersionNumber>
#include <optional>

struct PntRelease
{
    QString version;
    QUrl url;
    QByteArray sha256;
    qint64 size = 0;

    static std::optional<PntRelease> fromJson(const QJsonObject &json, const QString &assetName)
    {
        if (json.value("draft").toBool() || json.value("prerelease").toBool())
            return std::nullopt;
        const QString tag = json.value("tag_name").toString();
        const QString version = tag.startsWith('v') ? tag.mid(1) : tag;
        static const QRegularExpression versionPattern("^[0-9]+\\.[0-9]+\\.[0-9]+(?:\\.[0-9]+)?$");
        static const QRegularExpression digestPattern("^sha256:[a-fA-F0-9]{64}$");
        if (!versionPattern.match(version).hasMatch())
            return std::nullopt;
        for (const auto &value : json.value("assets").toArray()) {
            const auto asset = value.toObject();
            if (asset.value("name").toString() != assetName || asset.value("state").toString() != "uploaded")
                continue;
            const QUrl url(asset.value("browser_download_url").toString());
            const QString digest = asset.value("digest").toString();
            const qint64 size = asset.value("size").toInteger();
            if (url.scheme() != "https" || url.host() != "github.com" || !url.userInfo().isEmpty() || url.port(-1) != -1
                || url.hasQuery() || url.hasFragment()
                || url.path() != "/spark108/amnezia-pnt/releases/download/" + tag + "/" + assetName
                || !digestPattern.match(digest).hasMatch() || size <= 0 || size > 512 * 1024 * 1024)
                return std::nullopt;
            return PntRelease { version, url, QByteArray::fromHex(digest.mid(7).toLatin1()), size };
        }
        return std::nullopt;
    }

    bool newerThan(const QString &current) const
    {
        return QVersionNumber::compare(QVersionNumber::fromString(version).normalized(),
                                       QVersionNumber::fromString(current).normalized())
                > 0;
    }
};
