#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

namespace MediaBox {

// The engine does not own its backend. Keeping this interface independent of
// Qt Multimedia also allows queue and protocol tests to run without an audio device.
class AudioBackend : public QObject
{
    Q_OBJECT
public:
    enum class State { Stopped, Playing, Paused };
    Q_ENUM(State)

    explicit AudioBackend(QObject *parent = nullptr) : QObject(parent) {}
    ~AudioBackend() override = default;

    virtual void setSource(const QUrl &source) = 0;
    virtual void play() = 0;
    virtual void pause() = 0;
    virtual void stop() = 0;
    virtual void seek(qint64 positionMs) = 0;
    virtual void setVolume(int percent) = 0;
    virtual void setMuted(bool muted) = 0;

signals:
    void stateChanged(MediaBox::AudioBackend::State state);
    void finished();
    void errorOccurred(const QString &message);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
};

} // namespace MediaBox
