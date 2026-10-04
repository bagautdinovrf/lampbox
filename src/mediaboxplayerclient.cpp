#include "mediaboxplayerclient.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QTcpSocket>
#include <QUuid>

#include <limits>
#include <memory>
#include <utility>

namespace {

constexpr qsizetype MaxRequestBytes = 1024 * 1024;
constexpr qsizetype MaxResponseBytes = 32 * 1024 * 1024;
constexpr qsizetype ReadChunkBytes = 64 * 1024;
constexpr qint64 MaxSeekPosition = 9007199254740991LL;
constexpr qsizetype MaxQueuedCommands = 256;

// The path belongs to the player machine, which can use a different OS.
bool validPath(const QString &path)
{
    if (path.isEmpty() || path.size() > 4096 || path.contains(QChar::Null))
        return false;
    const bool drivePath = path.size() >= 3
        && ((path.at(0) >= QLatin1Char('A') && path.at(0) <= QLatin1Char('Z'))
            || (path.at(0) >= QLatin1Char('a') && path.at(0) <= QLatin1Char('z')))
        && path.at(1) == QLatin1Char(':')
        && (path.at(2) == QLatin1Char('/') || path.at(2) == QLatin1Char('\\'));
    return path.startsWith(QLatin1Char('/'))
        || path.startsWith(QStringLiteral("\\\\")) || drivePath;
}

bool validPaths(const QStringList &paths)
{
    if (paths.isEmpty() || paths.size() > 1000)
        return false;
    for (const QString &path : paths) {
        if (!validPath(path))
            return false;
    }
    return true;
}

bool validRepeat(const QString &value)
{
    return value == QStringLiteral("off") || value == QStringLiteral("all")
        || value == QStringLiteral("one");
}

bool integer(const QJsonValue &value, qint64 minimum, qint64 maximum, qint64 *result)
{
    if (!value.isDouble())
        return false;
    // Qt preserves JSON integers internally. Unlike toDouble()/a cast, this
    // rejects fractional and out-of-qint64 values without overflow.
    const qint64 number = value.toInteger(minimum - 1);
    if (number < minimum || number > maximum)
        return false;
    *result = number;
    return true;
}

bool parseStatus(const QJsonObject &object, PlayerStatus *status)
{
    const auto state = object.value(QStringLiteral("state"));
    const auto playbackRequested = object.value(QStringLiteral("playbackRequested"));
    const auto queue = object.value(QStringLiteral("queue"));
    const auto currentTrack = object.value(QStringLiteral("currentTrack"));
    const auto muted = object.value(QStringLiteral("muted"));
    const auto repeat = object.value(QStringLiteral("repeat"));
    const auto error = object.value(QStringLiteral("error"));
    if (!state.isString() || !playbackRequested.isBool() || !queue.isArray()
        || !currentTrack.isString() || !muted.isBool() || !repeat.isString()
        || !error.isString())
        return false;

    const QString stateString = state.toString();
    if (stateString != QStringLiteral("stopped") && stateString != QStringLiteral("loading")
        && stateString != QStringLiteral("playing") && stateString != QStringLiteral("paused")
        && stateString != QStringLiteral("error"))
        return false;
    if (!validRepeat(repeat.toString()))
        return false;

    const QJsonArray entries = queue.toArray();
    if (entries.size() > 1000)
        return false;
    QStringList paths;
    paths.reserve(entries.size());
    for (const QJsonValue &entry : entries) {
        if (!entry.isString() || !validPath(entry.toString()))
            return false;
        paths.append(entry.toString());
    }

    qint64 index, position, duration, volume;
    if (!integer(object.value(QStringLiteral("currentIndex")), -1, 999, &index)
        || !integer(object.value(QStringLiteral("positionMs")), 0,
                    std::numeric_limits<qint64>::max(), &position)
        || !integer(object.value(QStringLiteral("durationMs")), 0,
                    std::numeric_limits<qint64>::max(), &duration)
        || !integer(object.value(QStringLiteral("volumePercent")), 0, 100, &volume))
        return false;

    if (paths.isEmpty()) {
        if (index != -1 || !currentTrack.toString().isEmpty()
            || stateString != QStringLiteral("stopped") || playbackRequested.toBool()
            || position != 0 || duration != 0)
            return false;
    } else if (index < 0 || index >= paths.size()
               || currentTrack.toString() != paths.at(index)) {
        return false;
    }

    status->state = stateString;
    status->playbackRequested = playbackRequested.toBool();
    status->queue = paths;
    status->currentIndex = static_cast<int>(index);
    status->currentTrack = currentTrack.toString();
    status->positionMs = position;
    status->durationMs = duration;
    status->volumePercent = static_cast<int>(volume);
    status->muted = muted.toBool();
    status->repeat = repeat.toString();
    status->error = error.toString();
    // Older v1 players omit these fields; accept their manual playback status.
    const auto mode = object.value(QStringLiteral("playbackMode"));
    const auto channel = object.value(QStringLiteral("channelName"));
    const auto available = object.value(QStringLiteral("scheduleAvailable"));
    const auto scheduleError = object.value(QStringLiteral("scheduleError"));
    if ((!mode.isUndefined() && (!mode.isString()
            || (mode.toString() != QStringLiteral("manual") && mode.toString() != QStringLiteral("schedule"))))
        || (!channel.isUndefined() && !channel.isString())
        || (!available.isUndefined() && !available.isBool())
        || (!scheduleError.isUndefined() && !scheduleError.isString()))
        return false;
    status->playbackMode = mode.toString(QStringLiteral("manual"));
    status->channelName = channel.toString();
    status->scheduleAvailable = available.toBool();
    status->scheduleError = scheduleError.toString();
    return true;
}

} // namespace

