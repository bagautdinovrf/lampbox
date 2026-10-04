#pragma once

#include "audiobackend.h"
#include "playbackschedule.h"
#include "schedulev1runtime.h"

#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <QTimer>

#include <functional>

namespace MediaBox {

class PlayerEngine final : public QObject
{
    Q_OBJECT
public:
    // backend must remain alive for the lifetime of this engine.
    explicit PlayerEngine(AudioBackend *backend, QObject *parent = nullptr,
                          std::function<QDateTime()> clock = [] { return QDateTime::currentDateTime(); },
                          const QString &runtimePath = {});

    QJsonObject execute(const QJsonObject &request);
    QJsonObject status() const;
    void evaluateSchedule(const QDateTime &at);
    void setPlaybackAvailable(bool available);
    // Called by the actual audio process after output and status observers exist.
    QString restoreScheduledPlayback();

signals:
    void statusChanged();

private:
    QJsonObject success() const;
    QJsonObject failure(const QString &code, const QString &message) const;
    void invalidateContinuation();
    void selectTrack(int index, bool autoplay, qint64 resumePositionMs = 0);
    void stopPlayback();
    void handleFinished();
    void handleError(const QString &message);
    void scheduleAdvance(bool failed);
    void advanceAfterError();
    int nextIndex(bool automatic);
    int randomIndex();
    void switchToManual(bool clearChannelName = false);
    void clearPlaybackQueue();
    void applyScheduledChannel(const ScheduleCore::Snapshot &snapshot);
    void startNextAdvert();
    void finishAdvert();
    void applyResumePosition();
    void evaluateV1(const QDateTime &at, bool trackBoundary = false);
    void launchV1Track(const ScheduleV1Runtime::Track &track, qint64 position = 0);
    void finishV1Track(bool failed);

    AudioBackend *m_backend;
    QStringList m_queue;
    int m_currentIndex = -1;
    QString m_state = QStringLiteral("stopped");
    QString m_repeat = QStringLiteral("off");
    QString m_order = QStringLiteral("sequential");
    QList<int> m_remainingTracks;
    QList<int> m_cycleTracks;
    QString m_error;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;
    qint64 m_resumePositionMs = 0;
    int m_volumePercent = 100;
    bool m_muted = false;
    bool m_wantsPlayback = false;
    bool m_ignoringBackendSignals = false;
    bool m_settingSource = false;
    bool m_sourceLoaded = false;
    bool m_pendingAdvance = false;
    quint64 m_generation = 0;
    QSet<int> m_failedTracks;
    std::function<QDateTime()> m_clock;
    QTimer m_scheduleTimer;
    PlaybackSchedule m_schedule;
    bool m_scheduleAvailable = false;
    bool m_playbackAvailable = true;
    QString m_playbackMode = QStringLiteral("manual");
    QString m_channelName;
    QString m_scheduleError;
    QString m_activeChannelId;
    int m_activeChannelVolume = -1;
    bool m_runningAdvert = false;
    QString m_runningAdvertId;
    QList<ScheduledAdvert> m_pendingAdverts;
    struct InterruptedChannel {
        QString id;
        QStringList paths;
        int index = -1;
        qint64 positionMs = 0;
        QList<int> remainingTracks;
        QString order;
        QString repeat;
        QList<int> cycleTracks;
    } m_interruptedChannel;
    QMap<qint64, QSet<QString>> m_firedAdverts;
    ScheduleV1Runtime m_v1;
    bool m_usesV1 = false;
    bool m_v1Started = false;
    ScheduleV1Runtime::Track m_v1Track, m_v1Suspended;
    qint64 m_v1SuspendedPosition = 0;
    QSet<QString> m_v1FailedAssets;
};

} // namespace MediaBox
