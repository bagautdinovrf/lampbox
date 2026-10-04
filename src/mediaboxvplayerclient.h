#pragma once

#ifndef MEDIABOXVPLAYERCLIENT_H
#define MEDIABOXVPLAYERCLIENT_H

#include "mediaboxplayerclient.h"

#include <QList>

struct VideoDisplayStatus
{
    QString id;
    QString name;
    int index = 0;
};

struct VideoWindowStatus
{
    QString id;
    QString name;
    QString screen;
    QString actualScreen;
    bool fullscreen = true;
    PlayerStatus playback;
    QString restoreError;
};

struct VideoPlayerStatus
{
    QList<VideoDisplayStatus> displays;
    QList<VideoWindowStatus> windows;
    QString persistenceError;
};

Q_DECLARE_METATYPE(VideoDisplayStatus)
Q_DECLARE_METATYPE(VideoWindowStatus)
Q_DECLARE_METATYPE(VideoPlayerStatus)

// Each instance owns its connection and each command explicitly targets a
// window. Screen identifiers come from the remote player's display snapshot;
// an empty configured screen selects its primary display.
class MediaBoxVPlayerClient : public MediaBoxPlayerClient
{
    Q_OBJECT

public:
    static constexpr quint16 DefaultPort = 17656;

    explicit MediaBoxVPlayerClient(QObject *parent = nullptr);
    const VideoPlayerStatus &videoStatus() const { return m_videoStatus; }

    QString configureWindow(const QString &windowId, const QString &name,
                            const QString &screen, bool fullscreen);
    QString removeWindow(const QString &windowId);
    QString setFullscreen(const QString &windowId, bool fullscreen);
    QString load(const QString &windowId, const QStringList &paths,
                 int startIndex = 0, bool autoplay = false);
    QString enqueue(const QString &windowId, const QStringList &paths);
    QString setSchedule(const QString &windowId, const QJsonObject &schedule);
    QString startSchedule(const QString &windowId, const QJsonObject &schedule = {});
    QString playChannel(const QString &windowId, const QString &name,
                        const QStringList &paths, int volume,
                        const QString &order = QStringLiteral("shuffle_cycle"));
    QString play(const QString &windowId);
    QString pause(const QString &windowId);
    QString stop(const QString &windowId);
    QString next(const QString &windowId);
    QString previous(const QString &windowId);
    QString seek(const QString &windowId, qint64 positionMs);
    QString setVolume(const QString &windowId, int value);
    QString setMuted(const QString &windowId, bool muted);
    QString setRepeat(const QString &windowId, const QString &mode);
    QString clear(const QString &windowId);

signals:
    void videoStatusChanged(const VideoPlayerStatus &status);

protected:
    bool decodeStatus(const QJsonObject &object, QVariant *snapshot) const override;
    void applyStatus(const QVariant &snapshot) override;
    void publishStatus(const QVariant &snapshot) override;
    void resetStatus() override;

private:
    QString submitWindow(const QString &command, const QString &windowId,
                         QJsonObject arguments = {});
    VideoPlayerStatus m_videoStatus;
};

#endif // MEDIABOXVPLAYERCLIENT_H