MediaBoxPlayerClient::MediaBoxPlayerClient(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<PlayerStatus>();
    qRegisterMetaType<PlayerConnectionSettings>();
    qRegisterMetaType<ConnectionState>();
    m_connectTimer.setParent(this);
    m_responseTimer.setParent(this);
    m_pollTimer.setParent(this);
    m_reconnectTimer.setParent(this);
    m_connectTimer.setSingleShot(true);
    m_responseTimer.setSingleShot(true);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_connectTimer, &QTimer::timeout, this, [this] {
        if (m_socket)
            failConnectionAttempt(tr("Истекло время подключения к плееру."));
    });
    connect(&m_responseTimer, &QTimer::timeout, this, [this] {
        if (m_current)
            failConnection(tr("Плеер не ответил вовремя."));
    });
    connect(&m_reconnectTimer, &QTimer::timeout, this, [this] {
        if (m_wantsConnection && m_state == ConnectionState::Reconnecting)
            beginConnection();
    });
    connect(&m_pollTimer, &QTimer::timeout, this, [this] {
        if (isReady() && !m_current && m_queue.isEmpty())
            requestStatus();
    });
}

MediaBoxPlayerClient::~MediaBoxPlayerClient()
{
    // Destruction does not emit lifecycle signals into partially destroyed UI.
    if (m_socket) {
        m_socket->disconnect(this);
        m_socket->abort();
    }
}

bool MediaBoxPlayerClient::parsePlaybackStatus(const QJsonObject &object, PlayerStatus *status)
{
    return parseStatus(object, status);
}

bool MediaBoxPlayerClient::validMediaPaths(const QStringList &paths)
{
    return validPaths(paths);
}

bool MediaBoxPlayerClient::decodeStatus(const QJsonObject &object, QVariant *snapshot) const
{
    PlayerStatus status;
    if (!parsePlaybackStatus(object, &status))
        return false;
    *snapshot = QVariant::fromValue(status);
    return true;
}

