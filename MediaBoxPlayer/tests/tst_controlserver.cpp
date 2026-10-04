#include "audiobackend.h"
#include "controlserver.h"
#include "playerengine.h"
#include "schedulecore/schedulecompiler.h"

#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

#ifdef Q_OS_WIN
#include <QScopeGuard>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Aclapi.h>
#include <Sddl.h>
#endif

using namespace MediaBox;

class SilentBackend final : public AudioBackend
{
public:
    void setSource(const QUrl &) override {}
    void play() override {}
    void pause() override {}
    void stop() override {}
    void seek(qint64) override {}
    void setVolume(int) override {}
    void setMuted(bool) override {}
};

class ConfirmingBackend final : public AudioBackend
{
public:
    void setSource(const QUrl &) override {}
    void play() override { emit stateChanged(State::Playing); }
    void pause() override { emit stateChanged(State::Paused); }
    void stop() override { emit stateChanged(State::Stopped); }
    void seek(qint64 value) override { emit positionChanged(value); }
    void setVolume(int) override {}
    void setMuted(bool) override {}
};

class ControlTests final : public QObject
{
    Q_OBJECT
private slots:
    void tcpLoadsLargePublicationThroughSmallFileNotification()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QFile media(directory.filePath("music.wav"));
        QVERIFY(media.open(QIODevice::WriteOnly));
        media.close();
        const QJsonObject channel{{"id", "00000000-0000-4000-8000-000000000001"},
            {"name", QString(1100000, u'x')}, {"start", "00:00"}, {"end", "00:00"},
            {"untilDayOffset", 1}, {"weekdays", "*"}, {"days", "*"}, {"months", "*"},
            {"volume", 70}, {"order", "sequential"}, {"paths", QJsonArray{media.fileName()}}};
        QJsonObject document;
        QString error;
        QVERIFY2(ScheduleCompiler::fromChannels({{"channels", QJsonArray{channel}}, {"adverts", QJsonArray{}}},
            directory.path(), {}, &document, &error), qPrintable(error));
        const QByteArray snapshot = QJsonDocument(document).toJson(QJsonDocument::Compact);
        QVERIFY(snapshot.size() > 1024 * 1024);
        QVERIFY(QDir().mkpath(directory.filePath("snapshots")));
        const QString relative = QStringLiteral("snapshots/%1.json").arg(document.value("publicationId").toString());
        QSaveFile snapshotFile(directory.filePath(relative));
        QVERIFY(snapshotFile.open(QIODevice::WriteOnly));
        QCOMPARE(snapshotFile.write(snapshot), snapshot.size());
        QVERIFY(snapshotFile.commit());
        const QJsonObject pointer{{"format", "mediabox.active"}, {"schemaVersion", 1},
            {"scheduleId", document.value("scheduleId")}, {"stationId", document.value("stationId")},
            {"publicationId", document.value("publicationId")}, {"revision", document.value("revision")},
            {"snapshotPath", relative},
            {"sha256", QString::fromLatin1(QCryptographicHash::hash(snapshot, QCryptographicHash::Sha256).toHex())}};
        QSaveFile pointerFile(directory.filePath("active.json"));
        QVERIFY(pointerFile.open(QIODevice::WriteOnly));
        QVERIFY(pointerFile.write(QJsonDocument(pointer).toJson(QJsonDocument::Compact)) > 0);
        QVERIFY(pointerFile.commit());
        SilentBackend backend;
        PlayerEngine engine(&backend, nullptr, [] { return QDateTime::currentDateTime(); }, ":memory:");
        const QByteArray token(64, 'a');
        ControlServer server(&engine, token);
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        const QByteArray notification = QJsonDocument(QJsonObject{{"protocolVersion", 1},
            {"token", QString::fromLatin1(token)}, {"command", "loadPublication"},
            {"activePath", pointerFile.fileName()}, {"contentRoot", directory.path()}, {"autoplay", false}})
                .toJson(QJsonDocument::Compact) + '\n';
        QVERIFY(notification.size() < 4096);
        QCOMPARE(client.write(notification), notification.size());
        QTRY_VERIFY(client.canReadLine());
        const auto response = QJsonDocument::fromJson(client.readLine()).object();
        QVERIFY2(response.value("ok").toBool(), qPrintable(QString::fromUtf8(QJsonDocument(response).toJson())));
        QCOMPARE(response.value("status").toObject().value("publicationId"), document.value("publicationId"));
        QVERIFY(response.value("status").toObject().value("scheduleAvailable").toBool());
    }

    void allPlaybackControlsUseTcpAndSurviveReconnect()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString firstPath = directory.filePath(QStringLiteral("Первый трек.wav"));
        const QString secondPath = directory.filePath(QStringLiteral("Второй трек.wav"));
        for (const QString &path : {firstPath, secondPath}) {
            QFile file(path);
            QVERIFY(file.open(QIODevice::WriteOnly));
        }

        ConfirmingBackend backend;
        PlayerEngine engine(&backend);
        const QByteArray token(64, 'b');
        ControlServer server(&engine, token);
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        QTcpSocket client;
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);

        int requestId = 0;
        const auto exchange = [&](QJsonObject request) {
            request.insert("id", ++requestId);
            request.insert("protocolVersion", 1);
            request.insert("token", QString::fromLatin1(token));
            client.write(QJsonDocument(request).toJson(QJsonDocument::Compact) + '\n');
            QElapsedTimer timer;
            timer.start();
            while (!client.canReadLine() && timer.elapsed() < 2000)
                QTest::qWait(1);
            const QJsonObject reply = QJsonDocument::fromJson(client.readLine()).object();
            if (!reply.value("ok").toBool() || reply.value("id").toInt() != requestId)
                return QJsonObject();
            return reply.value("status").toObject();
        };

        auto status = exchange({{"command", "load"}, {"paths", QJsonArray{firstPath, secondPath}}});
        QCOMPARE(status.value("queue").toArray().size(), 2);
        QCOMPARE(status.value("state").toString(), QStringLiteral("stopped"));
        status = exchange({{"command", "enqueue"}, {"paths", QJsonArray{firstPath}}});
        QCOMPARE(status.value("queue").toArray().size(), 3);
        QCOMPARE(exchange({{"command", "volume"}, {"value", 37}}).value("volumePercent").toInt(), 37);
        QVERIFY(exchange({{"command", "mute"}, {"value", true}}).value("muted").toBool());
        QCOMPARE(exchange({{"command", "repeat"}, {"mode", "all"}}).value("repeat").toString(), QStringLiteral("all"));
        QCOMPARE(exchange({{"command", "play"}}).value("state").toString(), QStringLiteral("playing"));
        QCOMPARE(exchange({{"command", "seek"}, {"positionMs", 1200}}).value("positionMs").toInt(), 1200);
        QCOMPARE(exchange({{"command", "pause"}}).value("state").toString(), QStringLiteral("paused"));
        QCOMPARE(exchange({{"command", "next"}}).value("currentIndex").toInt(-1), 1);
        QCOMPARE(exchange({{"command", "previous"}}).value("currentIndex").toInt(-1), 0);
        QCOMPARE(exchange({{"command", "play"}}).value("state").toString(), QStringLiteral("playing"));

        // Losing Manager's connection must not stop playback or change state.
        client.disconnectFromHost();
        QTRY_COMPARE(client.state(), QAbstractSocket::UnconnectedState);
        client.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(client.state(), QAbstractSocket::ConnectedState);
        status = exchange({{"command", "status"}});
        QCOMPARE(status.value("state").toString(), QStringLiteral("playing"));
        QCOMPARE(status.value("queue").toArray().size(), 3);
        QCOMPARE(status.value("volumePercent").toInt(), 37);
        QVERIFY(status.value("muted").toBool());
        QCOMPARE(status.value("repeat").toString(), QStringLiteral("all"));

        status = exchange({{"command", "stop"}});
        QCOMPARE(status.value("state").toString(), QStringLiteral("stopped"));
        QCOMPARE(status.value("positionMs").toInt(-1), 0);
        QCOMPARE(status.value("queue").toArray().size(), 3);
        // Playback stop keeps the same TCP service available for more commands.
        QCOMPARE(exchange({{"command", "play"}}).value("state").toString(), QStringLiteral("playing"));
        status = exchange({{"command", "clear"}});
        QCOMPARE(status.value("state").toString(), QStringLiteral("stopped"));
        QVERIFY(status.value("queue").toArray().isEmpty());
        QCOMPARE(status.value("currentIndex").toInt(), -1);
    }

    void authenticatedCommandsAndFraming()
    {
        SilentBackend backend;
        PlayerEngine engine(&backend);
        const QByteArray token(64, 'a');
        ControlServer server(&engine, token);
        QString error;
        QVERIFY2(server.listen(QHostAddress::LocalHost, 0, &error), qPrintable(error));
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(socket.state(), QAbstractSocket::ConnectedState);

        const auto request = [&](QJsonObject value) {
            value.insert(QStringLiteral("protocolVersion"), 1);
            value.insert(QStringLiteral("token"), QString::fromLatin1(token));
            return QJsonDocument(value).toJson(QJsonDocument::Compact) + '\n';
        };
        const QByteArray volume = request({{"id", "first"}, {"command", "volume"}, {"value", 37}});
        socket.write(volume.first(9));
        socket.flush();
        QTest::qWait(20);
        QCOMPARE(engine.status().value("volumePercent").toInt(), 100);
        QVERIFY(!socket.canReadLine());
        socket.write(volume.mid(9) + request({{"id", 2}, {"command", "status"}}));
        QTRY_VERIFY(socket.canReadLine());
        const auto first = QJsonDocument::fromJson(socket.readLine()).object();
        QVERIFY(first.value("ok").toBool());
        QCOMPARE(first.value("id").toString(), QStringLiteral("first"));
        QCOMPARE(first.value("protocolVersion").toInt(), 1);
        QCOMPARE(first.value("status").toObject().value("volumePercent").toInt(), 37);
        QTRY_VERIFY(socket.canReadLine());
        const auto second = QJsonDocument::fromJson(socket.readLine()).object();
        QCOMPARE(second.value("id").toInt(), 2);
        QCOMPARE(second.value("status").toObject(), engine.status());

        socket.write("{\"protocolVersion\":1,\"command\":\"volume\",\"value\":0,\"token\":\"wrong\"}\n");
        QTRY_VERIFY(socket.canReadLine());
        const auto denied = QJsonDocument::fromJson(socket.readLine()).object();
        QVERIFY(!denied.value("ok").toBool());
        QCOMPARE(denied.value("error").toObject().value("code").toString(), QStringLiteral("unauthorized"));
        QVERIFY(!denied.contains("status"));
        QCOMPARE(engine.status().value("volumePercent").toInt(), 37);

        socket.write("not json\n[]\n");
        for (int i = 0; i < 2; ++i) {
            QTRY_VERIFY(socket.canReadLine());
            const auto reply = QJsonDocument::fromJson(socket.readLine()).object();
            QCOMPARE(reply.value("error").toObject().value("code").toString(), QStringLiteral("invalid_json"));
        }
        // Duplicate fields, including nested active-publication metadata, are
        // rejected before an authenticated command can mutate player state.
        const QByteArray duplicate = QByteArray("{\"protocolVersion\":1,\"token\":\"") + token
            + "\",\"command\":\"volume\",\"value\":0,\"value\":99}\n";
        socket.write(duplicate);
        QTRY_VERIFY(socket.canReadLine());
        QCOMPARE(QJsonDocument::fromJson(socket.readLine()).object().value("error").toObject()
                     .value("code").toString(), QStringLiteral("invalid_json"));
        QCOMPARE(engine.status().value("volumePercent").toInt(), 37);
        auto wrongVersion = QJsonDocument::fromJson(request({{"command", "clear"}})).object();
        wrongVersion.insert("protocolVersion", 2);
        socket.write(QJsonDocument(wrongVersion).toJson(QJsonDocument::Compact) + '\n');
        QTRY_VERIFY(socket.canReadLine());
        QCOMPARE(QJsonDocument::fromJson(socket.readLine()).object().value("error").toObject()
                     .value("code").toString(), QStringLiteral("unsupported_protocol"));
    }

    void oversizedRequestDoesNotExecute()
    {
        SilentBackend backend;
        PlayerEngine engine(&backend);
        ControlServer server(&engine, QByteArray(64, 'a'));
        QString error;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0, &error));
        QTcpSocket socket;
        socket.connectToHost(QHostAddress::LocalHost, server.port());
        QTRY_COMPARE(socket.state(), QAbstractSocket::ConnectedState);
        socket.write(QByteArray(1024 * 1024 + 1, ' '));
        QTRY_COMPARE(socket.state(), QAbstractSocket::UnconnectedState);
        QCOMPARE(engine.status().value("volumePercent").toInt(), 100);
    }

    void tokenIsPersistentAndInvalidTokenIsNotReplaced()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QByteArray first;
        QString error;
        QVERIFY2(loadControlToken(directory.path(), &first, &error), qPrintable(error));
        QCOMPARE(first.size(), 64);
        QByteArray second;
        QVERIFY(loadControlToken(directory.path(), &second, &error));
        QCOMPARE(second, first);
        QFile file(directory.filePath("control.token"));
