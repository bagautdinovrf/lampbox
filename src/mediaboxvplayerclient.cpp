#include "mediaboxvplayerclient.h"

#include <QJsonArray>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>
#include <limits>

namespace {

constexpr int MaxWindows = 16;
constexpr qint64 MaxSeekPosition = 9007199254740991LL;

bool validWindowId(const QString &id)
{
    static const QRegularExpression expression(QStringLiteral("\\A[A-Za-z0-9_-]{1,64}\\z"));
    return expression.match(id).hasMatch();
}

bool validName(const QString &name)
{
    return !name.trimmed().isEmpty() && name.size() <= 128 && !name.contains(QChar::Null);
}

bool validScreen(const QString &screen)
{
    return screen.size() <= 256 && !screen.contains(QChar::Null);
}

bool validRepeat(const QString &mode)
{
    return mode == QStringLiteral("off") || mode == QStringLiteral("all")
        || mode == QStringLiteral("one");
}

} // namespace

MediaBoxVPlayerClient::MediaBoxVPlayerClient(QObject *parent)
    : MediaBoxPlayerClient(parent)
{
    qRegisterMetaType<VideoDisplayStatus>();
    qRegisterMetaType<VideoWindowStatus>();
    qRegisterMetaType<VideoPlayerStatus>();
}

bool MediaBoxVPlayerClient::decodeStatus(const QJsonObject &object, QVariant *snapshot) const
{
    if (object.value(QStringLiteral("application")).toString() != QStringLiteral("MediaBoxVPlayer")
        || !object.value(QStringLiteral("displays")).isArray()
        || !object.value(QStringLiteral("windows")).isArray()
        || (object.contains(QStringLiteral("persistenceError"))
            && !object.value(QStringLiteral("persistenceError")).isString()))
        return false;

    VideoPlayerStatus status;
    status.persistenceError = object.value(QStringLiteral("persistenceError")).toString();
    QSet<QString> displayIds;
    QSet<int> displayIndexes;
    const auto displays = object.value(QStringLiteral("displays")).toArray();
    for (const QJsonValue &value : displays) {
        if (!value.isObject())
            return false;
        const QJsonObject display = value.toObject();
        const auto id = display.value(QStringLiteral("id"));
        const auto name = display.value(QStringLiteral("name"));
        const auto index = display.value(QStringLiteral("index"));
        if (!id.isString() || id.toString().isEmpty() || !validScreen(id.toString())
            || !name.isString() || !index.isDouble())
            return false;
        const qint64 displayIndex = index.toInteger(-1);
        if (displayIndex < 0 || displayIndex > std::numeric_limits<int>::max()
            || displayIds.contains(id.toString()) || displayIndexes.contains(int(displayIndex)))
            return false;
        displayIds.insert(id.toString());
        displayIndexes.insert(int(displayIndex));
        status.displays.append({id.toString(), name.toString(), int(displayIndex)});
    }

    const auto windows = object.value(QStringLiteral("windows")).toArray();
    if (windows.size() > MaxWindows)
        return false;
    QSet<QString> windowIds;
    for (const QJsonValue &value : windows) {
        if (!value.isObject())
            return false;
        const QJsonObject window = value.toObject();
        const auto id = window.value(QStringLiteral("id"));
        const auto name = window.value(QStringLiteral("name"));
        const auto screen = window.value(QStringLiteral("screen"));
        const auto actualScreen = window.value(QStringLiteral("actualScreen"));
        const auto fullscreen = window.value(QStringLiteral("fullscreen"));
        const auto playback = window.value(QStringLiteral("playback"));
        if (!id.isString() || !validWindowId(id.toString()) || windowIds.contains(id.toString())
            || !name.isString() || !validName(name.toString())
            || !screen.isString() || !validScreen(screen.toString())
            || !actualScreen.isString() || !validScreen(actualScreen.toString())
            || (!actualScreen.toString().isEmpty() && !displayIds.contains(actualScreen.toString()))
            || !fullscreen.isBool() || !playback.isObject()
            || (window.contains(QStringLiteral("restoreError"))
                && !window.value(QStringLiteral("restoreError")).isString()))
            return false;
        VideoWindowStatus entry;
        if (!parsePlaybackStatus(playback.toObject(), &entry.playback))
            return false;
        entry.id = id.toString();
        entry.name = name.toString();
        entry.screen = screen.toString();
        entry.actualScreen = actualScreen.toString();
        entry.fullscreen = fullscreen.toBool();
        entry.restoreError = window.value(QStringLiteral("restoreError")).toString();
        status.windows.append(entry);
        windowIds.insert(entry.id);
    }

    *snapshot = QVariant::fromValue(status);
    return true;
}

