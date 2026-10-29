#include "core/utils/pntLatency.h"
#include "core/utils/pntRelease.h"
#include <QtTest>

class TestPntRelease : public QObject
{
    Q_OBJECT

    QJsonObject release() const
    {
        return { { "tag_name", "v5.1.0.5" },
                 { "assets",
                   QJsonArray { QJsonObject { { "name", "PntVPN-android-arm64.apk" },
                                              { "state", "uploaded" },
                                              { "size", 123 },
                                              { "digest", "sha256:" + QString(64, 'a') },
                                              { "browser_download_url",
                                                "https://github.com/spark108/amnezia-pnt/releases/download/v5.1.0.5/"
                                                "PntVPN-android-arm64.apk" } } } } };
    }

    std::optional<PntRelease> parse(const QJsonObject &json) const
    {
        return PntRelease::fromJson(json, "PntVPN-android-arm64.apk");
    }

    QJsonObject withAssetField(const QString &name, const QJsonValue &value) const
    {
        auto json = release();
        auto asset = json["assets"].toArray().first().toObject();
        asset[name] = value;
        json["assets"] = QJsonArray { asset };
        return json;
    }

private slots:
    void ping()
    {
        QCOMPARE(PntLatency::measure("not-an-ip"), -1);
        QCOMPARE(PntLatency::measure("127.0.0.1; whoami"), -1);
#ifdef Q_OS_WIN
        QVERIFY(PntLatency::measure("127.0.0.1") >= 0);
#endif
    }

    void newerRelease()
    {
        const auto result = parse(release());
        QVERIFY(result);
        QVERIFY(result->newerThan("5.1.0.4"));
        QVERIFY(!result->newerThan("5.1.0.5"));
        QVERIFY(!result->newerThan("5.1.0.6"));
        QCOMPARE(result->sha256.size(), 32);
        QCOMPARE(result->size, 123);
    }

    void numericVersionOrder()
    {
        auto json = release();
        json["tag_name"] = "v5.1.0.10";
        auto asset = json["assets"].toArray().first().toObject();
        asset["browser_download_url"] =
                "https://github.com/spark108/amnezia-pnt/releases/download/v5.1.0.10/PntVPN-android-arm64.apk";
        json["assets"] = QJsonArray { asset };
        QVERIFY(parse(json)->newerThan("5.1.0.9"));
    }

    void invalidMetadata_data()
    {
        QTest::addColumn<QString>("field");
        QTest::addColumn<QJsonValue>("value");
        QTest::newRow("missing hash") << QString("digest") << QJsonValue();
        QTest::newRow("invalid hash") << QString("digest") << QJsonValue("sha256:abcd");
        QTest::newRow("wrong platform") << QString("name") << QJsonValue("PntVPN-windows-x64.zip");
        QTest::newRow("upload incomplete") << QString("state") << QJsonValue("new");
        QTest::newRow("empty") << QString("size") << QJsonValue(0);
        QTest::newRow("oversized") << QString("size") << QJsonValue(600 * 1024 * 1024);
        QTest::newRow("wrong host") << QString("browser_download_url") << QJsonValue("https://example.com/update.apk");
        QTest::newRow("insecure") << QString("browser_download_url")
                                  << QJsonValue("http://github.com/spark108/amnezia-pnt/releases/download/v5.1.0.5/"
                                                "PntVPN-android-arm64.apk");
        QTest::newRow("other repository")
                << QString("browser_download_url")
                << QJsonValue("https://github.com/other/repo/releases/download/v5.1.0.5/PntVPN-android-arm64.apk");
    }

    void invalidMetadata()
    {
        QFETCH(QString, field);
        QFETCH(QJsonValue, value);
        QVERIFY(!parse(withAssetField(field, value)));
    }

    void unpublishedRelease()
    {
        auto json = release();
        json["draft"] = true;
        QVERIFY(!parse(json));
        json["draft"] = false;
        json["prerelease"] = true;
        QVERIFY(!parse(json));
        json["prerelease"] = false;
        json["tag_name"] = "invalid";
        QVERIFY(!parse(json));
        QVERIFY(!parse({ }));
    }
};

QTEST_GUILESS_MAIN(TestPntRelease)
#include "testPntRelease.moc"
