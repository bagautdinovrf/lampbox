#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

class ProcessTests final : public QObject
{
    Q_OBJECT
private slots:
    void rejectsInvalidPort()
    {
        QProcess process;
        process.start(QStringLiteral(PLAYER_EXECUTABLE), {"--port", "65536"});
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 2);
    }

    void serviceAcceptsCommandsAndStopsCleanly()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        QTcpServer reservation;
        QVERIFY(reservation.listen(QHostAddress::LocalHost, 0));
        const auto port = reservation.serverPort();
        reservation.close();
        QProcess process;
        process.start(QStringLiteral(PLAYER_EXECUTABLE),
                      {"--data-dir", directory.path(), "--port", QString::number(port)});
        QVERIFY(process.waitForStarted(5000));
        QTcpSocket socket;
        const auto connected = [&] {
            if (socket.state() == QAbstractSocket::ConnectedState)
                return true;
            socket.abort();
            socket.connectToHost(QHostAddress::LocalHost, port);
            return socket.waitForConnected(50);
        };
        QTRY_VERIFY_WITH_TIMEOUT(connected(), 5000);
        QFile token(directory.filePath("control.token"));
        QVERIFY(token.open(QIODevice::ReadOnly));
        const QByteArray controlToken = token.readAll().trimmed();
        token.close();
        const QJsonObject command{{"protocolVersion", 1}, {"token", QString::fromLatin1(controlToken)},
                                  {"command", "status"}, {"id", "service-test"}};
        socket.write(QJsonDocument(command).toJson(QJsonDocument::Compact) + '\n');
        QVERIFY(socket.waitForReadyRead(5000));
        QTRY_VERIFY(socket.canReadLine());
        const auto response = QJsonDocument::fromJson(socket.readLine()).object();
        QVERIFY(response.value("ok").toBool());
        QCOMPARE(response.value("id").toString(), QStringLiteral("service-test"));
        QCOMPARE(response.value("status").toObject().value("state").toString(), QStringLiteral("stopped"));

        // The data-directory lock applies even when a second process chooses another port.
        QProcess duplicate;
        duplicate.start(QStringLiteral(PLAYER_EXECUTABLE),
                        {"--data-dir", directory.path(), "--port", port == 17655 ? "17656" : "17655"});
        QVERIFY(duplicate.waitForFinished(5000));
        QCOMPARE(duplicate.exitCode(), 1);
        QCOMPARE(process.state(), QProcess::Running);

#ifdef Q_OS_UNIX
        process.terminate(); // SIGTERM must reach the event loop and release the lock.
        QVERIFY(process.waitForFinished(5000));
        QCOMPARE(process.exitStatus(), QProcess::NormalExit);
        QCOMPARE(process.exitCode(), 0);
        QVERIFY(!QFile::exists(directory.filePath("player.lock")));
#else
        // Windows SCM stop is tested on an installed service, not a console subprocess.
        process.kill();
        QVERIFY(process.waitForFinished(5000));
#endif
        QVERIFY(!process.readAllStandardError().contains(controlToken));
    }
};

QTEST_GUILESS_MAIN(ProcessTests)
#include "tst_process.moc"
