#pragma once

#ifndef MEDIABOXPLAYERCLIENT_H
#define MEDIABOXPLAYERCLIENT_H

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QStringList>
#include <QTimer>
#include <QVariant>

#include <optional>

class QTcpSocket;

struct PlayerConnectionSettings
{
    QString host = QStringLiteral("127.0.0.1");
    quint16 port = 17655;
    QString token;
};

struct PlayerStatus
{
    QString state = QStringLiteral("stopped");
    bool playbackRequested = false;
    QStringList queue;
    int currentIndex = -1;
    QString currentTrack;
    qint64 positionMs = 0;
    qint64 durationMs = 0;
    int volumePercent = 100;
    bool muted = false;
    QString repeat = QStringLiteral("off");
    QString error;
    QString playbackMode = QStringLiteral("manual");
    QString channelName;
    bool scheduleAvailable = false;
    QString scheduleError;
};

Q_DECLARE_METATYPE(PlayerConnectionSettings)
Q_DECLARE_METATYPE(PlayerStatus)

// All methods must be called in this object's Qt thread. The last status is a
// snapshot; it is current only while isReady() is true.
class MediaBoxPlayerClient : public QObject
{
    Q_OBJECT

public:
    enum class ConnectionState {
        Disconnected,
        Connecting,
        Synchronizing,
        Ready,
        Reconnecting,
        AuthenticationFailed,
        ProtocolMismatch
    };
    Q_ENUM(ConnectionState)

    struct Timing {
        int pollIntervalMs = 1000;
        int connectTimeoutMs = 3000;
        int responseTimeoutMs = 5000;
        int reconnectBaseMs = 1000;
        int reconnectMaximumMs = 30000;
    };

    explicit MediaBoxPlayerClient(QObject *parent = nullptr);
    ~MediaBoxPlayerClient() override;

    ConnectionState connectionState() const { return m_state; }
    const PlayerStatus &status() const { return m_status; }
    bool hasStatus() const { return m_hasStatus; }
    bool isReady() const { return m_state == ConnectionState::Ready; }
    void setTiming(const Timing &timing);

    virtual void connectToPlayer(const PlayerConnectionSettings &settings);
    virtual void disconnectFromPlayer();

    // An empty return value means local rejection (commandFailed has an empty
    // id). Commands are accepted only after the initial status synchronization.
    QString requestStatus();
    QString load(const QStringList &paths, int startIndex = 0, bool autoplay = false);
    QString enqueue(const QStringList &paths);
    QString play();
    QString pause();
    QString stop();
    QString next();
    QString previous();
    QString seek(qint64 positionMs);
    QString setVolume(int value);
    QString setMuted(bool muted);
    QString setRepeat(const QString &mode);
    QString clear();
    QString setSchedule(const QJsonObject &schedule);
    QString startSchedule(const QJsonObject &schedule = {});
    QString playChannel(const QString &name, const QStringList &paths, int volume);

signals:
    void connectionStateChanged(MediaBoxPlayerClient::ConnectionState state);
    void statusChanged(const PlayerStatus &status);
    void commandSucceeded(QString id, QString command);
    void commandFailed(QString id, QString command, QString code, QString message);
    void commandOutcomeUnknown(QString id, QString command);
    void commandCancelled(QString id, QString command);
    void connectionError(QString message);
    // TCP establishment failed; never emitted for authentication or replies.
    void connectionAttemptFailed(const PlayerConnectionSettings &settings);

protected:
    // Reuse the bounded, authenticated transport for players with a different
    // status schema. Decode without side effects, then apply and publish the
    // same stable snapshot around connection lifecycle notifications.
    virtual bool decodeStatus(const QJsonObject &object, QVariant *snapshot) const;
    virtual void applyStatus(const QVariant &snapshot);
    virtual void publishStatus(const QVariant &snapshot);
    virtual void resetStatus();
    static bool parsePlaybackStatus(const QJsonObject &object, PlayerStatus *status);
    static bool validMediaPaths(const QStringList &paths);
    QString submit(const QString &command, const QJsonObject &arguments = {},
                   bool synchronizing = false);
    QString reject(const QString &command, const QString &code, const QString &message);

private:
    struct Request {
        QString id;
        QString command;
        QByteArray frame;
    };

    void setState(ConnectionState state);
    void beginConnection();
    void closeTransport(ConnectionState state, const QString &reason);
    void failConnection(const QString &message,
                        ConnectionState state = ConnectionState::Reconnecting);
    void failConnectionAttempt(const QString &message);
    void readAvailable();
    bool processReply(const QByteArray &line);
    void sendNext();

    PlayerConnectionSettings m_settings;
    PlayerStatus m_status;
    ConnectionState m_state = ConnectionState::Disconnected;
    Timing m_timing;
    QTcpSocket *m_socket = nullptr;
    QTimer m_connectTimer;
    QTimer m_responseTimer;
    QTimer m_pollTimer;
    QTimer m_reconnectTimer;
    QByteArray m_input;
    QQueue<Request> m_queue;
    std::optional<Request> m_current;
    quint64 m_generation = 0;
    int m_reconnectDelayMs = 1000;
    bool m_wantsConnection = false;
    bool m_hasStatus = false;
    bool m_reading = false;
};

Q_DECLARE_METATYPE(MediaBoxPlayerClient::ConnectionState)

#endif // MEDIABOXPLAYERCLIENT_H
