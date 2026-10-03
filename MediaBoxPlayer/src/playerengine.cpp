#include "playerengine.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QTimer>

#include <cmath>

namespace MediaBox {
namespace {

constexpr int MaxQueueSize = 1000;
constexpr int MaxPathLength = 4096;

bool integerValue(const QJsonValue &value, qint64 minimum, qint64 maximum, qint64 *result)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || std::floor(number) != number
        || number < static_cast<double>(minimum) || number > static_cast<double>(maximum))
        return false;
    *result = static_cast<qint64>(number);
    return true;
}

QString fileError(const QString &path)
{
    if (path.isEmpty() || path.size() > MaxPathLength || path.contains(QChar::Null)
        || !QDir::isAbsolutePath(path))
        return QStringLiteral("Each path must be an absolute local path of at most 4096 characters.");
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || !info.isReadable())
        return QStringLiteral("Audio file does not exist or is not a readable regular file: %1").arg(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Cannot read audio file: %1").arg(path);
    return {};
}

} // namespace

PlayerEngine::PlayerEngine(AudioBackend *backend, QObject *parent)
    : QObject(parent), m_backend(backend)
{
    Q_ASSERT(m_backend);
    m_backend->setVolume(m_volumePercent);
    m_backend->setMuted(m_muted);
    connect(m_backend, &AudioBackend::stateChanged, this, [this](AudioBackend::State state) {
        if (m_ignoringBackendSignals || m_settingSource || m_currentIndex < 0 || m_state == QStringLiteral("error"))
            return;
        if (state == AudioBackend::State::Playing) {
            if (!m_wantsPlayback)
                return;
            m_state = QStringLiteral("playing");
        } else if (state == AudioBackend::State::Paused) {
            if (m_wantsPlayback || m_state == QStringLiteral("stopped"))
                return;
            m_state = QStringLiteral("paused");
        } else {
            // A backend can stop immediately before EndOfMedia. Preserve the
            // playback intent until the finish handler chooses the next entry.
            m_state = QStringLiteral("stopped");
        }
        emit statusChanged();
    });
    connect(m_backend, &AudioBackend::positionChanged, this, [this](qint64 value) {
        if (m_ignoringBackendSignals || m_currentIndex < 0)
            return;
        m_positionMs = qMax(qint64(0), value);
        emit statusChanged();
    });
    connect(m_backend, &AudioBackend::durationChanged, this, [this](qint64 value) {
        if (m_ignoringBackendSignals || m_currentIndex < 0)
            return;
        m_durationMs = qMax(qint64(0), value);
        emit statusChanged();
    });
    connect(m_backend, &AudioBackend::finished, this, &PlayerEngine::handleFinished);
    connect(m_backend, &AudioBackend::errorOccurred, this, &PlayerEngine::handleError);
}

QJsonObject PlayerEngine::status() const
{
    return {{QStringLiteral("state"), m_state},
            {QStringLiteral("playbackRequested"), m_wantsPlayback},
            {QStringLiteral("queue"), QJsonArray::fromStringList(m_queue)},
            {QStringLiteral("currentIndex"), m_currentIndex},
            {QStringLiteral("currentTrack"), m_currentIndex >= 0 ? m_queue.at(m_currentIndex) : QString()},
            {QStringLiteral("positionMs"), m_positionMs},
            {QStringLiteral("durationMs"), m_durationMs},
            {QStringLiteral("volumePercent"), m_volumePercent},
            {QStringLiteral("muted"), m_muted},
            {QStringLiteral("repeat"), m_repeat},
            {QStringLiteral("error"), m_error}};
}

QJsonObject PlayerEngine::success() const
{
    return {{QStringLiteral("ok"), true}, {QStringLiteral("status"), status()}};
}

QJsonObject PlayerEngine::failure(const QString &code, const QString &message) const
{
    return {{QStringLiteral("ok"), false},
            {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), code},
                                                  {QStringLiteral("message"), message}}},
            {QStringLiteral("status"), status()}};
}

