#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>

#include "settings.h"

class SettingsTest final : public QObject
{
    Q_OBJECT

private slots:
    void configurationFileSelection_data()
    {
        QTest::addColumn<QString>("applicationName");
        QTest::addColumn<bool>("legacyExists");
        QTest::addColumn<bool>("preferredExists");
        QTest::addColumn<QString>("selectedName");

        QTest::newRow("new-install") << QString("MediaBoxManager") << false << false
                                     << QString("MediaBoxManager.conf");
        QTest::newRow("legacy-settings") << QString("MediaBoxManager") << true << false
                                         << QString("lampbox.conf");
        QTest::newRow("new-settings-preferred") << QString("MediaBoxManager") << true << true
                                                << QString("MediaBoxManager.conf");
        QTest::newRow("new-settings-only") << QString("MediaBoxManager") << false << true
                                           << QString("MediaBoxManager.conf");
        QTest::newRow("other-application") << QString("SettingsTestApplication") << true << false
                                           << QString("SettingsTestApplication.conf");
    }

    void configurationFileSelection()
    {
        QFETCH(QString, applicationName);
        QFETCH(bool, legacyExists);
        QFETCH(bool, preferredExists);
        QFETCH(QString, selectedName);

        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString originalApplicationName = QCoreApplication::applicationName();
        const bool originalTestMode = QStandardPaths::isTestModeEnabled();
        const auto restoreApplication = qScopeGuard([&] {
            QCoreApplication::setApplicationName(originalApplicationName);
            QStandardPaths::setTestModeEnabled(originalTestMode);
        });
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setApplicationName(applicationName);

        const QString legacyPath = directory.filePath("lampbox.conf");
        const QString preferredPath = directory.filePath(applicationName + ".conf");
        const QByteArray legacyContents("source=legacy\n[FileFormats]\nAudioFormats/flac=true\n");
        const QByteArray preferredContents("source=current\n[FileFormats]\nAudioFormats/ogg=true\n");
        if (legacyExists) {
            QFile file(legacyPath);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(legacyContents), qint64(legacyContents.size()));
        }
        if (preferredExists) {
            QFile file(preferredPath);
            QVERIFY(file.open(QIODevice::WriteOnly));
            QCOMPARE(file.write(preferredContents), qint64(preferredContents.size()));
        }

        const QString selectedPath = Settings::configurationFilePath(directory.path());
        QCOMPARE(selectedPath, directory.filePath(selectedName));
        if (QFileInfo::exists(selectedPath)) {
            const QSettings settings(selectedPath, QSettings::IniFormat);
            QCOMPARE(settings.value("source").toString(),
                     selectedName == "lampbox.conf" ? QString("legacy") : QString("current"));
        }
        QCOMPARE(QFileInfo::exists(legacyPath), legacyExists);
        QCOMPARE(QFileInfo::exists(preferredPath), preferredExists);
        if (legacyExists) {
            QFile file(legacyPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), legacyContents);
        }
        if (preferredExists) {
            QFile file(preferredPath);
            QVERIFY(file.open(QIODevice::ReadOnly));
            QCOMPARE(file.readAll(), preferredContents);
        }
    }
};

QTEST_GUILESS_MAIN(SettingsTest)
#include "tst_settings.moc"
