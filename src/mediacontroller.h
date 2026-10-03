#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H

#include "mediaboxplayerclient.h"

// Application-level entry point. All commands and telemetry use the TCP client.
class MediaController : public MediaBoxPlayerClient
{
    Q_OBJECT
public:
    explicit MediaController(QObject *parent = nullptr);
    bool isPlaying() const;
    QString playTrack(const QString &track);

public slots:
    void reloadConnection();
    void refreshPlayer();
};

#endif
