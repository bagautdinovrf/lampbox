#pragma once

#include "audiobackend.h"

#include <QJsonObject>
#include <QSet>
#include <QStringList>

namespace MediaBox {

class PlayerEngine final : public QObject
{
    Q_OBJECT
public:
    // backend must remain alive for the lifetime of this engine.
    explicit PlayerEngine(AudioBackend *backend, QObject *parent = nullptr);

    QJsonObject execute(const QJsonObject &request);
    QJsonObject status() const;

signals:
    void statusChanged();

private:
    QJsonObject success() const;
    QJsonObject failure(const QString &code, const QString &message) const;
    void invalidateContinuation();
    void selectTrack(int index, bool autoplay);
    void stopPlayback();
    void handleFinished();
    void handleError(const QString &message);
    void scheduleAdvance(bool failed);
    void advanceAfterError();
    int nextIndex(bool automatic) const;

    AudioBackend *m_backend;
    QStringList m_queue;
    int m_currentIndex = -1;
    QString m_state = QStringLiteral("stopped");
    QString m_repeat = QStringLiteral("off");
    QString m_error;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;
    int m_volumePercent = 100;
    bool m_muted = false;
    bool m_wantsPlayback = false;
    bool m_ignoringBackendSignals = false;
    bool m_settingSource = false;
    bool m_sourceLoaded = false;
    bool m_pendingAdvance = false;
    quint64 m_generation = 0;
    QSet<int> m_failedTracks;
};

} // namespace MediaBox
