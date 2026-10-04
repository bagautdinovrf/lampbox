#include "videoservice.h"

#include "playerengine.h"
#include "qtvideobackend.h"
#include "videowindow.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScreen>
#include <QScopedValueRollback>
#include <QTimer>
#include <QWindow>
#include <cmath>

namespace MediaBox {
namespace {
constexpr int MaxWindows = 16;
constexpr qint64 MaxStateBytes = 64 * 1024 * 1024;
constexpr qint64 MaxQueueBytes = 8 * 1024 * 1024;

bool validId(const QJsonValue &value)
{
    static const QRegularExpression expression(QStringLiteral("\\A[A-Za-z0-9_-]{1,64}\\z"));
    return value.isString() && expression.match(value.toString()).hasMatch();
}

bool validDefinition(const QJsonObject &object)
{
    const auto name = object.value(QStringLiteral("name"));
    const auto screen = object.value(QStringLiteral("screen"));
    return validId(object.value(QStringLiteral("windowId")))
        && name.isString() && !name.toString().trimmed().isEmpty() && name.toString().size() <= 128
        && !name.toString().contains(QChar::Null)
        && screen.isString() && screen.toString().size() <= 256 && !screen.toString().contains(QChar::Null)
        && object.value(QStringLiteral("fullscreen")).isBool();
}

bool integer(const QJsonValue &value, int minimum, int maximum)
{
    const double number = value.toDouble(-1);
    return value.isDouble() && std::isfinite(number) && number == std::floor(number)
        && number >= minimum && number <= maximum;
}

qint64 pathsBytes(const QJsonArray &paths)
{
    return QJsonDocument(paths).toJson(QJsonDocument::Compact).size();
}

bool validPath(const QJsonValue &value)
{
    return value.isString() && !value.toString().isEmpty() && value.toString().size() <= 4096
        && !value.toString().contains(QChar::Null) && QDir::isAbsolutePath(value.toString());
}

bool validPublication(const QJsonValue &value)
{
    const auto object = value.toObject();
    return value.isObject() && object.size() == 2 && validPath(object.value("activePath"))
        && validPath(object.value("contentRoot"));
}

bool writeAtomic(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = QStringLiteral("Cannot save %1: %2").arg(path, file.errorString());
        return false;
    }
    return true;
}

} // namespace

struct VideoService::Record
{
    QString id;
    QString name;
    QString screen;
    QString restoreError;
    QJsonObject publication;
    QString runtimePath;
    // Destruction order matters: engine, decoder, then its video surface.
    std::unique_ptr<VideoWindow> window;
    std::unique_ptr<AudioBackend> backend;
    std::unique_ptr<PlayerEngine> engine;
};

VideoService::VideoService(const QString &dataDirectory, BackendFactory factory, QObject *parent)
    : QObject(parent), m_dataDirectory(dataDirectory), m_factory(std::move(factory))
{
    if (!m_factory)
        m_factory = [](QVideoWidget *video, QObject *owner) { return new QtVideoBackend(video, owner); };
    updateScreens();
    connect(qGuiApp, &QGuiApplication::screenAdded, this, [this] { reconcileScreens(); });
    connect(qGuiApp, &QGuiApplication::screenRemoved, this, [this] { reconcileScreens(); });
    connect(qGuiApp, &QGuiApplication::primaryScreenChanged, this, [this] { reconcileScreens(); });
}

VideoService::~VideoService() = default;

void VideoService::updateScreens()
{
    const auto screens = QGuiApplication::screens();
    for (auto it = m_screenIds.begin(); it != m_screenIds.end();) {
        if (!screens.contains(it.key()))
            it = m_screenIds.erase(it);
        else
            ++it;
    }
    QSet<QString> used;
    for (const auto &id : std::as_const(m_screenIds))
        used.insert(id);
    for (auto *screen : screens) {
        if (m_screenIds.contains(screen))
            continue;
        const QString identity = screen->name() + QChar::Null + screen->manufacturer()
            + QChar::Null + screen->model() + QChar::Null + screen->serialNumber();
        const QString base = QStringLiteral("screen-") + QString::fromLatin1(
            QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex().left(24));
        QString id = base;
        for (int suffix = 2; used.contains(id); ++suffix)
            id = base + QLatin1Char('-') + QString::number(suffix);
        m_screenIds.insert(screen, id);
        used.insert(id);
    }
}

