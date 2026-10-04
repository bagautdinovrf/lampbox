#ifndef LOCALPLAYERLAUNCHER_H
#define LOCALPLAYERLAUNCHER_H

#include "mediaboxplayerclient.h"
#include <QSet>

// Shared application lifecycle for local audio and video players. Pure TCP
// clients deliberately do not instantiate this helper or launch processes.
class LocalPlayerLauncher final : public QObject
{
    Q_OBJECT
public:
    enum class Kind { Audio, Video };
    explicit LocalPlayerLauncher(MediaBoxPlayerClient *client, Kind kind);
    static bool isLocalHost(const QString &host);
    static bool supportsLocalStart();
    void connectToPlayer(const PlayerConnectionSettings &settings);
    void disconnectFromPlayer();
    void reloadConnection();

signals:
    void localPlayerStarted(qint64 processId);
    void connectionError(QString message);

private:
    QString managedDataDirectory(const PlayerConnectionSettings &settings) const;
    void startLocalPlayer(const PlayerConnectionSettings &settings);
    MediaBoxPlayerClient *mClient;
    Kind mKind;
    PlayerConnectionSettings mConnection;
    QSet<QString> mLaunchAttempts;
    quint64 mConnectionRevision = 0;
    bool mConnectionRequested = false;
};

#endif