void MediaBoxPlayerClient::applyStatus(const QVariant &snapshot)
{
    m_status = snapshot.value<PlayerStatus>();
}

void MediaBoxPlayerClient::publishStatus(const QVariant &snapshot)
{
    const auto status = snapshot.value<PlayerStatus>();
    emit statusChanged(status);
}

void MediaBoxPlayerClient::resetStatus()
{
    m_status = PlayerStatus{};
}

void MediaBoxPlayerClient::setTiming(const Timing &timing)
{
    m_timing.pollIntervalMs = qMax(1, timing.pollIntervalMs);
    m_timing.connectTimeoutMs = qMax(1, timing.connectTimeoutMs);
    m_timing.responseTimeoutMs = qMax(1, timing.responseTimeoutMs);
    m_timing.reconnectBaseMs = qMax(1, timing.reconnectBaseMs);
    m_timing.reconnectMaximumMs = qMax(m_timing.reconnectBaseMs, timing.reconnectMaximumMs);
    m_reconnectDelayMs = m_timing.reconnectBaseMs;
    if (m_pollTimer.isActive())
        m_pollTimer.start(m_timing.pollIntervalMs);
}

void MediaBoxPlayerClient::setState(ConnectionState state)
{
    if (m_state == state)
        return;
    m_state = state;
    emit connectionStateChanged(state);
}

void MediaBoxPlayerClient::connectToPlayer(const PlayerConnectionSettings &settings)
{
    const quint64 generation = m_generation + 1;
    m_wantsConnection = false;
    closeTransport(ConnectionState::Disconnected, {});
    if (m_generation != generation)
        return;

    PlayerConnectionSettings normalized = settings;
    normalized.host = normalized.host.trimmed();
    normalized.token = normalized.token.trimmed();
    if (normalized.host != m_settings.host || normalized.port != m_settings.port
        || normalized.token != m_settings.token) {
        m_hasStatus = false;
        resetStatus();
    }
    m_settings = normalized;
    if (m_settings.host.isEmpty() || m_settings.port == 0) {
        emit connectionError(tr("Укажите адрес плеера и TCP-порт от 1 до 65535."));
        return;
    }
    m_wantsConnection = true;
    m_reconnectDelayMs = m_timing.reconnectBaseMs;
    beginConnection();
}

void MediaBoxPlayerClient::disconnectFromPlayer()
{
    m_wantsConnection = false;
    closeTransport(ConnectionState::Disconnected, {});
}

void MediaBoxPlayerClient::beginConnection()
{
    if (!m_wantsConnection || m_socket)
        return;
    const quint64 generation = m_generation;
    auto *socket = new QTcpSocket(this);
    m_socket = socket;
    auto connected = std::make_shared<bool>(false);
    socket->setReadBufferSize(MaxResponseBytes + 1);
    const auto current = [this, socket, generation] {
        return m_socket == socket && m_generation == generation;
    };

    connect(socket, &QTcpSocket::connected, this, [this, current, connected] {
        if (!current())
            return;
        *connected = true;
        m_connectTimer.stop();
        setState(ConnectionState::Synchronizing);
        if (current()) {
            const QString id = submit(QStringLiteral("status"), {}, true);
            if (id.isEmpty() && current())
                failConnection(tr("Не удалось подготовить запрос состояния. Проверьте настройки подключения."),
                               ConnectionState::Disconnected);
        }
    });
    connect(socket, &QTcpSocket::readyRead, this, [this, current] {
        if (current())
            readAvailable();
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, current] {
        if (current())
            failConnection(tr("Соединение с плеером закрыто."));
    });
    connect(socket, &QTcpSocket::errorOccurred, this,
            [this, socket, current, connected](QAbstractSocket::SocketError) {
        if (!current())
            return;
        // A peer may close immediately after its complete reply.
        if (socket->bytesAvailable() > 0)
            readAvailable();
        if (current()) {
            const QString message = tr("Ошибка соединения с плеером: %1").arg(socket->errorString());
            if (*connected)
                failConnection(message);
            else
                failConnectionAttempt(message);
        }
    });

    if (m_state != ConnectionState::Reconnecting)
        setState(ConnectionState::Connecting);
    if (!current())
        return;
    m_connectTimer.start(m_timing.connectTimeoutMs);
    socket->connectToHost(m_settings.host, m_settings.port);
}

