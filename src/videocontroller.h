#ifndef VIDEOCONTROLLER_H
#define VIDEOCONTROLLER_H

#include "mediaboxvplayerclient.h"

class LocalPlayerLauncher;

// Persistent application-level video connection, including local startup.
// All playback commands continue to use the authenticated TCP client.
class VideoController final : public MediaBoxVPlayerClient
{
    Q_OBJECT
public:
    explicit VideoController(QObject *parent = nullptr);
    static bool isLocalHost(const QString &host);
    static bool supportsLocalStart();
    void connectToPlayer(const PlayerConnectionSettings &settings) override;
    void disconnectFromPlayer() override;

public slots:
    void reloadConnection();

signals:
    void localPlayerStarted(qint64 processId);

private:
    LocalPlayerLauncher *mLocalPlayer;
};

#endif