void MediaBoxVPlayerClient::applyStatus(const QVariant &snapshot)
{
    m_videoStatus = snapshot.value<VideoPlayerStatus>();
}

void MediaBoxVPlayerClient::publishStatus(const QVariant &snapshot)
{
    const auto status = snapshot.value<VideoPlayerStatus>();
    emit videoStatusChanged(status);
}

void MediaBoxVPlayerClient::resetStatus()
{
    m_videoStatus = {};
}

QString MediaBoxVPlayerClient::submitWindow(const QString &command, const QString &windowId,
                                          QJsonObject arguments)
{
    if (!validWindowId(windowId))
        return reject(command, QStringLiteral("invalid_arguments"),
                      tr("Идентификатор видеоокна должен содержать от 1 до 64 латинских букв, "
                         "цифр, дефисов или знаков подчёркивания."));
    arguments.insert(QStringLiteral("windowId"), windowId);
    return submit(command, arguments);
}

QString MediaBoxVPlayerClient::configureWindow(const QString &windowId, const QString &name,
                                             const QString &screen, bool fullscreen)
{
    if (!validName(name) || !validScreen(screen))
        return reject(QStringLiteral("configureWindow"), QStringLiteral("invalid_arguments"),
                      tr("Укажите название видеоокна от 1 до 128 символов и допустимый экран."));
    return submitWindow(QStringLiteral("configureWindow"), windowId,
                        {{QStringLiteral("name"), name}, {QStringLiteral("screen"), screen},
                         {QStringLiteral("fullscreen"), fullscreen}});
}

QString MediaBoxVPlayerClient::removeWindow(const QString &windowId)
{
    return submitWindow(QStringLiteral("removeWindow"), windowId);
}

QString MediaBoxVPlayerClient::setFullscreen(const QString &windowId, bool fullscreen)
{
    return submitWindow(QStringLiteral("fullscreen"), windowId,
                        {{QStringLiteral("value"), fullscreen}});
}

QString MediaBoxVPlayerClient::load(const QString &windowId, const QStringList &paths,
                                  int startIndex, bool autoplay)
{
    if (!validMediaPaths(paths) || startIndex < 0 || startIndex >= paths.size())
        return reject(QStringLiteral("load"), QStringLiteral("invalid_arguments"),
                      tr("Укажите от 1 до 1000 абсолютных путей на машине видеоплеера "
                         "и существующий начальный индекс. Максимальная длина пути — 4096 символов."));
    return submitWindow(QStringLiteral("load"), windowId,
                        {{QStringLiteral("paths"), QJsonArray::fromStringList(paths)},
                         {QStringLiteral("startIndex"), startIndex},
                         {QStringLiteral("autoplay"), autoplay}});
}

QString MediaBoxVPlayerClient::enqueue(const QString &windowId, const QStringList &paths)
{
    if (!validMediaPaths(paths))
        return reject(QStringLiteral("enqueue"), QStringLiteral("invalid_arguments"),
                      tr("Укажите от 1 до 1000 абсолютных путей на машине видеоплеера. "
                         "Максимальная длина пути — 4096 символов."));
    return submitWindow(QStringLiteral("enqueue"), windowId,
                        {{QStringLiteral("paths"), QJsonArray::fromStringList(paths)}});
}

