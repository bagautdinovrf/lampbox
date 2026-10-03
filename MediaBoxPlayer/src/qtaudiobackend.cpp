#include "qtaudiobackend.h"

#include <QAudioDevice>
#include <QAudioOutput>
#include <QMediaDevices>
#include <QMediaPlayer>

namespace MediaBox {

QtAudioBackend::QtAudioBackend(QObject *parent)
    : AudioBackend(parent), m_output(new QAudioOutput(this))
{
    setVolume(100);
}

void QtAudioBackend::setSource(const QUrl &source)
{
    // Each source gets its own player: queued decoder events from a previous
    // source must never finish or fail the newly selected queue entry.
    if (m_player) {
        disconnect(m_player, nullptr, this, nullptr);
        m_player->stop();
        m_player->setAudioOutput(nullptr);
        m_player->deleteLater();
        m_player = nullptr;
    }
    if (source.isEmpty())
        return;

    auto *player = new QMediaPlayer(this);
    m_player = player;
    player->setAudioOutput(m_output);
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
            [this, player](QMediaPlayer::MediaStatus status) {
        if (player == m_player && status == QMediaPlayer::EndOfMedia)
            emit finished();
    });
    connect(player, &QMediaPlayer::errorOccurred, this,
            [this, player](QMediaPlayer::Error, const QString &message) {
        if (player == m_player)
            emit errorOccurred(message.isEmpty() ? QStringLiteral("Audio playback failed.") : message);
    });
    connect(player, &QMediaPlayer::positionChanged, this, [this, player](qint64 position) {
        if (player == m_player)
            emit positionChanged(position);
    });
    connect(player, &QMediaPlayer::durationChanged, this, [this, player](qint64 duration) {
        if (player == m_player)
            emit durationChanged(duration);
    });
    player->setSource(source);
}

void QtAudioBackend::play()
{
    if (!m_player)
        return;
    const auto device = QMediaDevices::defaultAudioOutput();
    if (device.isNull()) {
        emit errorOccurred(QStringLiteral("No audio output device is available."));
        return;
    }
    if (m_output->device() != device)
        m_output->setDevice(device);
    m_player->play();
}

void QtAudioBackend::pause()
{
    if (m_player)
        m_player->pause();
}

void QtAudioBackend::stop()
{
    if (m_player)
        m_player->stop();
}

void QtAudioBackend::seek(qint64 positionMs)
{
    if (m_player)
        m_player->setPosition(positionMs);
}

void QtAudioBackend::setVolume(int percent)
{
    m_output->setVolume(static_cast<float>(percent) / 100.0F);
}

void QtAudioBackend::setMuted(bool muted)
{
    m_output->setMuted(muted);
}

} // namespace MediaBox