QScreen *VideoService::resolveScreen(const QString &id) const
{
    if (id.isEmpty())
        return QGuiApplication::primaryScreen();
    for (auto it = m_screenIds.cbegin(); it != m_screenIds.cend(); ++it) {
        if (it.value() == id)
            return it.key();
    }
    return nullptr;
}

void VideoService::present(Record &record)
{
    if (QScreen *screen = resolveScreen(record.screen)) {
        record.window->presentOn(screen, m_screenIds.value(screen));
        record.engine->setPlaybackAvailable(true);
    } else {
        record.engine->setPlaybackAvailable(false);
        record.window->suspend();
    }
}

void VideoService::reconcileScreens()
{
    updateScreens();
    for (auto &[id, record] : m_windows)
        present(*record);
    emit statusChanged();
}

VideoService::Record &VideoService::createWindow(const QString &id, const QString &name,
                                                 const QString &screen, bool fullscreen)
{
    auto record = std::make_unique<Record>();
    record->id = id;
    record->name = name.trimmed();
    record->screen = screen;
    record->window = std::make_unique<VideoWindow>();
    record->window->setWindowTitle(record->name + QStringLiteral(" — MediaBoxVPlayer"));
    record->backend.reset(m_factory(record->window->videoWidget(), nullptr));
    // A digest avoids Windows reserved filenames and case-folding collisions.
    const QString runtimeName = QString::fromLatin1(QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha256).toHex());
    record->runtimePath = QDir(m_dataDirectory).filePath("runtimes/" + runtimeName + ".sqlite");
    record->engine = std::make_unique<PlayerEngine>(record->backend.get(), nullptr,
        [] { return QDateTime::currentDateTime(); },
        record->runtimePath, QStringLiteral("video"));
    record->window->setFullscreen(fullscreen);
    Record *entry = record.get();
    m_windows.emplace(id, std::move(record));
    connect(entry->engine.get(), &PlayerEngine::statusChanged, this, &VideoService::statusChanged);
    connect(entry->window.get(), &VideoWindow::fullscreenChanged, this, [this] {
        if (!m_restoring && !m_mutating) {
            QString error;
            if (!save(&error))
                m_persistenceError = error;
            else
                m_persistenceError.clear();
            emit statusChanged();
        }
    });
    connect(entry->window.get(), &VideoWindow::closeRequested, this, [this, entry] {
        entry->engine->execute({{QStringLiteral("command"), QStringLiteral("stop")}});
        entry->window->suspend();
        QString error;
        if (!save(&error))
            m_persistenceError = error;
        emit statusChanged();
    });
    present(*entry);
    return *entry;
}

VideoWindow *VideoService::window(const QString &id) const
{
    const auto it = m_windows.find(id);
    return it == m_windows.end() ? nullptr : it->second->window.get();
}

QJsonObject VideoService::status() const
{
    QJsonArray displays;
    const auto screens = QGuiApplication::screens();
    for (qsizetype i = 0; i < screens.size(); ++i) {
        QScreen *screen = screens.at(i);
        displays.append(QJsonObject{{QStringLiteral("id"), m_screenIds.value(screen)},
                                    {QStringLiteral("name"), screen->name().isEmpty()
                                         ? tr("Экран %1").arg(i + 1) : screen->name()},
                                    {QStringLiteral("index"), i}});
    }
    QJsonArray windows;
    for (const auto &[id, record] : m_windows) {
        const QString actual = record->window->isVisible() && record->window->windowHandle()
            ? m_screenIds.value(record->window->windowHandle()->screen()) : QString();
        auto playback = record->engine->status();
        windows.append(QJsonObject{{QStringLiteral("id"), id},
                                   {QStringLiteral("name"), record->name},
                                   {QStringLiteral("screen"), record->screen},
                                   {QStringLiteral("actualScreen"), actual},
                                   {QStringLiteral("fullscreen"), record->window->requestedFullscreen()},
                                   {QStringLiteral("restoreError"), record->restoreError},
                                   {QStringLiteral("playback"), playback}});
    }
    return {{QStringLiteral("application"), QStringLiteral("MediaBoxVPlayer")},
            {QStringLiteral("displays"), displays}, {QStringLiteral("windows"), windows},
            {QStringLiteral("persistenceError"), m_persistenceError}};
}

