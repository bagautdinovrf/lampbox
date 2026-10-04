#pragma once

#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H

#include "mediaboxplayerclient.h"

class LocalPlayerLauncher;

// Application-level entry point. All commands and telemetry use the TCP client.
class MediaController : public MediaBoxPlayerClient
{
    Q_OBJECT
public:
    explicit MediaController(QObject *parent = nullptr);
    static bool isLocalHost(const QString &host);
    static bool supportsLocalStart();
    void connectToPlayer(const PlayerConnectionSettings &settings) override;
    void disconnectFromPlayer() override;
    bool isPlaying() const;
    QString playTrack(const QString &track);
    QString playSchedule(const QJsonObject &schedule);

public slots:
    void reloadConnection();
    void refreshPlayer();

signals:
    void localPlayerStarted(qint64 processId);

private:
    LocalPlayerLauncher *mLocalPlayer;
};

#endif
