#include "videocontroller.h"
#include "localplayerlauncher.h"

VideoController::VideoController(QObject *parent)
    : MediaBoxVPlayerClient(parent), mLocalPlayer(new LocalPlayerLauncher(this, LocalPlayerLauncher::Kind::Video))
{
    connect(mLocalPlayer, &LocalPlayerLauncher::localPlayerStarted, this, &VideoController::localPlayerStarted);
}

bool VideoController::isLocalHost(const QString &host)
{
    return LocalPlayerLauncher::isLocalHost(host);
}

bool VideoController::supportsLocalStart()
{
    return LocalPlayerLauncher::supportsLocalStart();
}

void VideoController::connectToPlayer(const PlayerConnectionSettings &settings)
{
    mLocalPlayer->connectToPlayer(settings);
}

void VideoController::disconnectFromPlayer()
{
    mLocalPlayer->disconnectFromPlayer();
}

void VideoController::reloadConnection()
{
    mLocalPlayer->reloadConnection();
}