QJsonObject VideoService::success() const
{
    return {{QStringLiteral("ok"), true}, {QStringLiteral("status"), status()}};
}

QJsonObject VideoService::failure(const QString &code, const QString &message) const
{
    return {{QStringLiteral("ok"), false},
            {QStringLiteral("error"), QJsonObject{{QStringLiteral("code"), code}, {QStringLiteral("message"), message}}},
            {QStringLiteral("status"), status()}};
}

QJsonObject VideoService::finishMutation()
{
    QString error;
    if (!save(&error)) {
        m_persistenceError = error;
        emit statusChanged();
        return failure(QStringLiteral("persistence_error"), error);
    }
    m_persistenceError.clear();
    emit statusChanged();
    return success();
}

QJsonObject VideoService::execute(const QJsonObject &request)
{
    QScopedValueRollback<bool> guard(m_mutating, true);
    if (!request.value(QStringLiteral("command")).isString())
        return failure(QStringLiteral("invalid_request"), QStringLiteral("command must be a string."));
    const QString command = request.value(QStringLiteral("command")).toString();
    const auto invalid = [this](const QString &message) { return failure(QStringLiteral("invalid_arguments"), message); };
    const QSet<QString> playbackCommands{QStringLiteral("load"), QStringLiteral("enqueue"), QStringLiteral("play"),
        QStringLiteral("pause"), QStringLiteral("stop"), QStringLiteral("next"), QStringLiteral("previous"),
        QStringLiteral("seek"), QStringLiteral("volume"), QStringLiteral("mute"), QStringLiteral("repeat"), QStringLiteral("clear"),
        QStringLiteral("loadPublication"), QStringLiteral("schedule"), QStringLiteral("playChannel")};
    QSet<QString> fields{QStringLiteral("command"), QStringLiteral("id")};
    if (command == QStringLiteral("configureWindow"))
        fields.unite({QStringLiteral("windowId"), QStringLiteral("name"), QStringLiteral("screen"), QStringLiteral("fullscreen")});
    else if (command == QStringLiteral("fullscreen"))
        fields.unite({QStringLiteral("windowId"), QStringLiteral("value")});
    else if (command == QStringLiteral("removeWindow"))
        fields.insert(QStringLiteral("windowId"));
    else if (command != QStringLiteral("status") && !playbackCommands.contains(command))
        return failure(QStringLiteral("unknown_command"), QStringLiteral("Unknown command: %1").arg(command));
    if (!playbackCommands.contains(command)) {
        for (auto it = request.begin(); it != request.end(); ++it) {
            if (!fields.contains(it.key()))
                return invalid(QStringLiteral("Unknown field for %1: %2").arg(command, it.key()));
        }
    }
    if (command == QStringLiteral("status"))
        return success();
    if (!validId(request.value(QStringLiteral("windowId"))))
        return invalid(QStringLiteral("windowId must match [A-Za-z0-9_-]{1,64}."));
    const QString id = request.value(QStringLiteral("windowId")).toString();
    auto it = m_windows.find(id);
    if (command == QStringLiteral("configureWindow")) {
        if (!validDefinition(request))
            return invalid(QStringLiteral("configureWindow requires name (1..128), screen (0..256), and boolean fullscreen."));
        const QString screen = request.value(QStringLiteral("screen")).toString();
        if (!resolveScreen(screen))
            return failure(QStringLiteral("unknown_screen"), QStringLiteral("The selected display is unavailable."));
        if (it == m_windows.end() && m_windows.size() >= MaxWindows)
            return failure(QStringLiteral("window_limit"), QStringLiteral("At most 16 video windows are supported."));
        const QString name = request.value(QStringLiteral("name")).toString().trimmed();
        const bool fullscreen = request.value(QStringLiteral("fullscreen")).toBool();
        if (it == m_windows.end()) {
            createWindow(id, name, screen, fullscreen);
        } else {
            Record &record = *it->second;
            record.name = name;
            record.screen = screen;
            record.window->setWindowTitle(name + QStringLiteral(" — MediaBoxVPlayer"));
            record.window->setFullscreen(fullscreen);
            present(record);
        }
        return finishMutation();
    }
    if (it == m_windows.end())
        return failure(QStringLiteral("unknown_window"), QStringLiteral("The video window does not exist."));
    Record &record = *it->second;
    if (command == QStringLiteral("removeWindow")) {
        const QString runtimePath = record.runtimePath;
        m_windows.erase(it);
        const auto reply = finishMutation();
        if (reply.value("ok").toBool()) {
            for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")})
                QFile::remove(runtimePath + suffix);
        }
        return reply;
    }
    if (command == QStringLiteral("fullscreen")) {
        if (!request.value(QStringLiteral("value")).isBool())
            return invalid(QStringLiteral("value must be a boolean."));
        record.window->setFullscreen(request.value(QStringLiteral("value")).toBool());
        return finishMutation();
    }
    if (command == QStringLiteral("loadPublication")
        && (!validPath(request.value("activePath")) || !validPath(request.value("contentRoot"))))
        return invalid(QStringLiteral("activePath and contentRoot must be absolute paths of at most 4096 characters."));
    if ((command == QStringLiteral("load") || command == QStringLiteral("enqueue")
         || command == QStringLiteral("playChannel")) && request.value(QStringLiteral("paths")).isArray()) {
        const auto current = record.engine->status();
        qint64 queueBytes = pathsBytes(request.value(QStringLiteral("paths")).toArray());
        if (command == QStringLiteral("enqueue"))
            queueBytes += pathsBytes(current.value(QStringLiteral("queue")).toArray());
        for (const auto &[otherId, other] : m_windows) {
            if (otherId == id) continue;
            const auto otherPlayback = other->engine->status();
            queueBytes += pathsBytes(otherPlayback.value(QStringLiteral("queue")).toArray());
        }
        if (queueBytes > MaxQueueBytes)
            return failure(QStringLiteral("queue_limit"), QStringLiteral("Combined video queues exceed 8 MiB of JSON paths."));
    }
    // Do not start invisible playback after a selected screen was unplugged.
    if (!resolveScreen(record.screen)
        && (command == QStringLiteral("play") || command == QStringLiteral("playChannel")
            || command == QStringLiteral("schedule") || ((command == QStringLiteral("load") || command == QStringLiteral("loadPublication"))
            && request.value(QStringLiteral("autoplay")).toBool())))
        return failure(QStringLiteral("unknown_screen"), QStringLiteral("The selected display is unavailable."));
    QJsonObject playbackRequest = request;
    playbackRequest.remove(QStringLiteral("windowId"));
    QJsonObject response = record.engine->execute(playbackRequest);
    if (!response.value(QStringLiteral("ok")).toBool()) {
        response.insert(QStringLiteral("status"), status());
        return response;
    }
    if (command == QStringLiteral("loadPublication")) {
        record.publication = {{"activePath", request.value("activePath")}, {"contentRoot", request.value("contentRoot")}};
    }
    if (command == QStringLiteral("play") || command == QStringLiteral("schedule")
        || command == QStringLiteral("playChannel") || ((command == QStringLiteral("load") || command == QStringLiteral("loadPublication"))
        && request.value(QStringLiteral("autoplay")).toBool()))
        present(record);
    if (command == QStringLiteral("load") || command == QStringLiteral("clear")
        || command == QStringLiteral("playChannel") || command == QStringLiteral("schedule") || command == QStringLiteral("loadPublication"))
        record.restoreError.clear();
    if (command == QStringLiteral("seek"))
        return success();
    return finishMutation();
}

