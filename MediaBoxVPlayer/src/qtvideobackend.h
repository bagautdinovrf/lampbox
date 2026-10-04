#pragma once

#include "audiobackend.h"

class QAudioOutput;
class QMediaPlayer;
class QVideoWidget;

namespace MediaBox {

class QtVideoBackend final : public AudioBackend
{
public:
    explicit QtVideoBackend(QVideoWidget *video, QObject *parent = nullptr);
    ~QtVideoBackend() override;
    void setSource(const QUrl &source) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(qint64 positionMs) override;
    void setVolume(int percent) override;
    void setMuted(bool muted) override;

private:
    QVideoWidget *m_video;
    QAudioOutput *m_audio;
    QMediaPlayer *m_player = nullptr;
};

} // namespace MediaBox
