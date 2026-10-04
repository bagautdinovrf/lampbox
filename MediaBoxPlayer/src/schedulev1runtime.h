#pragma once

#include "schedulecore/schedulev1.h"

#include <QJsonObject>
#include <QSet>
#include <QSqlDatabase>
#include <QTimer>

#include <functional>
#include <limits>

namespace MediaBox {

// Durable state belongs to the player, never to the published document.
// Selecting a track is speculative; only confirmStarted consumes its entry.
class ScheduleV1Runtime final
{
public:
    struct Track {
        QString playbackId, scheduleId, publicationId, playlistId, entryId;
        QString path, name, mixKey;
        QString deviation;
        int volumePercent = 100;
        int nextPhase = 0;
        QJsonObject cursor;
        bool event = false;
        QString eventKey;
        bool isValid() const { return !path.isEmpty(); }
    };

    explicit ScheduleV1Runtime(QString databasePath = {},
                               std::function<QDateTime()> clock = [] { return QDateTime::currentDateTime(); },
                               const QString &mediaType = QStringLiteral("audio"));
    ~ScheduleV1Runtime();
    QString restore();
    QJsonArray supportedCapabilities() const;
    QString pruneHistory();
    QString accept(const QByteArray &bytes, const QString &contentRoot,
                   const QJsonObject &active = {}, const QDateTime &now = QDateTime::currentDateTime(),
                   const QString &activePath = {});
    QString loadPublication(const QString &activePath, const QString &contentRoot,
                            const QDateTime &now = QDateTime::currentDateTime());
    QString setScheduledPlayback(bool enabled);
    bool scheduledPlaybackEnabled() const;
    bool available() const { return !m_document.object.isEmpty(); }
    const ScheduleV1::Document &document() const { return m_document; }
    QString diagnostic() const {
        QStringList messages{m_recoveryDiagnostic, m_sourceError, m_storageError, m_maintenanceError, m_diagnostic};
        messages.removeAll(QString());
        messages.removeDuplicates();
        return messages.join(u'\n');
    }
    const QString &contentRoot() const { return m_contentRoot; }
    Track selectMusic(const QDateTime &now, const QSet<QString> &failedAssets = {});
    QString confirmStarted(const Track &track);
    QList<ScheduleV1::EventOccurrence> dueEvents(const QDateTime &now);
    Track startEvent(const ScheduleV1::EventOccurrence &event, const QDateTime &now);
    QString finishEvent(const Track &track, const QString &state);
    QString eventKey(const ScheduleV1::EventOccurrence &event) const;

private:
    QString open();
    QString readAccepted();
    QString writeState(const QString &scheduleId, const QJsonObject &state);
    QString assetPath(const QString &assetId) const;
    QJsonObject playlist(const QString &id) const;
    Track selectPlaylist(const QString &id, const QSet<QString> &failedAssets);
    QString sqlError() const;
    QString validateMedia(const ScheduleV1::Document &document) const;
    QString m_mediaType;
    QString m_databasePath, m_connectionName, m_contentRoot, m_diagnostic, m_recoveryDiagnostic, m_storageError;
    QString m_maintenanceError, m_sourceError;
    QSqlDatabase m_db;
    std::function<QDateTime()> m_clock;
    QTimer m_maintenanceTimer;
    qint64 m_retentionCutoffMs = std::numeric_limits<qint64>::lowest();
    ScheduleV1::Document m_document;
    QJsonObject m_state;
    bool m_opened = false;
};

} // namespace MediaBox