bool VideoService::save(QString *error) const
{
    QJsonArray windows;
    for (const auto &[id, record] : m_windows) {
        const QJsonObject playback = record->engine->status();
        const bool scheduled = playback.value(QStringLiteral("playbackMode")).toString() == QStringLiteral("schedule");
        QJsonObject item{{QStringLiteral("windowId"), id}, {QStringLiteral("name"), record->name},
            {QStringLiteral("screen"), record->screen}, {QStringLiteral("fullscreen"), record->window->requestedFullscreen()},
            {QStringLiteral("paths"), scheduled ? QJsonValue(QJsonArray{}) : playback.value(QStringLiteral("queue"))},
            {QStringLiteral("currentIndex"), scheduled ? QJsonValue(-1) : playback.value(QStringLiteral("currentIndex"))},
            {QStringLiteral("volumePercent"), playback.value(QStringLiteral("volumePercent"))},
            {QStringLiteral("muted"), playback.value(QStringLiteral("muted"))},
            {QStringLiteral("repeat"), playback.value(QStringLiteral("repeat"))},
            {QStringLiteral("playbackMode"), playback.value(QStringLiteral("playbackMode"))}};
        if (!record->publication.isEmpty())
            item.insert(QStringLiteral("publication"), record->publication);
        windows.append(item);
    }
    if (!QDir().mkpath(m_dataDirectory)) {
        *error = QStringLiteral("Cannot create the video player data directory. Running changes were not saved.");
        return false;
    }
    const QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("version"), 3},
        {QStringLiteral("windows"), windows}}).toJson(QJsonDocument::Indented);
    qint64 queueBytes = 0;
    for (const auto &value : windows) queueBytes += pathsBytes(value.toObject().value("paths").toArray());
    if (data.size() > MaxStateBytes || queueBytes > MaxQueueBytes) {
        *error = QStringLiteral("Running changes exceed the restorable state size limit and were not saved.");
        return false;
    }
    return writeAtomic(QDir(m_dataDirectory).filePath(QStringLiteral("windows.json")), data, error);
}

