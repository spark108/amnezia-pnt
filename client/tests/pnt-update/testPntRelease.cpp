#include "core/utils/pntLatency.h"
#include "core/utils/pntRelease.h"
#include <QtTest>

class TestPntRelease : public QObject
{
    Q_OBJECT

    QJsonObject release(const QString &tag = "v5.1.0.5", const QString &name = { }) const
    {
        return { { "tag_name", tag },
                 { "name", name },
                 { "assets",
                   QJsonArray { QJsonObject { { "name", "PntVPN-android-arm64.apk" },
                                              { "state", "uploaded" },
                                              { "size", 123 },
                                              { "digest", "sha256:" + QString(64, 'a') },
                                              { "browser_download_url",
                                                QString("https://github.com/spark108/amnezia-pnt/releases/download/%1/"
                                                        "PntVPN-android-arm64.apk")
                                                        .arg(tag) } } } } };
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
        QCOMPARE(result->version.displayName(), "5.1.0.5");
        QVERIFY(result->version.newerThan({ "5.1.0.4", { } }));
        QVERIFY(!result->version.newerThan({ "5.1.0.5", { } }));
        QVERIFY(!result->version.newerThan({ "5.1.0.6", { } }));
        QCOMPARE(result->sha256.size(), 32);
        QCOMPARE(result->size, 123);
    }

    void numericVersionOrder()
    {
        const auto result = parse(release("v5.1.0.10"));
        QVERIFY(result);
        QVERIFY(result->version.newerThan({ "5.1.0.9", { } }));
    }

    void independentVersions()
    {
        const auto result = parse(release("v5.1.1.2200", "PNT VPN 5.1.1.8 rev 1.10"));
        QVERIFY(result);
        QCOMPARE(result->version.displayName(), "5.1.1.8 rev 1.10");
        QVERIFY(result->version.newerThan({ "5.1.1.8", "1.9" }));
        QVERIFY(result->version.newerThan({ "5.1.1.7", "1.10" }));
        QVERIFY(result->version.newerThan({ "5.1.0.12", { } }));
        QVERIFY(!result->version.newerThan({ "5.1.1.8", "1.10" }));
        QVERIFY(!result->version.newerThan({ "5.1.1.8", "2.0" }));
        QVERIFY(!result->version.newerThan({ "5.1.1.9", "1.0" }));
        const auto oldRelease = parse(release("v5.1.0.12"));
        QVERIFY(oldRelease);
        QVERIFY(!oldRelease->version.newerThan(result->version));
    }

    void invalidRevisionMetadata()
    {
        QVERIFY(!parse(release("v5.1.1.2200")));
        QVERIFY(!parse(release("v5.1.1.2200", "PNT VPN 5.1.0.8 rev 1.1")));
        QVERIFY(!parse(release("v5.1.1.2200", "PNT VPN 5.1.1.8 rev development")));
        QVERIFY(!parse(release("v5.1.1.2200", "PNT VPN 5.1.1.8 rev 1.1-beta")));
    }

    void transitionRelease()
    {
        const auto result = parse(release("5.1.0.13", "PNT VPN 5.1.1.8 rev 1.1"));
        QVERIFY(result);
        QCOMPARE(result->version.displayName(), "5.1.1.8 rev 1.1");
        QVERIFY(result->version.newerThan({ "5.1.0.12", { } }));
        QVERIFY(!result->version.newerThan({ "5.1.1.8", "1.1" }));
        QVERIFY(!parse(release("5.1.0.13")));
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
