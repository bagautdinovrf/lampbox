#include "qtvideobackend.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QVideoWidget>

namespace MediaBox {

QtVideoBackend::QtVideoBackend(QVideoWidget *video, QObject *parent)
    : AudioBackend(parent), m_video(video), m_audio(new QAudioOutput(this))
{
}

QtVideoBackend::~QtVideoBackend()
{
    if (m_player) {
        disconnect(m_player, nullptr, this, nullptr);
        m_player->setVideoOutput(nullptr);
        m_player->stop();
    }
}

void QtVideoBackend::setSource(const QUrl &source)
{
    // A new decoder isolates queued completion/error signals from the old clip.
    if (m_player) {
        disconnect(m_player, nullptr, this, nullptr);
        m_player->stop();
        m_player->setVideoOutput(nullptr);
        m_player->setAudioOutput(nullptr);
        m_player->deleteLater();
        m_player = nullptr;
    }
    if (source.isEmpty())
        return;
    auto *player = new QMediaPlayer(this);
    m_player = player;
    player->setVideoOutput(m_video);
    // Video-only installations are valid even without an audio device.
    const auto device = QMediaDevices::defaultAudioOutput();
    if (!device.isNull()) {
        m_audio->setDevice(device);
        player->setAudioOutput(m_audio);
    }
    connect(player, &QMediaPlayer::playbackStateChanged, this,
            [this, player](QMediaPlayer::PlaybackState state) {
        if (player != m_player)
            return;
        switch (state) {
        case QMediaPlayer::PlayingState: emit stateChanged(State::Playing); break;
        case QMediaPlayer::PausedState: emit stateChanged(State::Paused); break;
        case QMediaPlayer::StoppedState: emit stateChanged(State::Stopped); break;
        }
    });
    connect(player, &QMediaPlayer::mediaStatusChanged, this,
            [this, player](QMediaPlayer::MediaStatus state) {
        if (player == m_player && state == QMediaPlayer::EndOfMedia)
            emit finished();
    });
    connect(player, &QMediaPlayer::errorOccurred, this,
            [this, player](QMediaPlayer::Error, const QString &message) {
        if (player == m_player)
            emit errorOccurred(message.isEmpty() ? QStringLiteral("Video playback failed.") : message);
    });
    connect(player, &QMediaPlayer::positionChanged, this, [this, player](qint64 value) {
        if (player == m_player)
            emit positionChanged(value);
    });
    connect(player, &QMediaPlayer::durationChanged, this, [this, player](qint64 value) {
        if (player == m_player)
            emit durationChanged(value);
    });
    player->setSource(source);
}

void QtVideoBackend::play()
{
    if (!m_player)
        return;
    const auto device = QMediaDevices::defaultAudioOutput();
    if (!device.isNull()) {
        m_audio->setDevice(device);
        m_player->setAudioOutput(m_audio);
    } else {
        m_player->setAudioOutput(nullptr);
    }
    m_player->play();
}

void QtVideoBackend::pause() { if (m_player) m_player->pause(); }
void QtVideoBackend::stop() { if (m_player) m_player->stop(); }
void QtVideoBackend::seek(qint64 positionMs) { if (m_player) m_player->setPosition(positionMs); }
void QtVideoBackend::setVolume(int percent) { m_audio->setVolume(static_cast<float>(percent) / 100.0F); }
void QtVideoBackend::setMuted(bool muted) { m_audio->setMuted(muted); }

} // namespace MediaBox
