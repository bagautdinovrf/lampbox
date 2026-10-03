#pragma once

#include "audiobackend.h"

class QAudioOutput;
class QMediaPlayer;

namespace MediaBox {

class QtAudioBackend final : public AudioBackend
{
    Q_OBJECT
public:
    explicit QtAudioBackend(QObject *parent = nullptr);

    void setSource(const QUrl &source) override;
    void play() override;
    void pause() override;
    void stop() override;
    void seek(qint64 positionMs) override;
    void setVolume(int percent) override;
    void setMuted(bool muted) override;

private:
    QAudioOutput *m_output;
    QMediaPlayer *m_player = nullptr;
};

} // namespace MediaBox