QJsonObject PlayerEngine::execute(const QJsonObject &request)
{
    const auto invalid = [this](const QString &message) {
        return failure(QStringLiteral("invalid_arguments"), message);
    };
    if (!request.value(QStringLiteral("command")).isString())
        return failure(QStringLiteral("invalid_request"), QStringLiteral("command must be a string."));
    const QString command = request.value(QStringLiteral("command")).toString();
    QSet<QString> allowed{QStringLiteral("command"), QStringLiteral("id")};
    if (command == QStringLiteral("load"))
        allowed.unite({QStringLiteral("paths"), QStringLiteral("startIndex"), QStringLiteral("autoplay")});
    else if (command == QStringLiteral("enqueue"))
        allowed.insert(QStringLiteral("paths"));
    else if (command == QStringLiteral("seek"))
        allowed.insert(QStringLiteral("positionMs"));
    else if (command == QStringLiteral("volume") || command == QStringLiteral("mute"))
        allowed.insert(QStringLiteral("value"));
    else if (command == QStringLiteral("repeat"))
        allowed.insert(QStringLiteral("mode"));
    else if (command != QStringLiteral("status") && command != QStringLiteral("play")
             && command != QStringLiteral("pause") && command != QStringLiteral("stop")
             && command != QStringLiteral("next") && command != QStringLiteral("previous")
             && command != QStringLiteral("clear"))
        return failure(QStringLiteral("unknown_command"), QStringLiteral("Unknown command: %1").arg(command));
    for (auto it = request.begin(); it != request.end(); ++it) {
        if (!allowed.contains(it.key()))
            return invalid(QStringLiteral("Unknown field for %1: %2").arg(command, it.key()));
    }

    if (command == QStringLiteral("status"))
        return success();
    if (command == QStringLiteral("load") || command == QStringLiteral("enqueue")) {
        if (!request.value(QStringLiteral("paths")).isArray())
            return invalid(QStringLiteral("paths must be a non-empty array of absolute local file paths."));
        const auto paths = request.value(QStringLiteral("paths")).toArray();
        const auto total = paths.size() + (command == QStringLiteral("enqueue") ? m_queue.size() : 0);
        if (paths.isEmpty() || total > MaxQueueSize)
            return invalid(QStringLiteral("The queue must contain between 1 and 1000 files; use clear to empty it."));
        qint64 startIndex = 0;
        if (request.contains(QStringLiteral("startIndex"))
            && !integerValue(request.value(QStringLiteral("startIndex")), 0, paths.size() - 1, &startIndex))
            return invalid(QStringLiteral("startIndex must be an integer identifying an entry in paths."));
        if (request.contains(QStringLiteral("autoplay")) && !request.value(QStringLiteral("autoplay")).isBool())
            return invalid(QStringLiteral("autoplay must be a boolean."));

        QStringList validated;
        validated.reserve(paths.size());
        for (const auto &value : paths) {
            if (!value.isString())
                return invalid(QStringLiteral("Every entry in paths must be a string."));
            const QString path = value.toString();
            const QString error = fileError(path);
            if (!error.isEmpty())
                return failure(QStringLiteral("invalid_path"), error);
            validated.append(QDir::cleanPath(path));
        }
        // No playback or queue mutation occurs until every entry is validated.
        if (command == QStringLiteral("load")) {
            m_queue = validated;
            m_failedTracks.clear();
            selectTrack(static_cast<int>(startIndex), request.value(QStringLiteral("autoplay")).toBool(false));
        } else {
            m_queue.append(validated);
            if (m_currentIndex < 0)
                selectTrack(0, false);
            else
                emit statusChanged();
        }
        return success();
    }
    if (command == QStringLiteral("volume")) {
        qint64 volume = 0;
        if (!integerValue(request.value(QStringLiteral("value")), 0, 100, &volume))
            return invalid(QStringLiteral("value must be an integer from 0 to 100."));
        m_volumePercent = static_cast<int>(volume);
        m_backend->setVolume(m_volumePercent);
    } else if (command == QStringLiteral("mute")) {
        if (!request.value(QStringLiteral("value")).isBool())
            return invalid(QStringLiteral("value must be a boolean."));
        m_muted = request.value(QStringLiteral("value")).toBool();
        m_backend->setMuted(m_muted);
    } else if (command == QStringLiteral("repeat")) {
        const QString mode = request.value(QStringLiteral("mode")).toString();
        if (mode != QStringLiteral("off") && mode != QStringLiteral("all") && mode != QStringLiteral("one"))
            return invalid(QStringLiteral("mode must be off, all or one."));
        m_repeat = mode;
    } else if (command == QStringLiteral("clear")) {
        invalidateContinuation();
        m_wantsPlayback = false;
        m_currentIndex = -1;
        m_queue.clear();
        m_failedTracks.clear();
        m_sourceLoaded = false;
        m_backend->stop();
        m_backend->setSource(QUrl());
        m_state = QStringLiteral("stopped");
        m_positionMs = 0;
        m_durationMs = 0;
        m_error.clear();
    } else if (command == QStringLiteral("stop")) {
        stopPlayback();
    } else if (command == QStringLiteral("pause")) {
        invalidateContinuation();
        m_wantsPlayback = false;
        m_backend->pause();
        // A loading player can acknowledge pause only after decoding starts.
        // Cancel that pending start by stopping it when no state was reported.
        if (m_state == QStringLiteral("loading")) {
            m_backend->stop();
            m_state = QStringLiteral("stopped");
        }
    } else {
        if (m_currentIndex < 0)
            return failure(QStringLiteral("empty_queue"), QStringLiteral("The playback queue is empty."));
        if (command == QStringLiteral("play")) {
            invalidateContinuation();
            m_failedTracks.clear();
            m_wantsPlayback = true;
            const QString error = fileError(m_queue.at(m_currentIndex));
            if (!error.isEmpty()) {
                handleError(error);
            } else if (!m_sourceLoaded || m_state == QStringLiteral("error")) {
                selectTrack(m_currentIndex, true);
            } else {
                m_error.clear();
                if (m_state != QStringLiteral("playing"))
                    m_state = QStringLiteral("loading");
                m_backend->play();
            }
        } else if (command == QStringLiteral("seek")) {
            qint64 position = 0;
            if (!integerValue(request.value(QStringLiteral("positionMs")), 0, 9007199254740991LL, &position))
                return invalid(QStringLiteral("positionMs must be a non-negative safe integer."));
            if (m_durationMs > 0 && position > m_durationMs)
                return invalid(QStringLiteral("positionMs exceeds the current track duration."));
            invalidateContinuation();
            m_backend->seek(position);
        } else if (command == QStringLiteral("next") || command == QStringLiteral("previous")) {
            const bool autoplay = m_wantsPlayback;
            m_failedTracks.clear();
            int index = nextIndex(false);
            if (command == QStringLiteral("previous"))
                index = m_currentIndex > 0 ? m_currentIndex - 1
                    : (m_repeat == QStringLiteral("all") ? static_cast<int>(m_queue.size()) - 1 : 0);
            if (index < 0)
                stopPlayback();
            else
                selectTrack(index, autoplay);
        }
    }
    emit statusChanged();
    return success();
}