void MediaBoxPlayerClient::closeTransport(ConnectionState state, const QString &reason)
{
    ++m_generation;
    m_connectTimer.stop();
    m_responseTimer.stop();
    m_pollTimer.stop();
    m_reconnectTimer.stop();
    if (m_socket) {
        QTcpSocket *socket = std::exchange(m_socket, nullptr);
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    m_input.clear();
    m_reading = false;
    auto current = std::exchange(m_current, std::nullopt);
    const auto queued = std::exchange(m_queue, {});
    setState(state);

    // Pending actions were never sent. Notify their cancellation first so an
    // uncertain outcome of the sent mutation remains the primary UI message.
    for (const Request &request : queued)
        emit commandCancelled(request.id, request.command);
    if (current) {
        if (current->command != QStringLiteral("status"))
            emit commandOutcomeUnknown(current->id, current->command);
        else if (!reason.isEmpty())
            emit commandFailed(current->id, current->command,
                               QStringLiteral("connection_lost"), reason);
        else
            emit commandCancelled(current->id, current->command);
    }
}

void MediaBoxPlayerClient::failConnection(const QString &message, ConnectionState state)
{
    const bool retry = m_wantsConnection && state == ConnectionState::Reconnecting;
    if (!retry)
        m_wantsConnection = false;
    const ConnectionState target = retry ? ConnectionState::Reconnecting : state;
    const quint64 previousGeneration = m_generation;
    // Stop accepting commands before notifying observers. Report the general
    // connection problem before the more specific uncertain command outcome.
    setState(target);
    if (m_generation != previousGeneration)
        return;
    emit connectionError(message);
    if (m_generation != previousGeneration)
        return;
    const quint64 generation = previousGeneration + 1;
    closeTransport(target, message);
    if (m_generation != generation || !retry || !m_wantsConnection)
        return;

    m_reconnectTimer.start(m_reconnectDelayMs);
    m_reconnectDelayMs = static_cast<int>(qMin<qint64>(
        qint64(m_reconnectDelayMs) * 2, m_timing.reconnectMaximumMs));
}

void MediaBoxPlayerClient::failConnectionAttempt(const QString &message)
{
    const quint64 generation = m_generation;
    const PlayerConnectionSettings settings = m_settings;
    failConnection(message);
    // A direct UI receiver may have disconnected or selected another player.
    if (m_generation == generation + 1 && m_wantsConnection
        && m_state == ConnectionState::Reconnecting)
        emit connectionAttemptFailed(settings);
}

QString MediaBoxPlayerClient::reject(const QString &command, const QString &code,
                                   const QString &message)
{
    emit commandFailed({}, command, code, message);
    return {};
}

QString MediaBoxPlayerClient::submit(const QString &command, const QJsonObject &arguments,
                                   bool synchronizing)
{
    if (!isReady() && !(synchronizing && m_state == ConnectionState::Synchronizing))
        return reject(command, QStringLiteral("not_ready"),
                      tr("Дождитесь подключения и получения состояния плеера."));
    if (command == QStringLiteral("status")) {
        if (m_current && m_current->command == command)
            return m_current->id;
        for (const Request &request : std::as_const(m_queue)) {
            if (request.command == command)
                return request.id;
        }
    }
    if (m_queue.size() >= MaxQueuedCommands)
        return reject(command, QStringLiteral("queue_full"),
                      tr("Слишком много команд ожидает отправки плееру."));

    Request request;
    request.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    request.command = command;
    QJsonObject object = arguments;
    object.insert(QStringLiteral("protocolVersion"), 1);
    object.insert(QStringLiteral("id"), request.id);
    object.insert(QStringLiteral("token"), m_settings.token);
    object.insert(QStringLiteral("command"), command);
    request.frame = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    if (request.frame.size() > MaxRequestBytes)
        return reject(command, QStringLiteral("request_too_large"),
                      tr("Размер команды превышает 1 МиБ. Разделите список файлов."));

    const QString id = request.id;
    m_queue.enqueue(std::move(request));
    sendNext();
    return id;
}

void MediaBoxPlayerClient::sendNext()
{
    if (m_reading || m_current || m_queue.isEmpty() || !m_socket
        || m_socket->state() != QAbstractSocket::ConnectedState
        || (!isReady() && m_state != ConnectionState::Synchronizing))
        return;

    m_current = m_queue.dequeue();
    const quint64 generation = m_generation;
    const Request request = *m_current;
    m_responseTimer.start(m_timing.responseTimeoutMs);
    const qint64 written = m_socket->write(request.frame);
    if (generation != m_generation)
        return;
    if (written < 0) {
        m_current.reset();
        m_responseTimer.stop();
        emit commandFailed(request.id, request.command, QStringLiteral("write_failed"),
                           tr("Не удалось отправить команду плееру."));
        if (generation == m_generation)
            failConnection(tr("Не удалось записать команду в TCP-соединение."));
    } else if (written != request.frame.size()) {
        failConnection(tr("Команда отправлена не полностью; её результат неизвестен."));
    }
}

void MediaBoxPlayerClient::readAvailable()
{
    if (!m_socket || m_reading)
        return;
    const quint64 generation = m_generation;
    m_reading = true;
    while (m_socket && m_socket->bytesAvailable() > 0) {
        m_input.append(m_socket->read(ReadChunkBytes));
        qsizetype newline;
        while ((newline = m_input.indexOf('\n')) >= 0) {
            if (newline + 1 > MaxResponseBytes) {
                failConnection(tr("Ответ плеера превышает допустимые 32 МиБ."));
                return;
            }
            const QByteArray line = m_input.left(newline);
            m_input.remove(0, newline + 1);
            if (!processReply(line) || m_generation != generation)
                return;
        }
        if (m_input.size() >= MaxResponseBytes) {
            failConnection(tr("Незавершённый ответ плеера превышает допустимые 32 МиБ."));
            return;
        }
    }
    if (m_generation == generation) {
        m_reading = false;
        sendNext();
    }
}

bool MediaBoxPlayerClient::processReply(const QByteArray &line)
{
    const auto invalid = [this](const QString &detail) {
        failConnection(tr("Некорректный ответ плеера: %1").arg(detail));
        return false;
    };
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return invalid(tr("ожидался JSON-объект."));
    const QJsonObject object = document.object();
    const auto version = object.value(QStringLiteral("protocolVersion"));
    if (!version.isDouble())
        return invalid(tr("отсутствует числовая версия протокола."));
    if (version.toDouble() != 1) {
        failConnection(tr("Версия протокола плеера не поддерживается. Требуется версия 1."),
                       ConnectionState::ProtocolMismatch);
        return false;
    }
    const auto id = object.value(QStringLiteral("id"));
    if (!m_current || !id.isString() || id.toString() != m_current->id)
        return invalid(tr("идентификатор ответа не соответствует ожидаемой команде."));
    const auto ok = object.value(QStringLiteral("ok"));
    if (!ok.isBool())
        return invalid(tr("отсутствует логическое поле ok."));

    QString errorCode;
    QString errorMessage;
    if (!ok.toBool()) {
        const auto error = object.value(QStringLiteral("error"));
        if (!error.isObject())
            return invalid(tr("отсутствует описание ошибки команды."));
        const auto code = error.toObject().value(QStringLiteral("code"));
        const auto message = error.toObject().value(QStringLiteral("message"));
        if (!code.isString() || code.toString().isEmpty() || !message.isString())
            return invalid(tr("неверная структура ошибки команды."));
        errorCode = code.toString();
        errorMessage = message.toString();
    }

    const bool hasStatus = object.contains(QStringLiteral("status"));
    QVariant status;
    if (hasStatus) {
        const auto value = object.value(QStringLiteral("status"));
        if (!value.isObject() || !decodeStatus(value.toObject(), &status))
            return invalid(tr("неверная структура состояния плеера."));
    } else if (ok.toBool()) {
        return invalid(tr("успешный ответ не содержит состояния плеера."));
    }

    const quint64 generation = m_generation;
    const Request request = *m_current;
    const bool synchronizing = m_state == ConnectionState::Synchronizing;
    const auto reportResult = [this, &request, &ok, &errorCode, &errorMessage] {
        if (ok.toBool())
            emit commandSucceeded(request.id, request.command);
        else
            emit commandFailed(request.id, request.command, errorCode, errorMessage);
    };
    m_current.reset();
    m_responseTimer.stop();
    if (hasStatus) {
        applyStatus(status);
        m_hasStatus = true;
    }
    if (ok.toBool() && synchronizing) {
        m_reconnectDelayMs = m_timing.reconnectBaseMs;
        m_pollTimer.start(m_timing.pollIntervalMs);
        setState(ConnectionState::Ready);
        if (m_generation != generation) {
            reportResult();
            return false;
        }
    }
    if (hasStatus) {
        // Pass a stable local snapshot: a direct receiver can change settings,
        // which resets the stored status before other receivers run.
        publishStatus(status);
        if (m_generation != generation) {
            reportResult();
            return false;
        }
    }
    reportResult();
    if (!ok.toBool()) {
        if (m_generation != generation)
            return false;
        if (errorCode == QStringLiteral("unauthorized")) {
            failConnection(tr("Плеер отклонил токен доступа. Проверьте настройки подключения."),
                           ConnectionState::AuthenticationFailed);
            return false;
        }
        if (errorCode == QStringLiteral("unsupported_protocol")) {
            failConnection(tr("Плеер не поддерживает версию протокола 1."),
                           ConnectionState::ProtocolMismatch);
            return false;
        }
        if (synchronizing) {
            failConnection(tr("Не удалось получить начальное состояние плеера: %1")
                               .arg(errorMessage));
            return false;
        }
    }
    return m_generation == generation;
}

QString MediaBoxPlayerClient::requestStatus()
{
    return submit(QStringLiteral("status"));
}

QString MediaBoxPlayerClient::load(const QStringList &paths, int startIndex, bool autoplay)
{
    if (!validPaths(paths) || startIndex < 0 || startIndex >= paths.size())
        return reject(QStringLiteral("load"), QStringLiteral("invalid_arguments"),
                      tr("Укажите от 1 до 1000 абсолютных путей на машине плеера "
                         "и существующий начальный индекс. Максимальная длина пути — 4096 символов."));
    return submit(QStringLiteral("load"),
                  {{QStringLiteral("paths"), QJsonArray::fromStringList(paths)},
                   {QStringLiteral("startIndex"), startIndex},
                   {QStringLiteral("autoplay"), autoplay}});
}

QString MediaBoxPlayerClient::enqueue(const QStringList &paths)
{
    if (!validPaths(paths))
        return reject(QStringLiteral("enqueue"), QStringLiteral("invalid_arguments"),
                      tr("Укажите от 1 до 1000 абсолютных путей на машине плеера. "
                         "Максимальная длина пути — 4096 символов."));
    return submit(QStringLiteral("enqueue"),
                  {{QStringLiteral("paths"), QJsonArray::fromStringList(paths)}});
}

QString MediaBoxPlayerClient::play() { return submit(QStringLiteral("play")); }
QString MediaBoxPlayerClient::pause() { return submit(QStringLiteral("pause")); }
QString MediaBoxPlayerClient::stop() { return submit(QStringLiteral("stop")); }
QString MediaBoxPlayerClient::next() { return submit(QStringLiteral("next")); }
QString MediaBoxPlayerClient::previous() { return submit(QStringLiteral("previous")); }
QString MediaBoxPlayerClient::clear() { return submit(QStringLiteral("clear")); }

QString MediaBoxPlayerClient::setSchedule(const QJsonObject &schedule)
{
    if (!schedule.value(QStringLiteral("channels")).isArray()
        || !schedule.value(QStringLiteral("adverts")).isArray())
        return reject(QStringLiteral("setSchedule"), QStringLiteral("invalid_arguments"),
                      tr("Расписание должно содержать массивы каналов и рекламы."));
    return submit(QStringLiteral("setSchedule"), {{QStringLiteral("schedule"), schedule}});
}

QString MediaBoxPlayerClient::startSchedule(const QJsonObject &schedule)
{
    if (schedule.isEmpty()) return submit(QStringLiteral("schedule"));
    if (!schedule.value(QStringLiteral("channels")).isArray()
        || !schedule.value(QStringLiteral("adverts")).isArray())
        return reject(QStringLiteral("schedule"), QStringLiteral("invalid_arguments"),
                      tr("Расписание должно содержать массивы каналов и рекламы."));
    return submit(QStringLiteral("schedule"), {{QStringLiteral("schedule"), schedule}});
}

QString MediaBoxPlayerClient::playChannel(const QString &name, const QStringList &paths, int volume,
                                        const QString &order)
{
    if (order != QStringLiteral("sequential") && order != QStringLiteral("shuffle_cycle"))
        return reject(QStringLiteral("playChannel"), QStringLiteral("invalid_arguments"),
                      tr("Выберите порядок треков: по порядку или случайно."));
    if (name.trimmed().isEmpty() || name.size() > 256 || name.contains(QChar::Null)
        || !validPaths(paths) || volume < 0 || volume > 100)
        return reject(QStringLiteral("playChannel"), QStringLiteral("invalid_arguments"),
                      tr("Выберите непустой канал с абсолютными путями и громкостью от 0 до 100."));
    return submit(QStringLiteral("playChannel"), {{QStringLiteral("name"), name},
                  {QStringLiteral("paths"), QJsonArray::fromStringList(paths)},
                  {QStringLiteral("volume"), volume}, {QStringLiteral("order"), order}});
}

QString MediaBoxPlayerClient::seek(qint64 positionMs)
{
    if (positionMs < 0 || positionMs > MaxSeekPosition)
        return reject(QStringLiteral("seek"), QStringLiteral("invalid_arguments"),
                      tr("Позиция должна быть целым числом от 0 до 9007199254740991 мс."));
    return submit(QStringLiteral("seek"), {{QStringLiteral("positionMs"), positionMs}});
}

QString MediaBoxPlayerClient::setVolume(int value)
{
    if (value < 0 || value > 100)
        return reject(QStringLiteral("volume"), QStringLiteral("invalid_arguments"),
                      tr("Громкость должна быть от 0 до 100."));
    return submit(QStringLiteral("volume"), {{QStringLiteral("value"), value}});
}

QString MediaBoxPlayerClient::setMuted(bool muted)
{
    return submit(QStringLiteral("mute"), {{QStringLiteral("value"), muted}});
}

QString MediaBoxPlayerClient::setRepeat(const QString &mode)
{
    if (!validRepeat(mode))
        return reject(QStringLiteral("repeat"), QStringLiteral("invalid_arguments"),
                      tr("Режим повтора должен быть off, all или one."));
    return submit(QStringLiteral("repeat"), {{QStringLiteral("mode"), mode}});
}
