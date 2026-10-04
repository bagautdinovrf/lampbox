#include "localplayerlauncher.h"
#include "settings.h"
#include "controlserver.h"
#include "storagepaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

namespace {
QString normalizedHost(const QString &host)
{
    const QString trimmed = host.trimmed();
    if (trimmed.compare(QStringLiteral("localhost"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("127.0.0.1");
    const QHostAddress address(trimmed);
    return address.isNull() ? trimmed : address.toString();
}

QString endpointKey(const PlayerConnectionSettings &settings)
{
    QString host = normalizedHost(settings.host);
    host.replace(QLatin1Char(':'), QLatin1Char('_'));
    return host + QLatin1Char('-') + QString::number(settings.port);
}

bool validToken(const QString &token)
{
    static const QRegularExpression pattern(QStringLiteral("\\A[0-9a-f]{64}\\z"));
    return pattern.match(token).hasMatch();
}
}

LocalPlayerLauncher::LocalPlayerLauncher(MediaBoxPlayerClient *client, Kind kind)
    : QObject(client), mClient(client), mKind(kind)
{
    connect(this, &LocalPlayerLauncher::connectionError, mClient, &MediaBoxPlayerClient::connectionError);
    // Let the window subscribe to errors and state changes before connecting.
    QTimer::singleShot(0, this, [this] {
        if (!mConnectionRequested)
            reloadConnection();
    });
    connect(mClient, &MediaBoxPlayerClient::connectionStateChanged, this, [this](MediaBoxPlayerClient::ConnectionState state) {
        if (state == MediaBoxPlayerClient::ConnectionState::Ready)
            mLaunchAttempts.remove(endpointKey(mConnection));
    });
    connect(mClient, &MediaBoxPlayerClient::connectionAttemptFailed, this,
            [this](const PlayerConnectionSettings &settings) {
        if (!supportsLocalStart() || !isLocalHost(settings.host))
            return;
        const quint64 revision = mConnectionRevision;
        // Finish the failed connection's lifecycle first. Explicit disconnect
        // or new settings before this callback cancel the scheduled launch.
        QTimer::singleShot(0, this, [this, settings, revision] {
            if (revision == mConnectionRevision && mClient->connectionState() == MediaBoxPlayerClient::ConnectionState::Reconnecting)
                startLocalPlayer(settings);
        });
    });
}

bool LocalPlayerLauncher::isLocalHost(const QString &host)
{
    return QHostAddress(normalizedHost(host)).isLoopback();
}

bool LocalPlayerLauncher::supportsLocalStart()
{
#ifdef Q_OS_ANDROID
    return false;
#else
    return true;
#endif
}

QString LocalPlayerLauncher::managedDataDirectory(const PlayerConnectionSettings &settings) const
{
    const QString root = MediaBox::StoragePaths::configurationDirectory(
        MediaBox::StoragePaths::Application::Manager);
    return QDir(root).filePath((mKind == Kind::Audio ? QStringLiteral("managed-player/") : QStringLiteral("managed-vplayer/")) + endpointKey(settings));
}

void LocalPlayerLauncher::connectToPlayer(const PlayerConnectionSettings &settings)
{
    mConnectionRequested = true;
    ++mConnectionRevision;
    PlayerConnectionSettings prepared = settings;
    prepared.host = normalizedHost(settings.host);
    prepared.token = prepared.token.trimmed();
    // This is an explicit user/settings request; automatic transport retries
    // do not pass through here and remain limited to one launch attempt.
    mLaunchAttempts.remove(endpointKey(prepared));
    if (supportsLocalStart() && isLocalHost(prepared.host) && prepared.token.isEmpty()) {
        // A default standalone/service player already owns this token. Read
        // it without modifying its directory or rotating its credentials.
        const QString standardToken = QDir(MediaBox::StoragePaths::configurationDirectory(
            (mKind == Kind::Audio ? MediaBox::StoragePaths::Application::Player
                                 : MediaBox::StoragePaths::Application::VideoPlayer))).filePath(QStringLiteral("control.token"));
        const QString dataDirectory = managedDataDirectory(prepared);
        QString error;
        QByteArray token;
        if (prepared.port == (mKind == Kind::Audio ? 17655 : 17656) && QFileInfo::exists(standardToken)) {
            QFile file(standardToken);
            if (file.open(QIODevice::ReadOnly)) {
                token = file.read(66).trimmed();
                if (!file.atEnd() || !validToken(QString::fromLatin1(token)))
                    token.clear();
            }
        }
        // A service account may own the standard token. Keep going with an
        // independent managed credential; a live service will reject it during
        // authentication, which never launches another player.
        if (token.isEmpty() && !QDir().mkpath(dataDirectory)) {
            error = tr("Не удалось создать каталог локального плеера: %1").arg(dataDirectory);
        } else if (token.isEmpty()) {
            // Serialize first-run credential creation between Manager instances.
            QLockFile tokenLock(QDir(dataDirectory).filePath(QStringLiteral("token.lock")));
            if (!tokenLock.tryLock(0))
                error = tr("Другой Manager подготавливает локальный токен. Повторите подключение.");
            else
                MediaBox::loadControlToken(dataDirectory, &token, &error);
        }
        if (!error.isEmpty()) {
            mClient->MediaBoxPlayerClient::disconnectFromPlayer();
            emit connectionError(error);
            return;
        }
        prepared.token = QString::fromLatin1(token);
    }
    mConnection = prepared;
    mClient->MediaBoxPlayerClient::connectToPlayer(prepared);
}

void LocalPlayerLauncher::disconnectFromPlayer()
{
    mConnectionRequested = true;
    ++mConnectionRevision;
    mClient->MediaBoxPlayerClient::disconnectFromPlayer();
}

void LocalPlayerLauncher::startLocalPlayer(const PlayerConnectionSettings &settings)
{
    const QString key = endpointKey(settings);
    if (!supportsLocalStart() || mLaunchAttempts.contains(key) || !isLocalHost(settings.host))
        return;
    mLaunchAttempts.insert(key);
    if (!validToken(settings.token)) {
        emit connectionError(tr("Для автозапуска локального плеера нужен корректный токен доступа."));
        return;
    }
    QString executable = QDir(QCoreApplication::applicationDirPath()).filePath(
        mKind == Kind::Audio ? QStringLiteral("MediaBoxPlayer") : QStringLiteral("MediaBoxVPlayer"));
#ifdef Q_OS_WIN
    executable += QStringLiteral(".exe");
#endif
    if (!QFileInfo(executable).isFile()) {
        emit connectionError(tr("Не удалось запустить локальный плеер: файл %1 не найден.").arg(executable));
        return;
    }
    const QString directory = managedDataDirectory(settings);
    if (!QDir().mkpath(directory)) {
        emit connectionError(tr("Не удалось создать каталог локального плеера: %1").arg(directory));
        return;
    }
    QProcess process;
    process.setProgram(executable);
    process.setArguments({QStringLiteral("--managed"), (mKind == Kind::Audio ? QStringLiteral("--listen") : QStringLiteral("--host")),
                          normalizedHost(settings.host),
                          QStringLiteral("--port"), QString::number(settings.port),
                          QStringLiteral("--data-dir"), directory});
    auto environment = QProcessEnvironment::systemEnvironment();
    environment.insert(mKind == Kind::Audio ? QStringLiteral("MEDIABOXPLAYER_CONTROL_TOKEN")
                                            : QStringLiteral("MEDIABOXVPLAYER_CONTROL_TOKEN"), settings.token);
    process.setProcessEnvironment(environment);
    process.setWorkingDirectory(QCoreApplication::applicationDirPath());
    process.setStandardOutputFile(QProcess::nullDevice());
    process.setStandardErrorFile(QProcess::nullDevice());
#ifdef Q_OS_WIN
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
    });
#endif
    qint64 processId = 0;
    if (!process.startDetached(&processId)) {
        emit connectionError(tr("Не удалось запустить локальный плеер: %1").arg(process.errorString()));
        return;
    }
    // The normal authenticated client retry obtains the confirmed state. The
    // detached player remains alive, including playback, when Manager exits.
    emit localPlayerStarted(processId);
}

void LocalPlayerLauncher::reloadConnection()
{
    const auto connection = mKind == Kind::Audio ? Settings().playerConnection()
                                                : Settings().videoPlayerConnection();
    if (connection.token.isEmpty() && (!supportsLocalStart() || !isLocalHost(connection.host)))
        disconnectFromPlayer();
    else
        connectToPlayer(connection);
}