#ifdef Q_OS_UNIX
        QVERIFY(!(file.permissions() & (QFileDevice::ReadGroup | QFileDevice::WriteGroup
                                       | QFileDevice::ReadOther | QFileDevice::WriteOther)));
#endif
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write("invalid-token\n"), 14);
        file.close();
        QVERIFY(!loadControlToken(directory.path(), &second, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("invalid-token\n"));
    }

#ifdef Q_OS_WIN
    void newTokenDoesNotInheritSharedDirectoryAccess()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        // A readable shared parent models ProgramData without modifying the
        // machine's real directories or depending on the runner's normal ACLs.
        PSECURITY_DESCRIPTOR parentDescriptor = nullptr;
        QVERIFY(ConvertStringSecurityDescriptorToSecurityDescriptorW(
            L"D:P(A;OICI;FA;;;OW)(A;OICI;FA;;;SY)(A;OICI;FR;;;WD)",
            SDDL_REVISION_1, &parentDescriptor, nullptr));
        const auto freeParentDescriptor = qScopeGuard([&] { LocalFree(parentDescriptor); });
        PACL parentAcl = nullptr;
        BOOL present = FALSE;
        BOOL defaulted = FALSE;
        QVERIFY(GetSecurityDescriptorDacl(parentDescriptor, &present, &parentAcl, &defaulted));
        QVERIFY(present && parentAcl);
        QString parentPath = QDir::toNativeSeparators(directory.path());
        QCOMPARE(SetNamedSecurityInfoW(reinterpret_cast<LPWSTR>(parentPath.data()), SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION | PROTECTED_DACL_SECURITY_INFORMATION,
            nullptr, nullptr, parentAcl, nullptr), DWORD(ERROR_SUCCESS));

        QByteArray token;
        QString error;
        QVERIFY2(loadControlToken(directory.path(), &token, &error), qPrintable(error));
        QString path = QDir::toNativeSeparators(directory.filePath("control.token"));
        PACL tokenAcl = nullptr;
        PSECURITY_DESCRIPTOR tokenDescriptor = nullptr;
        QCOMPARE(GetNamedSecurityInfoW(reinterpret_cast<LPWSTR>(path.data()), SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION, nullptr, nullptr, &tokenAcl, nullptr, &tokenDescriptor),
            DWORD(ERROR_SUCCESS));
        const auto freeTokenDescriptor = qScopeGuard([&] { LocalFree(tokenDescriptor); });
        SECURITY_DESCRIPTOR_CONTROL control = 0;
        DWORD revision = 0;
        QVERIFY(GetSecurityDescriptorControl(tokenDescriptor, &control, &revision));
        QVERIFY(control & SE_DACL_PROTECTED);
        QVERIFY(tokenAcl && tokenAcl->AceCount > 0);

        HANDLE processToken = nullptr;
        QVERIFY(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &processToken));
        const auto closeToken = qScopeGuard([&] { CloseHandle(processToken); });
        DWORD tokenSize = 0;
        GetTokenInformation(processToken, TokenUser, nullptr, 0, &tokenSize);
        QVERIFY(tokenSize > 0);
        QByteArray tokenInformation(tokenSize, Qt::Uninitialized);
        QVERIFY(GetTokenInformation(processToken, TokenUser, tokenInformation.data(), tokenSize, &tokenSize));
        const auto user = reinterpret_cast<TOKEN_USER *>(tokenInformation.data());
        for (DWORD index = 0; index < tokenAcl->AceCount; ++index) {
            void *entry = nullptr;
            QVERIFY(GetAce(tokenAcl, index, &entry));
            const auto ace = static_cast<ACCESS_ALLOWED_ACE *>(entry);
            QCOMPARE(ace->Header.AceType, BYTE(ACCESS_ALLOWED_ACE_TYPE));
            QVERIFY(!(ace->Header.AceFlags & INHERITED_ACE));
            auto sid = &ace->SidStart;
            QVERIFY(EqualSid(sid, user->User.Sid)
                    || IsWellKnownSid(sid, WinLocalSystemSid)
                    || IsWellKnownSid(sid, WinBuiltinAdministratorsSid));
        }
        QByteArray second;
        QVERIFY2(loadControlToken(directory.path(), &second, &error), qPrintable(error));
        QCOMPARE(second, token);

        // Token protection must leave the caller-provided parent ACL intact.
        PACL unchangedAcl = nullptr;
        PSECURITY_DESCRIPTOR unchangedDescriptor = nullptr;
        QCOMPARE(GetNamedSecurityInfoW(reinterpret_cast<LPWSTR>(parentPath.data()), SE_FILE_OBJECT,
            DACL_SECURITY_INFORMATION, nullptr, nullptr, &unchangedAcl, nullptr, &unchangedDescriptor),
            DWORD(ERROR_SUCCESS));
        const auto freeUnchangedDescriptor = qScopeGuard([&] { LocalFree(unchangedDescriptor); });
        QVERIFY(unchangedAcl);
        QCOMPARE(unchangedAcl->AclSize, parentAcl->AclSize);
        QCOMPARE(QByteArray(reinterpret_cast<const char *>(unchangedAcl), unchangedAcl->AclSize),
                 QByteArray(reinterpret_cast<const char *>(parentAcl), parentAcl->AclSize));
    }
#endif
};

QTEST_GUILESS_MAIN(ControlTests)
#include "tst_controlserver.moc"
