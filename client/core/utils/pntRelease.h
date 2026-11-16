#pragma once

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QUrl>
#include <QVersionNumber>
#include <optional>

struct PntVersion
{
    QString amnezia;
    QString revision;

    QString displayName() const
    {
        if (revision.isEmpty())
            return amnezia;
        return QCoreApplication::translate("PntVersion", "%1 rev %2").arg(amnezia, revision);
    }

    bool newerThan(const PntVersion &current) const
    {
        const auto compare = [](const QString &left, const QString &right) {
            return QVersionNumber::compare(QVersionNumber::fromString(left).normalized(),
                                           QVersionNumber::fromString(right).normalized());
        };
        const int amneziaOrder = compare(amnezia, current.amnezia);
        return amneziaOrder == 0 ? compare(revision, current.revision) > 0 : amneziaOrder > 0;
    }

    static std::optional<PntVersion> fromRelease(const QString &tag, const QString &name)
    {
        static const QRegularExpression tagPattern(R"(^v?([0-9]{1,9}(?:\.[0-9]{1,9}){2,3})\z)");
        static const QRegularExpression releaseNamePattern(
                R"(^PNT VPN ([0-9]{1,9}(?:\.[0-9]{1,9}){3}) rev ([0-9]{1,9}\.[0-9]{1,9})\z)");
        const auto tagMatch = tagPattern.match(tag);
        if (!tagMatch.hasMatch())
            return std::nullopt;

        const QString number = tagMatch.captured(1);
        const bool transitionRelease = number == "5.1.0.13";
        if (!transitionRelease && number.section('.', 3, 3).toInt() < 2200)
            return PntVersion { number, { } };

        const auto nameMatch = releaseNamePattern.match(name);
        const QString baseVersion = nameMatch.captured(1);
        if (!nameMatch.hasMatch() || (!transitionRelease && baseVersion.section('.', 0, 2) != number.section('.', 0, 2)))
            return std::nullopt;
        return PntVersion { baseVersion, nameMatch.captured(2) };
    }
};

struct PntRelease
{
    PntVersion version;
    QUrl url;
    QByteArray sha256;
    qint64 size = 0;

    static std::optional<PntRelease> fromJson(const QJsonObject &json, const QString &assetName)
    {
        if (json.value("draft").toBool() || json.value("prerelease").toBool())
            return std::nullopt;
        const QString tag = json.value("tag_name").toString();
        const auto version = PntVersion::fromRelease(tag, json.value("name").toString());
        static const QRegularExpression digestPattern("^sha256:[a-fA-F0-9]{64}$");
        if (!version)
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
            return PntRelease { *version, url, QByteArray::fromHex(digest.mid(7).toLatin1()), size };
        }
        return std::nullopt;
    }
};