bool VideoService::restore(QString *error)
{
    if (!m_windows.empty()) {
        *error = QStringLiteral("Windows can only be restored into an empty service.");
        return false;
    }
    QFile file(QDir(m_dataDirectory).filePath(QStringLiteral("windows.json")));
    if (!file.exists())
        return true;
    if (!file.open(QIODevice::ReadOnly) || file.size() > MaxStateBytes) {
        *error = QStringLiteral("Cannot read windows.json, or it exceeds 64 MiB.");
        return false;
    }
    const QByteArray original = file.readAll();
    file.close();
    QJsonObject object;
    const QString parseError = ScheduleV1::strictJsonObject(original, &object);
    if (!parseError.isEmpty()
        || object.value(QStringLiteral("version")) != QJsonValue(3)
        || !object.value(QStringLiteral("windows")).isArray()) {
        *error = QStringLiteral("windows.json must contain version 3 and a windows array.");
        return false;
    }
    const auto windows = object.value(QStringLiteral("windows")).toArray();
    QSet<QString> ids;
    qint64 queueBytes = 0;
    if (windows.size() > MaxWindows) {
        *error = QStringLiteral("windows.json exceeds the 16 window limit.");
        return false;
    }
    // Validate the entire saved document before creating any visible windows.
    for (const auto &value : windows) {
        const auto item = value.toObject();
        const QString id = item.value(QStringLiteral("windowId")).toString();
        const auto paths = item.value(QStringLiteral("paths")).toArray();
        queueBytes += QJsonDocument(paths).toJson(QJsonDocument::Compact).size();
        const auto repeat = item.value(QStringLiteral("repeat")).toString();
        const QString mode = item.value(QStringLiteral("playbackMode")).toString();
        const QSet<QString> fields{"windowId", "name", "screen", "fullscreen", "paths", "currentIndex",
            "volumePercent", "muted", "repeat", "playbackMode", "publication"};
        bool valid = validDefinition(item) && !ids.contains(id) && queueBytes <= MaxQueueBytes
            && item.value(QStringLiteral("paths")).isArray()
            && paths.size() <= 1000 && integer(item.value(QStringLiteral("volumePercent")), 0, 100)
            && item.value(QStringLiteral("muted")).isBool()
            && (repeat == QStringLiteral("off") || repeat == QStringLiteral("all") || repeat == QStringLiteral("one"))
            && integer(item.value(QStringLiteral("currentIndex")), paths.isEmpty() ? -1 : 0, paths.size() - 1)
            && (mode == QStringLiteral("manual") || mode == QStringLiteral("schedule"))
            && item.value("playbackMode").isString()
            && (!item.contains("publication") || validPublication(item.value("publication")));
        for (auto field = item.begin(); field != item.end(); ++field) valid = valid && fields.contains(field.key());
        for (const auto &path : paths) {
            valid = valid && validPath(path);
        }
        if (!valid) {
            *error = QStringLiteral("windows.json contains an invalid or duplicate window definition.");
            return false;
        }
        ids.insert(id);
    }
    QScopedValueRollback<bool> guard(m_restoring, true);
    for (const auto &value : windows) {
        const auto item = value.toObject();
        Record &record = createWindow(item.value(QStringLiteral("windowId")).toString(),
            item.value(QStringLiteral("name")).toString(), item.value(QStringLiteral("screen")).toString(),
            item.value(QStringLiteral("fullscreen")).toBool());
        record.publication = item.value("publication").toObject();
        record.engine->execute({{"command", "volume"}, {"value", item.value("volumePercent")}});
        record.engine->execute({{"command", "mute"}, {"value", item.value("muted")}});
        record.engine->execute({{"command", "repeat"}, {"mode", item.value("repeat")}});
        // The runtime preference is authoritative even if the process stopped
        // between accepting a command and replacing windows.json.
        const QString runtimeError = record.engine->restoreScheduledPlayback();
        if (!runtimeError.isEmpty()) record.restoreError = runtimeError;
        // File notification is also the recovery path for a missing runtime DB.
        // Accepted runtime remains usable if the publication drive is offline.
        if (!record.engine->status().value("scheduleAvailable").toBool() && !record.publication.isEmpty()) {
            auto request = record.publication;
            request.insert("command", "loadPublication");
            request.insert("autoplay", item.value("playbackMode").toString() == QStringLiteral("schedule"));
            const auto reply = record.engine->execute(request);
            if (!reply.value("ok").toBool())
                record.restoreError = reply.value("error").toObject().value("message").toString();
        }
        if (item.value("playbackMode").toString() == QStringLiteral("schedule")
            && !record.engine->status().value("scheduleAvailable").toBool())
            record.restoreError = tr("Принятый выпуск недоступен. Загрузите расписание из Manager.");
        if (record.engine->status().value("playbackMode").toString() == QStringLiteral("schedule"))
            continue;
        QJsonArray readable;
        QStringList missing;
        int startIndex = 0;
        const auto paths = item.value(QStringLiteral("paths")).toArray();
        const int selected = item.value(QStringLiteral("currentIndex")).toInt();
        for (qsizetype i = 0; i < paths.size(); ++i) {
            const QString path = paths.at(i).toString();
            QFile media(path);
            if (QFileInfo(path).isFile() && media.open(QIODevice::ReadOnly)) {
                if (i == selected)
                    startIndex = readable.size();
                readable.append(path);
            } else {
                missing.append(path);
            }
        }
        if (!readable.isEmpty()) {
            const auto result = record.engine->execute({{QStringLiteral("command"), QStringLiteral("load")},
                {QStringLiteral("paths"), readable}, {QStringLiteral("startIndex"), startIndex},
                {QStringLiteral("autoplay"), false}});
            if (!result.value(QStringLiteral("ok")).toBool())
                record.restoreError = result.value(QStringLiteral("error")).toObject().value(QStringLiteral("message")).toString();
        }
        if (!missing.isEmpty())
            record.restoreError = tr("Пропущены недоступные файлы: %1").arg(missing.join(QStringLiteral("; ")));
    }
    emit statusChanged();
    return true;
}

} // namespace MediaBox
