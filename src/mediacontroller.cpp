#include "mediacontroller.h"
#include "localplayerlauncher.h"

MediaController::MediaController(QObject *parent)
    : MediaBoxPlayerClient(parent), mLocalPlayer(new LocalPlayerLauncher(this, LocalPlayerLauncher::Kind::Audio))
{
    connect(mLocalPlayer, &LocalPlayerLauncher::localPlayerStarted, this, &MediaController::localPlayerStarted);
}

bool MediaController::isLocalHost(const QString &host)
{
    return LocalPlayerLauncher::isLocalHost(host);
}

bool MediaController::supportsLocalStart()
{
    return LocalPlayerLauncher::supportsLocalStart();
}

void MediaController::connectToPlayer(const PlayerConnectionSettings &settings)
{
    mLocalPlayer->connectToPlayer(settings);
}

void MediaController::disconnectFromPlayer()
{
    mLocalPlayer->disconnectFromPlayer();
}

void MediaController::reloadConnection()
{
    mLocalPlayer->reloadConnection();
}

bool MediaController::isPlaying() const
{
    return isReady() && status().state == QStringLiteral("playing");
}

QString MediaController::playTrack(const QString &track)
{
    return load({track}, 0, true);
}

void MediaController::refreshPlayer()
{
    requestStatus();
}
