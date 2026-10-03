#include "mediacontroller.h"
#include "settings.h"

#include <QTimer>

MediaController::MediaController(QObject *parent) : MediaBoxPlayerClient(parent)
{
    // Let the window subscribe to errors and state changes before connecting.
    QTimer::singleShot(0, this, &MediaController::reloadConnection);
}

void MediaController::reloadConnection()
{
    const auto connection = Settings().playerConnection();
    if (connection.token.isEmpty())
        disconnectFromPlayer();
    else
        connectToPlayer(connection);
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