QString MediaBoxVPlayerClient::loadPublication(const QString &windowId, const QString &activePath,
                                             const QString &contentRoot, bool autoplay)
{
    if (!validWindowId(windowId) || !validMediaPaths({activePath, contentRoot}))
        return reject(QStringLiteral("loadPublication"), QStringLiteral("invalid_arguments"),
                      tr("Выберите видеоокно и укажите абсолютные пути к active.json и медиатеке, доступные видеоплееру."));
    if (isReady()) {
        const auto window = std::find_if(m_videoStatus.windows.cbegin(), m_videoStatus.windows.cend(),
                                        [&windowId](const auto &entry) { return entry.id == windowId; });
        if (window == m_videoStatus.windows.cend())
            return reject(QStringLiteral("loadPublication"), QStringLiteral("unknown_window"),
                          tr("Примените настройки видеоокна перед запуском расписания."));
        if (!window->playback.supportedCapabilities.contains(QStringLiteral("schedule.current.v1")))
            return reject(QStringLiteral("loadPublication"), QStringLiteral("unsupported_capability"),
                          tr("Обновите MediaBoxVPlayer: подключённый плеер не поддерживает загрузку текущего формата расписания из файлов."));
    }
    return submitWindow(QStringLiteral("loadPublication"), windowId,
                        {{"activePath", activePath}, {"contentRoot", contentRoot}, {"autoplay", autoplay}});
}

QString MediaBoxVPlayerClient::startSchedule(const QString &windowId)
{
    return submitWindow(QStringLiteral("schedule"), windowId);
}

QString MediaBoxVPlayerClient::playChannel(const QString &windowId, const QString &name,
                                         const QStringList &paths, int volume, const QString &order)
{
    if (order != QStringLiteral("sequential") && order != QStringLiteral("shuffle_cycle"))
        return reject(QStringLiteral("playChannel"), QStringLiteral("invalid_arguments"),
                      tr("Выберите порядок треков: по порядку или случайно."));
    if (name.trimmed().isEmpty() || name.size() > 256 || name.contains(QChar::Null)
        || !validMediaPaths(paths) || volume < 0 || volume > 100)
        return reject(QStringLiteral("playChannel"), QStringLiteral("invalid_arguments"),
                      tr("Выберите канал с названием до 256 символов, от 1 до 1000 файлов "
                         "с абсолютными путями на машине видеоплеера и громкостью от 0 до 100."));
    return submitWindow(QStringLiteral("playChannel"), windowId,
                        {{QStringLiteral("name"), name},
                         {QStringLiteral("paths"), QJsonArray::fromStringList(paths)},
                         {QStringLiteral("volume"), volume}, {QStringLiteral("order"), order}});
}

QString MediaBoxVPlayerClient::play(const QString &id) { return submitWindow(QStringLiteral("play"), id); }
QString MediaBoxVPlayerClient::pause(const QString &id) { return submitWindow(QStringLiteral("pause"), id); }
QString MediaBoxVPlayerClient::stop(const QString &id) { return submitWindow(QStringLiteral("stop"), id); }
QString MediaBoxVPlayerClient::next(const QString &id) { return submitWindow(QStringLiteral("next"), id); }
QString MediaBoxVPlayerClient::previous(const QString &id) { return submitWindow(QStringLiteral("previous"), id); }
QString MediaBoxVPlayerClient::clear(const QString &id) { return submitWindow(QStringLiteral("clear"), id); }

QString MediaBoxVPlayerClient::seek(const QString &windowId, qint64 positionMs)
{
    if (positionMs < 0 || positionMs > MaxSeekPosition)
        return reject(QStringLiteral("seek"), QStringLiteral("invalid_arguments"),
                      tr("Позиция должна быть целым числом от 0 до 9007199254740991 мс."));
    return submitWindow(QStringLiteral("seek"), windowId, {{QStringLiteral("positionMs"), positionMs}});
}

QString MediaBoxVPlayerClient::setVolume(const QString &windowId, int value)
{
    if (value < 0 || value > 100)
        return reject(QStringLiteral("volume"), QStringLiteral("invalid_arguments"),
                      tr("Громкость должна быть от 0 до 100."));
    return submitWindow(QStringLiteral("volume"), windowId, {{QStringLiteral("value"), value}});
}

QString MediaBoxVPlayerClient::setMuted(const QString &windowId, bool muted)
{
    return submitWindow(QStringLiteral("mute"), windowId, {{QStringLiteral("value"), muted}});
}

QString MediaBoxVPlayerClient::setRepeat(const QString &windowId, const QString &mode)
{
    if (!validRepeat(mode))
        return reject(QStringLiteral("repeat"), QStringLiteral("invalid_arguments"),
                      tr("Режим повтора должен быть off, all или one."));
    return submitWindow(QStringLiteral("repeat"), windowId, {{QStringLiteral("mode"), mode}});
}