void PlayerEngine::invalidateContinuation()
{
    ++m_generation;
    m_pendingAdvance = false;
}

void PlayerEngine::selectTrack(int index, bool autoplay)
{
    invalidateContinuation();
    m_wantsPlayback = autoplay;
    m_settingSource = true;
    m_ignoringBackendSignals = true;
    m_currentIndex = index;
    m_backend->stop();
    m_ignoringBackendSignals = false;
    m_positionMs = 0;
    m_durationMs = 0;
    m_error.clear();
    m_state = autoplay ? QStringLiteral("loading") : QStringLiteral("stopped");
    m_sourceLoaded = false;
    const QString error = fileError(m_queue.at(index));
    if (!error.isEmpty()) {
        m_backend->setSource(QUrl());
        m_settingSource = false;
        handleError(error);
        return;
    }
    m_sourceLoaded = true;
    m_backend->setSource(QUrl::fromLocalFile(m_queue.at(index)));
    m_settingSource = false;
    if (autoplay && m_state != QStringLiteral("error"))
        m_backend->play();
    emit statusChanged();
}

void PlayerEngine::stopPlayback()
{
    invalidateContinuation();
    m_wantsPlayback = false;
    m_failedTracks.clear();
    m_backend->stop();
    m_backend->seek(0);
    m_positionMs = 0;
    m_state = QStringLiteral("stopped");
    m_error.clear();
}

int PlayerEngine::nextIndex(bool automatic) const
{
    if (automatic && m_repeat == QStringLiteral("one"))
        return m_currentIndex;
    if (m_currentIndex + 1 < m_queue.size())
        return m_currentIndex + 1;
    return m_repeat == QStringLiteral("all") ? 0 : -1;
}

void PlayerEngine::handleFinished()
{
    if (m_ignoringBackendSignals || m_currentIndex < 0 || !m_wantsPlayback || m_settingSource || m_state == QStringLiteral("error"))
        return;
    scheduleAdvance(false);
}

void PlayerEngine::handleError(const QString &message)
{
    if (m_ignoringBackendSignals || m_currentIndex < 0)
        return;
    m_sourceLoaded = false;
    m_state = QStringLiteral("error");
    m_error = message.isEmpty() ? QStringLiteral("Audio playback failed.") : message;
    m_failedTracks.insert(m_currentIndex);
    emit statusChanged();
    if (m_wantsPlayback)
        scheduleAdvance(true);
}

void PlayerEngine::scheduleAdvance(bool failed)
{
    if (m_pendingAdvance)
        return;
    m_pendingAdvance = true;
    const auto generation = m_generation;
    QTimer::singleShot(0, this, [this, generation, failed] {
        if (generation != m_generation || !m_wantsPlayback)
            return;
        m_pendingAdvance = false;
        if (failed || m_state == QStringLiteral("error")) {
            advanceAfterError();
            return;
        }
        m_failedTracks.clear();
        const int index = nextIndex(true);
        if (index < 0) {
            m_wantsPlayback = false;
            m_state = QStringLiteral("stopped");
            emit statusChanged();
        } else {
            selectTrack(index, true);
        }
    });
}

void PlayerEngine::advanceAfterError()
{
    const int count = static_cast<int>(m_queue.size());
    for (int offset = 1; offset <= count; ++offset) {
        int candidate = m_currentIndex + offset;
        if (candidate >= count) {
            if (m_repeat != QStringLiteral("all"))
                break;
            candidate %= count;
        }
        if (!m_failedTracks.contains(candidate)) {
            selectTrack(candidate, true);
            return;
        }
    }
    m_wantsPlayback = false;
    m_backend->stop();
    m_state = QStringLiteral("error");
    emit statusChanged();
}

} // namespace MediaBox
