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
} // namespace

struct VideoService::Record
{
    QString id;
    QString name;
    QString screen;
    QString restoreError;
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
    } else {
        record.engine->execute({{QStringLiteral("command"), QStringLiteral("stop")}});
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
    record->engine = std::make_unique<PlayerEngine>(record->backend.get());
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
        windows.append(QJsonObject{{QStringLiteral("id"), id},
                                   {QStringLiteral("name"), record->name},
                                   {QStringLiteral("screen"), record->screen},
                                   {QStringLiteral("actualScreen"), actual},
                                   {QStringLiteral("fullscreen"), record->window->requestedFullscreen()},
                                   {QStringLiteral("restoreError"), record->restoreError},
                                   {QStringLiteral("playback"), record->engine->status()}});
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
        QStringLiteral("seek"), QStringLiteral("volume"), QStringLiteral("mute"), QStringLiteral("repeat"), QStringLiteral("clear")};
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
        m_windows.erase(it);
        return finishMutation();
    }
    if (command == QStringLiteral("fullscreen")) {
        if (!request.value(QStringLiteral("value")).isBool())
            return invalid(QStringLiteral("value must be a boolean."));
        record.window->setFullscreen(request.value(QStringLiteral("value")).toBool());
        return finishMutation();
    }
    if ((command == QStringLiteral("load") || command == QStringLiteral("enqueue"))
        && request.value(QStringLiteral("paths")).isArray()) {
        qint64 queueBytes = QJsonDocument(request.value(QStringLiteral("paths")).toArray())
                                .toJson(QJsonDocument::Compact).size();
        for (const auto &[otherId, other] : m_windows) {
            if (otherId != id || command == QStringLiteral("enqueue"))
                queueBytes += QJsonDocument(other->engine->status().value(QStringLiteral("queue")).toArray())
                                  .toJson(QJsonDocument::Compact).size();
        }
        if (queueBytes > MaxQueueBytes)
            return failure(QStringLiteral("queue_limit"), QStringLiteral("Combined video queues exceed 8 MiB of JSON paths."));
    }
    // Do not start invisible playback after a selected screen was unplugged.
    if (!resolveScreen(record.screen)
        && (command == QStringLiteral("play") || (command == QStringLiteral("load")
            && request.value(QStringLiteral("autoplay")).toBool())))
        return failure(QStringLiteral("unknown_screen"), QStringLiteral("The selected display is unavailable."));
    QJsonObject playbackRequest = request;
    playbackRequest.remove(QStringLiteral("windowId"));
    QJsonObject response = record.engine->execute(playbackRequest);
    if (!response.value(QStringLiteral("ok")).toBool()) {
        response.insert(QStringLiteral("status"), status());
        return response;
    }
    if (command == QStringLiteral("play") || (command == QStringLiteral("load")
        && request.value(QStringLiteral("autoplay")).toBool()))
        present(record);
    if (command == QStringLiteral("load") || command == QStringLiteral("clear"))
        record.restoreError.clear();
    if (command == QStringLiteral("seek") || command == QStringLiteral("play")
        || command == QStringLiteral("pause") || command == QStringLiteral("stop"))
        return success();
    return finishMutation();
}

bool VideoService::save(QString *error) const
{
    QJsonArray windows;
    for (const auto &[id, record] : m_windows) {
        const QJsonObject playback = record->engine->status();
        windows.append(QJsonObject{{QStringLiteral("windowId"), id}, {QStringLiteral("name"), record->name},
            {QStringLiteral("screen"), record->screen}, {QStringLiteral("fullscreen"), record->window->requestedFullscreen()},
            {QStringLiteral("paths"), playback.value(QStringLiteral("queue"))},
            {QStringLiteral("currentIndex"), playback.value(QStringLiteral("currentIndex"))},
            {QStringLiteral("volumePercent"), playback.value(QStringLiteral("volumePercent"))},
            {QStringLiteral("muted"), playback.value(QStringLiteral("muted"))},
            {QStringLiteral("repeat"), playback.value(QStringLiteral("repeat"))}});
    }
    if (!QDir().mkpath(m_dataDirectory)) {
        *error = QStringLiteral("Cannot create the video player data directory. Running changes were not saved.");
        return false;
    }
    QSaveFile file(QDir(m_dataDirectory).filePath(QStringLiteral("windows.json")));
    file.setDirectWriteFallback(false);
    const QByteArray data = QJsonDocument(QJsonObject{{QStringLiteral("version"), 1},
        {QStringLiteral("windows"), windows}}).toJson(QJsonDocument::Indented);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        *error = QStringLiteral("Running changes were not saved: %1").arg(file.errorString());
        return false;
    }
    return true;
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
    const auto document = QJsonDocument::fromJson(file.readAll());
    const auto object = document.object();
    if (object.value(QStringLiteral("version")) != QJsonValue(1)
        || !object.value(QStringLiteral("windows")).isArray()) {
        *error = QStringLiteral("windows.json must contain version 1 and a windows array.");
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
        bool valid = validDefinition(item) && !ids.contains(id) && queueBytes <= MaxQueueBytes
            && item.value(QStringLiteral("paths")).isArray()
            && paths.size() <= 1000 && integer(item.value(QStringLiteral("volumePercent")), 0, 100)
            && item.value(QStringLiteral("muted")).isBool()
            && (repeat == QStringLiteral("off") || repeat == QStringLiteral("all") || repeat == QStringLiteral("one"))
            && integer(item.value(QStringLiteral("currentIndex")), paths.isEmpty() ? -1 : 0, paths.size() - 1);
        for (const auto &path : paths) {
            valid = valid && path.isString() && !path.toString().isEmpty() && path.toString().size() <= 4096
                && !path.toString().contains(QChar::Null) && QDir::isAbsolutePath(path.toString());
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
        record.engine->execute({{QStringLiteral("command"), QStringLiteral("volume")}, {QStringLiteral("value"), item.value(QStringLiteral("volumePercent"))}});
        record.engine->execute({{QStringLiteral("command"), QStringLiteral("mute")}, {QStringLiteral("value"), item.value(QStringLiteral("muted"))}});
        record.engine->execute({{QStringLiteral("command"), QStringLiteral("repeat")}, {QStringLiteral("mode"), item.value(QStringLiteral("repeat"))}});
    }
    emit statusChanged();
    return true;
}

} // namespace MediaBox
