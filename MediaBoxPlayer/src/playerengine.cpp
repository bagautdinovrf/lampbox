#include "playerengine.h"

#include <QDir>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <utility>

namespace MediaBox {
namespace {

constexpr int MaxQueueSize = 1000;

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
    return playbackFileError(path);
}

} // namespace

PlayerEngine::PlayerEngine(AudioBackend *backend, QObject *parent, std::function<QDateTime()> clock)
    : QObject(parent), m_backend(backend), m_clock(std::move(clock))
{
    Q_ASSERT(m_backend);
    Q_ASSERT(m_clock);
    m_scheduleTimer.setInterval(1000);
    connect(&m_scheduleTimer, &QTimer::timeout, this, [this] { evaluateSchedule(m_clock()); });
    m_backend->setVolume(m_volumePercent);
    m_backend->setMuted(m_muted);
    connect(m_backend, &AudioBackend::stateChanged, this, [this](AudioBackend::State state) {
        if (m_ignoringBackendSignals || m_settingSource || m_currentIndex < 0 || m_state == QStringLiteral("error"))
            return;
        if (state == AudioBackend::State::Playing) {
            if (!m_wantsPlayback)
                return;
            m_state = QStringLiteral("playing");
            applyResumePosition();
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
        applyResumePosition();
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
            {QStringLiteral("order"), m_order},
            {QStringLiteral("playbackMode"), m_playbackMode},
            {QStringLiteral("channelName"), m_channelName},
            {QStringLiteral("scheduleAvailable"), m_scheduleAvailable},
            {QStringLiteral("scheduleError"), m_scheduleError},
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
    else if (command == QStringLiteral("playChannel"))
        allowed.unite({QStringLiteral("name"), QStringLiteral("paths"), QStringLiteral("volume"), QStringLiteral("order")});
    else if (command == QStringLiteral("setSchedule") || command == QStringLiteral("schedule"))
        allowed.insert(QStringLiteral("schedule"));
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
    if (command == QStringLiteral("setSchedule") || command == QStringLiteral("schedule")) {
        const bool replace = command == QStringLiteral("setSchedule") || request.contains(QStringLiteral("schedule"));
        PlaybackSchedule replacement;
        if (replace) {
            if (!request.value(QStringLiteral("schedule")).isObject())
                return invalid(QStringLiteral("schedule must be an object containing channels and adverts."));
            const QString error = PlaybackSchedule::decode(request.value(QStringLiteral("schedule")).toObject(), &replacement);
            if (!error.isEmpty())
                return failure(QStringLiteral("invalid_schedule"), error);
        } else if (!m_scheduleAvailable) {
            return failure(QStringLiteral("schedule_unavailable"), QStringLiteral("Load a schedule snapshot before activating scheduled playback."));
        }
        // Validate the entire snapshot before changing either queue or mode.
        if (replace) {
            m_schedule = std::move(replacement);
            m_scheduleAvailable = true;
            if (m_runningAdvert) {
                const auto current = std::find_if(m_schedule.adverts.cbegin(), m_schedule.adverts.cend(),
                    [this](const ScheduledAdvert &advert) { return advert.rule.stableId == m_runningAdvertId && advert.paths == m_queue; });
                if (current == m_schedule.adverts.cend()) {
                    m_runningAdvert = false;
                    m_runningAdvertId.clear();
                    clearPlaybackQueue();
                }
            }
            // A removed or changed block must not start from the old snapshot.
            QList<ScheduledAdvert> pending;
            for (const auto &old : std::as_const(m_pendingAdverts)) {
                const auto updated = std::find_if(m_schedule.adverts.cbegin(), m_schedule.adverts.cend(),
                    [&old](const ScheduledAdvert &advert) { return advert.rule.stableId == old.rule.stableId && advert.paths == old.paths; });
                if (updated != m_schedule.adverts.cend())
                    pending.append(*updated);
            }
            m_pendingAdverts = std::move(pending);
        }
        if (command == QStringLiteral("schedule")) {
            m_playbackMode = QStringLiteral("schedule");
            m_scheduleTimer.start();
        }
        if (m_playbackMode == QStringLiteral("schedule"))
            evaluateSchedule(m_clock());
        emit statusChanged();
        return success();
    }
    if (command == QStringLiteral("load") || command == QStringLiteral("enqueue")
        || command == QStringLiteral("playChannel")) {
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
        qint64 channelVolume = 100;
        QString channelName;
        QString channelOrder = QStringLiteral("shuffle_cycle");
        if (command == QStringLiteral("playChannel")) {
            if (request.contains(QStringLiteral("order"))) {
                channelOrder = request.value(QStringLiteral("order")).toString();
                if (channelOrder != QStringLiteral("sequential") && channelOrder != QStringLiteral("shuffle_cycle"))
                    return invalid(QStringLiteral("order must be sequential or shuffle_cycle."));
            }
            if (!request.value(QStringLiteral("name")).isString())
                return invalid(QStringLiteral("name must be a non-empty channel name of at most 256 characters."));
            channelName = request.value(QStringLiteral("name")).toString();
            if (channelName.trimmed().isEmpty() || channelName.size() > 256 || channelName.contains(QChar::Null))
                return invalid(QStringLiteral("name must be a non-empty channel name of at most 256 characters."));
            if (!integerValue(request.value(QStringLiteral("volume")), 0, 100, &channelVolume))
                return invalid(QStringLiteral("volume must be an integer from 0 to 100."));
            if (!m_playbackAvailable)
                return failure(QStringLiteral("playback_unavailable"), QStringLiteral("The playback output is unavailable."));
        }

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
        switchToManual(true);
        if (command == QStringLiteral("playChannel")) {
            m_queue = validated;
            m_failedTracks.clear();
            m_channelName = channelName;
            m_repeat = QStringLiteral("all");
            m_order = channelOrder;
            m_remainingTracks.clear();
            m_cycleTracks.clear();
            m_volumePercent = static_cast<int>(channelVolume);
            m_backend->setVolume(m_volumePercent);
            m_currentIndex = -1;
            selectTrack(m_order == QStringLiteral("shuffle_cycle") ? randomIndex() : 0, true);
        } else if (command == QStringLiteral("load")) {
            m_order = QStringLiteral("sequential");
            m_remainingTracks.clear();
            m_cycleTracks.clear();
            m_queue = validated;
            m_failedTracks.clear();
            selectTrack(static_cast<int>(startIndex), request.value(QStringLiteral("autoplay")).toBool(false));
        } else {
            for (int index = int(m_queue.size()); index < total; ++index)
                m_remainingTracks.append(index);
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
        switchToManual(true);
        clearPlaybackQueue();
    } else if (command == QStringLiteral("stop")) {
        switchToManual();
        stopPlayback();
    } else if (command == QStringLiteral("pause")) {
        switchToManual();
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
            if (!m_playbackAvailable)
                return failure(QStringLiteral("playback_unavailable"), QStringLiteral("The playback output is unavailable."));
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
            if (index < 0) {
                if (m_runningAdvert && m_playbackMode == QStringLiteral("schedule"))
                    finishAdvert();
                else
                    stopPlayback();
            } else
                selectTrack(index, autoplay);
        }
    }
    emit statusChanged();
    return success();
}

void PlayerEngine::switchToManual(bool clearChannelName)
{
    m_playbackMode = QStringLiteral("manual");
    m_scheduleTimer.stop();
    m_activeChannelId.clear();
    m_activeChannelVolume = -1;
    m_runningAdvert = false;
    m_runningAdvertId.clear();
    m_pendingAdverts.clear();
    m_interruptedChannel = {};
    m_scheduleError.clear();
    if (clearChannelName)
        m_channelName.clear();
}

void PlayerEngine::clearPlaybackQueue()
{
    invalidateContinuation();
    m_wantsPlayback = false;
    m_currentIndex = -1;
    m_queue.clear();
    m_remainingTracks.clear();
    m_cycleTracks.clear();
    m_failedTracks.clear();
    m_sourceLoaded = false;
    m_backend->stop();
    m_backend->setSource(QUrl());
    m_state = QStringLiteral("stopped");
    m_positionMs = 0;
    m_durationMs = 0;
    m_error.clear();
}

void PlayerEngine::setPlaybackAvailable(bool available)
{
    if (m_playbackAvailable == available)
        return;
    m_playbackAvailable = available;
    if (!available) {
        m_runningAdvert = false;
        m_runningAdvertId.clear();
        m_pendingAdverts.clear();
        m_interruptedChannel = {};
        m_activeChannelId.clear();
        m_activeChannelVolume = -1;
        stopPlayback();
    } else if (m_playbackMode == QStringLiteral("schedule")) {
        evaluateSchedule(m_clock());
    }
    emit statusChanged();
}

void PlayerEngine::evaluateSchedule(const QDateTime &at)
{
    if (m_playbackMode != QStringLiteral("schedule") || !m_scheduleAvailable || !m_playbackAvailable)
        return;
    const auto snapshot = ScheduleCore::evaluate(m_schedule.channelRules(), m_schedule.advertRules(), at, 1);
    QStringList errors;
    if (!at.isValid())
        errors.append(QStringLiteral("Некорректная дата или время расписания."));
    for (const auto &channel : snapshot.channels)
        if (!channel.valid || channel.status == QStringLiteral("Требует проверки"))
            errors.append(channel.name + QStringLiteral(": ") + channel.reason);
    if (!at.isValid() || snapshot.activeRows.size() > 1) {
        if (snapshot.activeRows.size() > 1)
            errors.prepend(snapshot.currentSummary);
        m_runningAdvert = false;
        m_runningAdvertId.clear();
        m_pendingAdverts.clear();
        m_interruptedChannel = {};
    }
    if (snapshot.activeRows.size() == 1) {
        const auto &channel = m_schedule.channels.at(snapshot.activeRows.first());
        if (channel.paths.isEmpty())
            errors.append(QStringLiteral("Канал «%1» не содержит файлов для воспроизведения.").arg(channel.rule.name));
    }

    if (at.isValid() && snapshot.activeRows.size() <= 1) {
        const qint64 minute = at.toSecsSinceEpoch() / 60;
        for (const auto &advert : std::as_const(m_schedule.adverts)) {
            // Preview and runtime both use ScheduleCore, including calendar,
            // compiled frequency phase and ambiguous local-time handling.
            if (!snapshot.exactAdvertsNow.contains(advert.rule.name)
                || ScheduleCore::evaluate({}, {advert.rule}, at, 1).exactAdvertsNow.isEmpty())
                continue;
            if (advert.paths.isEmpty()) {
                errors.append(QStringLiteral("Реклама «%1» не содержит файлов для воспроизведения.").arg(advert.rule.name));
                continue;
            }
            auto &fired = m_firedAdverts[minute];
            if (fired.contains(advert.rule.stableId))
                continue;
            fired.insert(advert.rule.stableId);
            m_pendingAdverts.append(advert);
        }
        // Bound memory while retaining deduplication through clock corrections
        // and a manual/schedule toggle within the same minute.
        while (m_firedAdverts.size() > 1440)
            m_firedAdverts.erase(m_firedAdverts.begin());
    }

    const QString scheduleError = errors.join(QStringLiteral("\n"));
    const bool changedError = m_scheduleError != scheduleError;
    m_scheduleError = scheduleError;
    if (!m_runningAdvert) {
        if (at.isValid() && snapshot.activeRows.size() <= 1 && !m_pendingAdverts.isEmpty())
            startNextAdvert();
        else
            applyScheduledChannel(snapshot);
    }
    if (changedError)
        emit statusChanged();
}

void PlayerEngine::applyScheduledChannel(const ScheduleCore::Snapshot &snapshot)
{
    if (snapshot.activeRows.size() != 1) {
        const bool changed = !m_queue.isEmpty() || !m_channelName.isEmpty();
        if (!m_queue.isEmpty() || m_sourceLoaded || m_state != QStringLiteral("stopped"))
            clearPlaybackQueue();
        m_activeChannelId.clear();
        m_activeChannelVolume = -1;
        m_channelName.clear();
        m_interruptedChannel = {};
        if (changed)
            emit statusChanged();
        return;
    }
    const auto &channel = m_schedule.channels.at(snapshot.activeRows.first());
    const bool sameQueue = m_activeChannelId == channel.rule.stableId && m_queue == channel.paths
        && m_order == channel.order;
    const bool nameChanged = m_channelName != channel.rule.name;
    m_channelName = channel.rule.name;
    const bool volumeChanged = !sameQueue || m_activeChannelVolume != channel.rule.volume;
    if (volumeChanged) {
        m_activeChannelVolume = channel.rule.volume;
        m_volumePercent = channel.rule.volume;
        m_backend->setVolume(m_volumePercent);
    }
    if (sameQueue) {
        if (nameChanged || volumeChanged)
            emit statusChanged();
        return;
    }
    m_activeChannelId = channel.rule.stableId;
    if (channel.paths.isEmpty()) {
        m_interruptedChannel = {};
        clearPlaybackQueue();
        emit statusChanged();
        return;
    }
    m_queue = channel.paths;
    m_repeat = QStringLiteral("all");
    m_order = channel.order;
    m_remainingTracks.clear();
    m_cycleTracks.clear();
    m_failedTracks.clear();
    const bool resume = m_interruptedChannel.id == channel.rule.stableId
        && m_interruptedChannel.paths == channel.paths && m_interruptedChannel.index >= 0
        && m_interruptedChannel.index < channel.paths.size();
    if (resume && m_interruptedChannel.order == m_order) {
        m_remainingTracks = m_interruptedChannel.remainingTracks;
        m_repeat = m_interruptedChannel.repeat;
        m_cycleTracks = m_interruptedChannel.cycleTracks;
    }
    if (!resume)
        m_currentIndex = -1;
    const int index = resume ? m_interruptedChannel.index
        : (m_order == QStringLiteral("shuffle_cycle") ? randomIndex() : 0);
    const qint64 position = resume ? m_interruptedChannel.positionMs : 0;
    m_interruptedChannel = {};
    selectTrack(index, true, position);
}

void PlayerEngine::startNextAdvert()
{
    if (m_pendingAdverts.isEmpty() || m_playbackMode != QStringLiteral("schedule") || !m_playbackAvailable)
        return;
    if (!m_activeChannelId.isEmpty() && m_currentIndex >= 0 && m_wantsPlayback
        && m_state != QStringLiteral("error")) {
        m_interruptedChannel = {m_activeChannelId, m_queue, m_currentIndex, m_positionMs,
                                m_remainingTracks, m_order, m_repeat, m_cycleTracks};
    }
    const auto advert = m_pendingAdverts.takeFirst();
    m_runningAdvert = true;
    m_runningAdvertId = advert.rule.stableId;
    m_activeChannelId.clear();
    m_activeChannelVolume = -1;
    m_channelName = advert.rule.name;
    m_queue = advert.paths;
    m_repeat = QStringLiteral("off");
    m_order = QStringLiteral("sequential");
    m_remainingTracks.clear();
    m_cycleTracks.clear();
    m_volumePercent = advert.rule.volume;
    m_backend->setVolume(m_volumePercent);
    m_failedTracks.clear();
    selectTrack(0, true);
}

void PlayerEngine::finishAdvert()
{
    m_runningAdvert = false;
    m_runningAdvertId.clear();
    evaluateSchedule(m_clock());
}

void PlayerEngine::invalidateContinuation()
{
    ++m_generation;
    m_pendingAdvance = false;
    m_resumePositionMs = 0;
}

void PlayerEngine::selectTrack(int index, bool autoplay, qint64 resumePositionMs)
{
    invalidateContinuation();
    m_resumePositionMs = qMax(qint64(0), resumePositionMs);
    autoplay = autoplay && m_playbackAvailable;
    m_wantsPlayback = autoplay;
    m_settingSource = true;
    m_ignoringBackendSignals = true;
    m_currentIndex = index;
    m_remainingTracks.removeAll(index);
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
    if (m_resumePositionMs > 0 && m_sourceLoaded && m_state != QStringLiteral("error")) {
        // Some backends accept an early seek; retain it until decoding confirms
        // playback or a duration so an asynchronous source load cannot lose it.
        m_backend->seek(m_resumePositionMs);
        applyResumePosition();
    }
    emit statusChanged();
}

void PlayerEngine::applyResumePosition()
{
    if (m_resumePositionMs <= 0 || !m_sourceLoaded || m_settingSource
        || (m_durationMs <= 0 && m_state != QStringLiteral("playing")))
        return;
    const qint64 position = m_durationMs > 0 ? qMin(m_resumePositionMs, m_durationMs) : m_resumePositionMs;
    m_resumePositionMs = 0;
    m_backend->seek(position);
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

int PlayerEngine::randomIndex()
{
    m_remainingTracks.removeIf([this](int index) { return m_failedTracks.contains(index); });
    if (m_remainingTracks.isEmpty()) {
        for (int index = 0; index < m_queue.size(); ++index)
            m_remainingTracks.append(index);
        std::shuffle(m_remainingTracks.begin(), m_remainingTracks.end(), *QRandomGenerator::global());
        // A fresh random draw can equal the last cycle; enforce a new order.
        if (m_remainingTracks.size() > 1 && m_remainingTracks == m_cycleTracks)
            std::swap(m_remainingTracks[0], m_remainingTracks[1]);
        m_cycleTracks = m_remainingTracks;
    }
    for (int index : std::as_const(m_remainingTracks))
        if (!m_failedTracks.contains(index))
            return index;
    return -1;
}

int PlayerEngine::nextIndex(bool automatic)
{
    if (automatic && !m_runningAdvert && m_repeat == QStringLiteral("one"))
        return m_currentIndex;
    if (!m_runningAdvert && m_order == QStringLiteral("shuffle_cycle")) {
        m_remainingTracks.removeIf([this](int index) { return m_failedTracks.contains(index); });
        if (m_remainingTracks.isEmpty() && m_repeat != QStringLiteral("all"))
            return -1;
        return randomIndex();
    }
    if (m_currentIndex + 1 < m_queue.size())
        return m_currentIndex + 1;
    return !m_runningAdvert && m_repeat == QStringLiteral("all") ? 0 : -1;
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
            if (m_runningAdvert && m_playbackMode == QStringLiteral("schedule")) {
                finishAdvert();
                return;
            }
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
    if (!m_runningAdvert && m_order == QStringLiteral("shuffle_cycle")) {
        const int candidate = nextIndex(false);
        if (candidate >= 0) {
            selectTrack(candidate, true);
            return;
        }
        m_wantsPlayback = false;
        m_backend->stop();
        m_state = QStringLiteral("error");
        emit statusChanged();
        return;
    }
    const int count = static_cast<int>(m_queue.size());
    for (int offset = 1; offset <= count; ++offset) {
        int candidate = m_currentIndex + offset;
        if (candidate >= count) {
            if (m_runningAdvert || m_repeat != QStringLiteral("all"))
                break;
            candidate %= count;
        }
        if (!m_failedTracks.contains(candidate)) {
            selectTrack(candidate, true);
            return;
        }
    }
    if (m_runningAdvert && m_playbackMode == QStringLiteral("schedule")) {
        finishAdvert();
        return;
    }
    m_wantsPlayback = false;
    m_backend->stop();
    m_state = QStringLiteral("error");
    emit statusChanged();
}

} // namespace MediaBox
