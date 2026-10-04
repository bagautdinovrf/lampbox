#include "schedulev1runtime.h"

#include "playbackfilevalidation.h"

#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>
#include <utility>

namespace MediaBox {
namespace {
constexpr qint64 MaxPointerBytes = 64 * 1024;
constexpr qint64 MaxSnapshotBytes = 64 * 1024 * 1024;
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString hash(const QByteArray &bytes)
{
    return QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
}
QByteArray json(const QJsonObject &object) { return QJsonDocument(object).toJson(QJsonDocument::Compact); }
QString queryError(const QSqlQuery &query) { return QStringLiteral("runtime.sqlite: ") + query.lastError().text(); }
QString rotationKey(const ScheduleV1::Evaluation &evaluation)
{
    if (evaluation.mixRuleId.isEmpty())
        return {};
    return evaluation.mixRuleId + u'|' + evaluation.activationStart.toUTC().toString(Qt::ISODateWithMs)
        + u'|' + hash(QJsonDocument(evaluation.pattern).toJson(QJsonDocument::Compact));
}
bool expired(const ScheduleV1::EventOccurrence &event, const QDateTime &now)
{
    // Contract times and lateness have one-second resolution. In particular,
    // maxLateSeconds=0 still permits delivery during its scheduled second.
    return event.scheduledUtc.secsTo(now) > event.maxLateSeconds;
}

// canonicalFilePath resolves links and junctions, including the configured root.
QString containedFile(const QString &root, const QString &relative)
{
    if (relative.isEmpty() || relative.contains(u'\\') || relative.contains(u':')
        || QDir::isAbsolutePath(relative))
        return {};
    const auto segments = relative.split(u'/');
    for (const QString &segment : segments)
        if (segment.isEmpty() || segment == QStringLiteral(".") || segment == QStringLiteral(".."))
            return {};
    const QString canonicalRoot = QFileInfo(root).canonicalFilePath();
    const QString canonical = QFileInfo(QDir(root).filePath(relative)).canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonical.isEmpty())
        return {};
#ifdef Q_OS_WIN
    constexpr auto sensitivity = Qt::CaseInsensitive;
#else
    constexpr auto sensitivity = Qt::CaseSensitive;
#endif
    const QString prefix = canonicalRoot.endsWith(u'/') ? canonicalRoot : canonicalRoot + u'/';
    if (!canonical.startsWith(prefix, sensitivity))
        return {};
    return canonical;
}

QString validateActive(const QJsonObject &active, const ScheduleV1::Document &document, const QByteArray &bytes)
{
    if (active.isEmpty())
        return {};
    const QSet<QString> keys{QStringLiteral("format"), QStringLiteral("schemaVersion"), QStringLiteral("scheduleId"),
        QStringLiteral("stationId"), QStringLiteral("publicationId"), QStringLiteral("revision"),
        QStringLiteral("snapshotPath"), QStringLiteral("sha256")};
    const auto activeKeys = active.keys();
    if (QSet<QString>(activeKeys.begin(), activeKeys.end()) != keys)
        return QStringLiteral("Указатель active.json содержит неверный набор полей.");
    if (active.value("format") != QJsonValue("mediabox.active") || active.value("schemaVersion") != QJsonValue(1)
        || active.value("scheduleId") != document.object.value("scheduleId")
        || active.value("stationId") != document.object.value("stationId")
        || active.value("publicationId") != document.object.value("publicationId")
        || active.value("revision") != document.object.value("revision")
        || active.value("snapshotPath").toString() != QStringLiteral("snapshots/%1.json").arg(document.publicationId())
        || active.value("sha256").toString() != hash(bytes))
        return QStringLiteral("Указатель, идентификаторы или SHA-256 снимка не совпадают.");
    return {};
}
}

ScheduleV1Runtime::ScheduleV1Runtime(QString databasePath, std::function<QDateTime()> clock, const QString &mediaType)
    : m_mediaType(mediaType), m_databasePath(std::move(databasePath)), m_clock(std::move(clock))
{
    if (m_databasePath.isEmpty())
        m_databasePath = QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
            .filePath(QStringLiteral("runtime.sqlite"));
    if (!m_clock)
        m_clock = [] { return QDateTime::currentDateTime(); };
    m_maintenanceTimer.setInterval(60 * 60 * 1000);
    QObject::connect(&m_maintenanceTimer, &QTimer::timeout, &m_maintenanceTimer, [this] { pruneHistory(); });
}

ScheduleV1Runtime::~ScheduleV1Runtime()
{
    m_maintenanceTimer.stop();
    if (!m_connectionName.isEmpty()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
}

QJsonArray ScheduleV1Runtime::supportedCapabilities() const
{
    QJsonArray capabilities{"schedule.current.v1", "calendar.v1", "rotation.strict.v1", "events.fixed.v1"};
    if (m_mediaType == QStringLiteral("video"))
        capabilities.append(QStringLiteral("media.video.v1"));
    return capabilities;
}

QString ScheduleV1Runtime::validateMedia(const ScheduleV1::Document &document) const
{
    if (m_mediaType != QStringLiteral("audio") && m_mediaType != QStringLiteral("video"))
        return QStringLiteral("Неподдерживаемый тип проигрывателя.");
    const auto supported = supportedCapabilities();
    for (const auto &capability : document.object.value("requiredCapabilities").toArray())
        if (!supported.contains(capability))
            return QStringLiteral("Проигрыватель не поддерживает возможность: ") + capability.toString();
    for (const auto &value : document.object.value("assets").toArray()) {
        const QString type = value.toObject().value("mediaType").toString();
        if (type != QStringLiteral("audio") && !(m_mediaType == QStringLiteral("video") && type == QStringLiteral("video")))
            return QStringLiteral("Аудиоплеер не поддерживает видео в расписании.");
    }
    return {};
}

QString ScheduleV1Runtime::sqlError() const { return QStringLiteral("runtime.sqlite: ") + m_db.lastError().text(); }

QString ScheduleV1Runtime::open()
{
    if (m_opened)
        return {};
    if (m_databasePath != QStringLiteral(":memory:") && !QDir().mkpath(QFileInfo(m_databasePath).absolutePath()))
        return QStringLiteral("Не удалось создать каталог runtime.sqlite.");
    if (!m_connectionName.isEmpty()) {
        m_db.close();
        m_db = QSqlDatabase();
        QSqlDatabase::removeDatabase(m_connectionName);
    }
    m_connectionName = QStringLiteral("schedule-v1-") + uuid();
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(m_databasePath);
    if (!m_db.open())
        return sqlError();
    const QStringList pragmas{
        QStringLiteral("PRAGMA busy_timeout=5000"),
        QStringLiteral("PRAGMA journal_mode=WAL"),
        QStringLiteral("PRAGMA synchronous=FULL")};
    for (const QString &statement : pragmas) {
        QSqlQuery query(m_db);
        if (!query.exec(statement))
            return queryError(query);
    }
    if (!m_db.transaction())
        return sqlError();
    const auto schemaFailure = [this](const QString &error) {
        m_db.rollback();
        return error;
    };
    const QStringList statements{
        QStringLiteral("CREATE TABLE IF NOT EXISTS accepted (singleton INTEGER PRIMARY KEY CHECK(singleton=1), bytes BLOB NOT NULL, root TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS publications (publication_id TEXT PRIMARY KEY, schedule_id TEXT NOT NULL, station_id TEXT NOT NULL, revision INTEGER NOT NULL, sha256 TEXT NOT NULL, last_used_at TEXT NOT NULL DEFAULT '', UNIQUE(schedule_id, revision))"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS runtime_state (schedule_id TEXT PRIMARY KEY, state BLOB NOT NULL, updated_at TEXT NOT NULL DEFAULT '')"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS preferences (name TEXT PRIMARY KEY, value INTEGER NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS publication_source (singleton INTEGER PRIMARY KEY CHECK(singleton=1), active_path TEXT NOT NULL, root TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS diagnostics (id INTEGER PRIMARY KEY, at TEXT NOT NULL, publication_id TEXT NOT NULL, playback_id TEXT NOT NULL, message TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS starts (playback_id TEXT PRIMARY KEY, schedule_id TEXT NOT NULL, publication_id TEXT NOT NULL, entry_id TEXT NOT NULL, at TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS events (event_key TEXT PRIMARY KEY, schedule_id TEXT NOT NULL, rule_id TEXT NOT NULL, scheduled_utc TEXT NOT NULL, state TEXT NOT NULL, diagnostic TEXT NOT NULL DEFAULT '')")};
    for (const QString &statement : statements) {
        QSqlQuery query(m_db);
        if (!query.exec(statement))
            return schemaFailure(queryError(query));
    }
    const QStringList indexes{
        QStringLiteral("CREATE INDEX IF NOT EXISTS starts_at ON starts(at)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS diagnostics_at ON diagnostics(at)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS events_scheduled_utc ON events(scheduled_utc)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS publications_last_used_at ON publications(last_used_at)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS runtime_state_updated_at ON runtime_state(updated_at)")};
    for (const QString &statement : indexes) {
        QSqlQuery query(m_db);
        if (!query.exec(statement))
            return schemaFailure(queryError(query));
    }
    QSqlQuery recovery(m_db);
    if (!recovery.exec(QStringLiteral("UPDATE events SET state='failed', diagnostic='Неоднозначное завершение после перезапуска; повтор запрещён' WHERE state IN ('starting','started')")))
        return schemaFailure(queryError(recovery));
    if (recovery.numRowsAffected() > 0)
        m_recoveryDiagnostic = QStringLiteral("После перезапуска обнаружены незавершённые события; повторный запуск запрещён.");
    if (!m_db.commit())
        return schemaFailure(sqlError());
    QSqlQuery cutoff(m_db);
    if (!cutoff.exec(QStringLiteral("SELECT value FROM preferences WHERE name='retentionCutoffMs'")))
        return queryError(cutoff);
    if (cutoff.next())
        m_retentionCutoffMs = cutoff.value(0).toLongLong();
    cutoff.finish();
    const QString restoreError = readAccepted();
    if (!restoreError.isEmpty())
        return restoreError;
    m_opened = true;
    // Cleanup errors are diagnostic only; the next hourly run retries them.
    pruneHistory();
    m_maintenanceTimer.start();
    return {};
}

QString ScheduleV1Runtime::readAccepted()
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT bytes, root FROM accepted WHERE singleton=1")))
        return queryError(query);
    if (!query.next())
        return {};
    ScheduleV1::Document document;
    QString error = ScheduleV1::parse(query.value(0).toByteArray(), &document);
    if (!error.isEmpty())
        return error;
    error = validateMedia(document);
    if (!error.isEmpty())
        return error;
    const QString root = query.value(1).toString();
    QSqlQuery state(m_db);
    state.prepare(QStringLiteral("SELECT state FROM runtime_state WHERE schedule_id=?"));
    state.addBindValue(document.scheduleId());
    if (!state.exec())
        return queryError(state);
    const auto restoredState = state.next() ? QJsonDocument::fromJson(state.value(0).toByteArray()).object() : QJsonObject();
    m_document = std::move(document);
    m_contentRoot = root;
    m_state = restoredState;
    return {};
}

QString ScheduleV1Runtime::restore()
{
    QString error = open();
    if (!error.isEmpty())
        return error;
    error = readAccepted();
    if (!error.isEmpty())
        return error;
    QSqlQuery source(m_db);
    if (!source.exec(QStringLiteral("SELECT active_path,root FROM publication_source WHERE singleton=1")))
        return queryError(source);
    if (!source.next() || source.value(0).toString().isEmpty())
        return {};
    const QString activePath = source.value(0).toString();
    const QString root = source.value(1).toString();
    source.finish();
    error = loadPublication(activePath, root, m_clock());
    if (!error.isEmpty()) {
        if (!available())
            return error;
        m_sourceError = QStringLiteral("Не удалось обновить расписание из файлов; сохранён последний принятый выпуск: ") + error;
    }
    return {};
}

QString ScheduleV1Runtime::pruneHistory()
{
    if (!m_opened) {
        const QString error = open();
        if (!error.isEmpty()) {
            m_maintenanceError = QStringLiteral("Не удалось очистить историю: ") + error;
            qWarning().noquote() << m_maintenanceError;
        }
        return m_maintenanceError;
    }
    const auto maintenanceFailure = [this](const QString &error) {
        m_maintenanceError = QStringLiteral("Не удалось очистить историю: ") + error;
        qWarning().noquote() << m_maintenanceError;
        return m_maintenanceError;
    };
    const QDateTime cutoffTime = m_clock().toUTC().addDays(-30);
    if (!cutoffTime.isValid())
        return maintenanceFailure(QStringLiteral("недоступно текущее время."));
    if (!m_db.transaction())
        return maintenanceFailure(sqlError());
    const auto rollback = [this, &maintenanceFailure](const QString &error) {
        m_db.rollback();
        return maintenanceFailure(error);
    };
    qint64 eventCutoffMs = std::max(m_retentionCutoffMs, cutoffTime.toMSecsSinceEpoch());
    QSqlQuery savedCutoff(m_db);
    if (!savedCutoff.exec(QStringLiteral("SELECT value FROM preferences WHERE name='retentionCutoffMs'")))
        return rollback(queryError(savedCutoff));
    if (savedCutoff.next())
        eventCutoffMs = std::max(eventCutoffMs, savedCutoff.value(0).toLongLong());
    savedCutoff.finish();
    const QString cutoff = cutoffTime.toString(Qt::ISODateWithMs);
    const QString eventCutoff = QDateTime::fromMSecsSinceEpoch(eventCutoffMs).toUTC().toString(Qt::ISODateWithMs);
    const QList<QPair<QString, QString>> historyDeletes{
        {QStringLiteral("DELETE FROM starts WHERE at < ?"), cutoff},
        {QStringLiteral("DELETE FROM diagnostics WHERE at < ?"), cutoff},
        {QStringLiteral("DELETE FROM events WHERE scheduled_utc < ?"), eventCutoff}};
    for (const auto &[statement, threshold] : historyDeletes) {
        QSqlQuery query(m_db);
        query.prepare(statement);
        query.addBindValue(threshold);
        if (!query.exec())
            return rollback(queryError(query));
    }
    const QList<QPair<QString, QString>> metadataDeletes{
        {QStringLiteral("DELETE FROM publications WHERE last_used_at < ? AND publication_id <> ?"), m_document.publicationId()},
        {QStringLiteral("DELETE FROM runtime_state WHERE updated_at < ? AND schedule_id <> ?"), m_document.scheduleId()}};
    for (const auto &[statement, activeId] : metadataDeletes) {
        QSqlQuery query(m_db);
        query.prepare(statement);
        query.addBindValue(cutoff);
        query.addBindValue(activeId.isNull() ? QStringLiteral("") : activeId);
        if (!query.exec())
            return rollback(queryError(query));
    }
    // Commit the deletion boundary with the deletions. Even after a clock
    // rollback or restart, a forgotten advert occurrence must not play again.
    QSqlQuery rememberCutoff(m_db);
    rememberCutoff.prepare(QStringLiteral("INSERT OR REPLACE INTO preferences(name,value) VALUES('retentionCutoffMs',?)"));
    rememberCutoff.addBindValue(eventCutoffMs);
    if (!rememberCutoff.exec())
        return rollback(queryError(rememberCutoff));
    if (!m_db.commit())
        return rollback(sqlError());
    m_retentionCutoffMs = eventCutoffMs;
    m_maintenanceError.clear();
    return {};
}

QString ScheduleV1Runtime::loadPublication(const QString &activePath, const QString &root, const QDateTime &now)
{
    QFile pointer(activePath);
    if (!pointer.open(QIODevice::ReadOnly))
        return QStringLiteral("Не удалось прочитать active.json: ") + pointer.errorString();
    QJsonObject active;
    const QByteArray pointerBytes = pointer.read(MaxPointerBytes + 1);
    if (pointerBytes.size() > MaxPointerBytes || !pointer.atEnd())
        return QStringLiteral("active.json превышает допустимые 64 KiB.");
    QString error = ScheduleV1::strictJsonObject(pointerBytes, &active);
    if (!error.isEmpty())
        return error;
    const QString relative = active.value("snapshotPath").toString();
    static const QRegularExpression pathPattern(QStringLiteral("^snapshots/[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}\\.json$"));
    if (!pathPattern.match(relative).hasMatch())
        return QStringLiteral("Некорректный путь снимка в active.json.");
    const QString snapshotPath = containedFile(QFileInfo(activePath).absolutePath(), relative);
    if (snapshotPath.isEmpty())
        return QStringLiteral("Файл снимка отсутствует или выходит за каталог публикации.");
    QFile snapshot(snapshotPath);
    if (!snapshot.open(QIODevice::ReadOnly))
        return snapshot.errorString();
    const QByteArray snapshotBytes = snapshot.read(MaxSnapshotBytes + 1);
    if (snapshotBytes.size() > MaxSnapshotBytes || !snapshot.atEnd())
        return QStringLiteral("Файл расписания превышает допустимые 64 MiB.");
    error = accept(snapshotBytes, root, active, now, QFileInfo(activePath).absoluteFilePath());
    if (error.isEmpty())
        m_sourceError.clear();
    return error;
}

QString ScheduleV1Runtime::accept(const QByteArray &bytes, const QString &root, const QJsonObject &active, const QDateTime &now, const QString &activePath)
{
    ScheduleV1::Document document;
    QString error = ScheduleV1::parse(bytes, &document);
    if (!error.isEmpty())
        return error;
    error = validateMedia(document);
    if (!error.isEmpty())
        return error;
    error = validateActive(active, document, bytes);
    if (!error.isEmpty())
        return error;
    error = open();
    if (!error.isEmpty())
        return error;
    if (!available()) {
        error = readAccepted();
        if (!error.isEmpty())
            return error;
    }
    const QString chosenRoot = root.isEmpty() ? m_contentRoot : root;
    if (!QDir::isAbsolutePath(chosenRoot) || !QFileInfo(chosenRoot).isDir())
        return QStringLiteral("contentRoot должен указывать на существующий абсолютный каталог контента.");
    for (const auto &value : document.object.value("assets").toArray()) {
        const QString relative = value.toObject().value("path").toString();
        if (!QFileInfo(QDir(chosenRoot).filePath(relative)).canonicalFilePath().isEmpty()
            && containedFile(chosenRoot, relative).isEmpty())
            return QStringLiteral("Медиафайл через ссылку выходит за каталог контента: ") + relative;
    }
    const QString digest = hash(bytes);
    QSqlQuery identity(m_db);
    identity.prepare(QStringLiteral("SELECT sha256 FROM publications WHERE publication_id=?"));
    identity.addBindValue(document.publicationId());
    if (!identity.exec())
        return queryError(identity);
    const bool known = identity.next();
    if (known && identity.value(0).toString() != digest)
        return QStringLiteral("publicationId уже принят с другими байтами.");
    QSqlQuery latest(m_db);
    latest.prepare(QStringLiteral("SELECT MAX(revision) FROM publications WHERE schedule_id=?"));
    latest.addBindValue(document.scheduleId());
    if (!latest.exec() || !latest.next())
        return queryError(latest);
    const int maximum = latest.value(0).toInt();
    if (document.revision() < maximum || (!known && document.revision() <= maximum))
        return QStringLiteral("Ревизия публикации не новее последней принятой.");
    if (available() && document.stationId() != m_document.stationId())
        return QStringLiteral("Выпуск предназначен другой станции.");
    QJsonObject acceptedState = m_state;
    if (document.scheduleId() != m_document.scheduleId()) {
        QSqlQuery state(m_db);
        state.prepare(QStringLiteral("SELECT state FROM runtime_state WHERE schedule_id=?"));
        state.addBindValue(document.scheduleId());
        if (!state.exec())
            return queryError(state);
        acceptedState = state.next() ? QJsonDocument::fromJson(state.value(0).toByteArray()).object() : QJsonObject();
    } else {
        const auto previous = ScheduleV1::evaluate(m_document, now);
        const auto next = ScheduleV1::evaluate(document, now);
        auto rotation = acceptedState.value("rotation").toObject();
        // A changed validity boundary may clip the new compiled interval even
        // though the station actually remained in this same rotation throughout.
        if (!previous.mixRuleId.isEmpty() && previous.mixRuleId == next.mixRuleId && previous.pattern == next.pattern
            && rotation.value("key").toString() == rotationKey(previous)) {
            rotation.insert("key", rotationKey(next));
            acceptedState.insert("rotation", rotation);
        }
    }
    if (!m_db.transaction())
        return sqlError();
    const QString acceptedAt = m_clock().toUTC().toString(Qt::ISODateWithMs);
    // A long-lived active snapshot/state gets its full retention period only
    // after it stops being active, not from its original acceptance time.
    if (available() && document.publicationId() != m_document.publicationId()) {
        QSqlQuery previous(m_db);
        previous.prepare(QStringLiteral("UPDATE publications SET last_used_at=? WHERE publication_id=?"));
        previous.addBindValue(acceptedAt);
        previous.addBindValue(m_document.publicationId());
        if (!previous.exec())
            error = queryError(previous);
    }
    if (error.isEmpty() && available() && document.scheduleId() != m_document.scheduleId()) {
        QSqlQuery previous(m_db);
        previous.prepare(QStringLiteral("UPDATE runtime_state SET updated_at=? WHERE schedule_id=?"));
        previous.addBindValue(acceptedAt);
        previous.addBindValue(m_document.scheduleId());
        if (!previous.exec())
            error = queryError(previous);
    }
    QSqlQuery publication(m_db);
    publication.prepare(QStringLiteral("INSERT INTO publications(publication_id,schedule_id,station_id,revision,sha256,last_used_at) VALUES(?,?,?,?,?,?) ON CONFLICT(publication_id) DO UPDATE SET last_used_at=excluded.last_used_at"));
    publication.addBindValue(document.publicationId());
    publication.addBindValue(document.scheduleId());
    publication.addBindValue(document.stationId());
    publication.addBindValue(document.revision());
    publication.addBindValue(digest);
    publication.addBindValue(acceptedAt);
    QSqlQuery accepted(m_db);
    accepted.prepare(QStringLiteral("INSERT OR REPLACE INTO accepted(singleton,bytes,root) VALUES(1,?,?)"));
    accepted.addBindValue(bytes);
    accepted.addBindValue(QDir::cleanPath(chosenRoot));
    if (error.isEmpty() && !publication.exec())
        error = queryError(publication);
    if (error.isEmpty() && !accepted.exec())
        error = queryError(accepted);
    if (error.isEmpty())
        error = writeState(document.scheduleId(), acceptedState);
    if (error.isEmpty()) {
        QSqlQuery source(m_db);
        source.prepare(QStringLiteral("INSERT OR REPLACE INTO publication_source(singleton,active_path,root) VALUES(1,?,?)"));
        source.addBindValue(activePath.isNull() ? QStringLiteral("") : activePath);
        source.addBindValue(QDir::cleanPath(chosenRoot));
        if (!source.exec())
            error = queryError(source);
    }
    if (!error.isEmpty() || !m_db.commit()) {
        if (error.isEmpty())
            error = sqlError();
        m_db.rollback();
        return error;
    }
    m_document = std::move(document);
    m_contentRoot = QDir::cleanPath(chosenRoot);
    m_state = acceptedState;
    return {};
}

QString ScheduleV1Runtime::setScheduledPlayback(bool enabled)
{
    const QString error = open();
    if (!error.isEmpty())
        return error;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO preferences(name,value) VALUES('scheduledPlayback',?)"));
    query.addBindValue(enabled ? 1 : 0);
    if (!query.exec()) {
        m_storageError = queryError(query);
        return m_storageError;
    }
    m_storageError.clear();
    return {};
}

bool ScheduleV1Runtime::scheduledPlaybackEnabled() const
{
    if (!m_opened)
        return false;
    QSqlQuery query(m_db);
    return query.exec(QStringLiteral("SELECT value FROM preferences WHERE name='scheduledPlayback'"))
        && query.next() && query.value(0).toInt() == 1;
}

QString ScheduleV1Runtime::assetPath(const QString &id) const
{
    for (const auto &value : m_document.object.value("assets").toArray()) {
        const auto asset = value.toObject();
        if (asset.value("id").toString() == id) {
            const QString path = containedFile(m_contentRoot, asset.value("path").toString());
            return !path.isEmpty() && playbackFileError(path).isEmpty() ? path : QString();
        }
    }
    return {};
}

QJsonObject ScheduleV1Runtime::playlist(const QString &id) const
{
    for (const auto &value : m_document.object.value("playlists").toArray())
        if (value.toObject().value("id").toString() == id)
            return value.toObject();
    return {};
}

QString ScheduleV1Runtime::writeState(const QString &id, const QJsonObject &state)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT OR REPLACE INTO runtime_state(schedule_id,state,updated_at) VALUES(?,?,?)"));
    query.addBindValue(id);
    query.addBindValue(json(state));
    query.addBindValue(m_clock().toUTC().toString(Qt::ISODateWithMs));
    return query.exec() ? QString() : queryError(query);
}

ScheduleV1Runtime::Track ScheduleV1Runtime::selectPlaylist(const QString &id, const QSet<QString> &failed)
{
    Track selected;
    const QJsonObject list = playlist(id);
    const auto entries = list.value("entries").toArray();
    if (entries.isEmpty())
        return selected;
    auto cursors = m_state.value("cursors").toObject();
    auto cursor = cursors.value(id).toObject();
    QStringList ordered;
    QHash<QString, QString> paths;
    for (const auto &value : entries) {
        const auto entry = value.toObject();
        const QString entryId = entry.value("id").toString();
        ordered.append(entryId);
        const QString path = assetPath(entry.value("assetId").toString());
        if (!path.isEmpty() && !failed.contains(path))
            paths.insert(entryId, path);
    }
    if (paths.isEmpty())
        return selected;
    const bool shuffle = list.value("order").toString() == QStringLiteral("shuffle_cycle");
    const bool changedOrder = cursor.value("order").toString() != list.value("order").toString();
    QString entryId;
    if (shuffle) {
        QStringList remaining;
        if (!changedOrder)
            for (const auto &value : cursor.value("remaining").toArray())
                if (ordered.contains(value.toString()))
                    remaining.append(value.toString());
        bool anyAvailable = false;
        for (const QString &candidate : remaining)
            anyAvailable |= paths.contains(candidate);
        if (!anyAvailable) {
            remaining = ordered;
            std::shuffle(remaining.begin(), remaining.end(), *QRandomGenerator::global());
        }
        // Save the unconsumed permutation before exposing a speculative choice.
        cursor.insert("remaining", QJsonArray::fromStringList(remaining));
        cursor.insert("order", list.value("order"));
        cursor.insert("revision", list.value("revision"));
        cursors.insert(id, cursor);
        QJsonObject preparedState = m_state;
        preparedState.insert("cursors", cursors);
        const QString error = writeState(m_document.scheduleId(), preparedState);
        if (!error.isEmpty()) {
            m_storageError = error;
            return {};
        }
        m_state = preparedState;
        for (const QString &candidate : remaining)
            if (paths.contains(candidate)) {
                entryId = candidate;
                break;
            }
        remaining.removeAll(entryId);
        cursor.insert("remaining", QJsonArray::fromStringList(remaining));
    } else {
        const int last = ordered.indexOf(cursor.value("lastEntry").toString());
        for (int offset = 1; offset <= ordered.size(); ++offset) {
            const QString candidate = ordered.at((last + offset) % ordered.size());
            if (paths.contains(candidate)) {
                entryId = candidate;
                break;
            }
        }
    }
    cursor.insert("lastEntry", entryId);
    cursor.insert("order", list.value("order"));
    cursor.insert("revision", list.value("revision"));
    selected.playbackId = uuid();
    selected.scheduleId = m_document.scheduleId();
    selected.publicationId = m_document.publicationId();
    selected.playlistId = id;
    selected.entryId = entryId;
    selected.path = paths.value(entryId);
    selected.name = list.value("name").toString();
    selected.cursor = cursor;
    return selected;
}

ScheduleV1Runtime::Track ScheduleV1Runtime::selectMusic(const QDateTime &now, const QSet<QString> &failed)
{
    if (!available())
        return {};
    const auto evaluation = ScheduleV1::evaluate(m_document, now);
    m_diagnostic = evaluation.diagnostics.join(u'\n');
    if (!evaluation.withinValidity)
        m_diagnostic = QStringLiteral("Срок действия публикации истёк; используется резерв.");
    if (evaluation.silence)
        return {};
    QString baseId = evaluation.playlistId;
    int volume = evaluation.volumePercent;
    Track base = selectPlaylist(baseId, failed);
    if (!base.isValid()) {
        const auto fallback = m_document.object.value("fallback").toObject();
        baseId = fallback.value("source").toObject().value("playlistId").toString();
        base = selectPlaylist(baseId, failed);
        volume = fallback.value("volumePercent").toInt();
        m_diagnostic = QStringLiteral("Основной список недоступен; используется резерв.");
    }
    if (!base.isValid()) {
        m_diagnostic = QStringLiteral("Нет доступных записей основного и резервного списка; тишина.");
        return {};
    }
    if (!evaluation.mixRuleId.isEmpty() && !evaluation.pattern.isEmpty()) {
        const QString key = rotationKey(evaluation);
        const auto rotation = m_state.value("rotation").toObject();
        const int phase = rotation.value("key").toString() == key
            ? rotation.value("phase").toInt() % evaluation.pattern.size() : 0;
        const auto source = evaluation.pattern.at(phase).toObject();
        if (source.value("type").toString() == QStringLiteral("playlist")) {
            Track additional = selectPlaylist(source.value("playlistId").toString(), failed);
            if (additional.isValid())
                base = additional;
            else {
                m_diagnostic = QStringLiteral("Дополнительный список недоступен; его слот исполняется основным треком.");
                base.deviation = m_diagnostic;
            }
        }
        base.mixKey = key;
        base.nextPhase = (phase + 1) % evaluation.pattern.size();
    }
    base.volumePercent = volume;
    return base;
}

QString ScheduleV1Runtime::confirmStarted(const Track &track)
{
    const auto storageFailure = [this](const QString &error) {
        m_storageError = error;
        return error;
    };
    if (!track.isValid() || !m_opened)
        return QStringLiteral("Не выбрано воспроизведение для подтверждения.");
    QSqlQuery seen(m_db);
    seen.prepare(QStringLiteral("SELECT 1 FROM starts WHERE playback_id=?"));
    seen.addBindValue(track.playbackId);
    if (!seen.exec())
        return storageFailure(queryError(seen));
    if (seen.next())
        return {};
    if (!m_db.transaction())
        return storageFailure(sqlError());
    const QString startedAt = m_clock().toUTC().toString(Qt::ISODateWithMs);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("INSERT INTO starts(playback_id,schedule_id,publication_id,entry_id,at) VALUES(?,?,?,?,?)"));
    query.addBindValue(track.playbackId);
    query.addBindValue(track.scheduleId);
    query.addBindValue(track.publicationId);
    query.addBindValue(track.entryId.isNull() ? QStringLiteral("") : track.entryId);
    query.addBindValue(startedAt);
    QString error;
    QJsonObject state = m_state;
    if (!query.exec())
        error = queryError(query);
    if (error.isEmpty() && track.event) {
        error = finishEvent(track, QStringLiteral("started"));
    } else if (error.isEmpty()) {
        auto cursors = state.value("cursors").toObject();
        cursors.insert(track.playlistId, track.cursor);
        state.insert("cursors", cursors);
        if (!track.mixKey.isEmpty())
            state.insert("rotation", QJsonObject{{"key", track.mixKey}, {"phase", track.nextPhase}});
        error = writeState(track.scheduleId, state);
    }
    if (error.isEmpty() && !track.deviation.isEmpty()) {
        QSqlQuery diagnostic(m_db);
        diagnostic.prepare(QStringLiteral("INSERT INTO diagnostics(at,publication_id,playback_id,message) VALUES(?,?,?,?)"));
        diagnostic.addBindValue(startedAt);
        diagnostic.addBindValue(track.publicationId);
        diagnostic.addBindValue(track.playbackId);
        diagnostic.addBindValue(track.deviation);
        if (!diagnostic.exec())
            error = queryError(diagnostic);
    }
    if (!error.isEmpty() || !m_db.commit()) {
        const QString finalError = error.isEmpty() ? sqlError() : error;
        m_db.rollback();
        return storageFailure(finalError);
    }
    if (!track.event)
        m_state = state;
    return {};
}

QString ScheduleV1Runtime::eventKey(const ScheduleV1::EventOccurrence &event) const
{
    return m_document.scheduleId() + u'|' + event.ruleId + u'|' + event.scheduledUtc.toUTC().toString(Qt::ISODateWithMs);
}

QList<ScheduleV1::EventOccurrence> ScheduleV1Runtime::dueEvents(const QDateTime &now)
{
    QList<ScheduleV1::EventOccurrence> result;
    if (!available() || !ScheduleV1::evaluate(m_document, now).withinValidity)
        return result;
    const QDateTime second = now.addMSecs(-now.time().msec());
    for (const auto &event : ScheduleV1::events(m_document, second.addSecs(-3600), now)) {
        if (event.scheduledUtc.toMSecsSinceEpoch() < m_retentionCutoffMs)
            continue;
        const QString key = eventKey(event);
        QSqlQuery insert(m_db);
        insert.prepare(QStringLiteral("INSERT OR IGNORE INTO events(event_key,schedule_id,rule_id,scheduled_utc,state) VALUES(?,?,?,?,?)"));
        insert.addBindValue(key);
        insert.addBindValue(m_document.scheduleId());
        insert.addBindValue(event.ruleId);
        insert.addBindValue(event.scheduledUtc.toUTC().toString(Qt::ISODateWithMs));
        insert.addBindValue(expired(event, now) ? QStringLiteral("skipped") : QStringLiteral("pending"));
        if (!insert.exec()) {
            m_storageError = queryError(insert);
            return {};
        }
        QSqlQuery state(m_db);
        state.prepare(QStringLiteral("SELECT state FROM events WHERE event_key=?"));
        state.addBindValue(key);
        if (!state.exec() || !state.next()) {
            m_storageError = queryError(state);
            return {};
        }
        if (state.value(0).toString() != QStringLiteral("pending"))
            continue;
        if (expired(event, now)) {
            Track skipped;
            skipped.eventKey = key;
            finishEvent(skipped, QStringLiteral("skipped"));
            continue;
        }
        result.append(event);
    }
    std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        if (a.scheduledUtc != b.scheduledUtc) return a.scheduledUtc < b.scheduledUtc;
        return a.ruleId < b.ruleId;
    });
    return result;
}

ScheduleV1Runtime::Track ScheduleV1Runtime::startEvent(const ScheduleV1::EventOccurrence &event, const QDateTime &now)
{
    if (event.scheduledUtc.toMSecsSinceEpoch() < m_retentionCutoffMs)
        return {};
    Track track;
    track.event = true;
    track.eventKey = eventKey(event);
    if (expired(event, now)) {
        finishEvent(track, QStringLiteral("skipped"));
        return {};
    }
    track.path = assetPath(event.assetId);
    if (!track.isValid()) {
        finishEvent(track, QStringLiteral("failed"));
        m_diagnostic = QStringLiteral("Аудиофайл рекламного события отсутствует или недоступен.");
        return {};
    }
    QSqlQuery starting(m_db);
    starting.prepare(QStringLiteral("UPDATE events SET state='starting' WHERE event_key=? AND state='pending'"));
    starting.addBindValue(track.eventKey);
    if (!starting.exec()) {
        m_storageError = queryError(starting);
        return {};
    }
    if (starting.numRowsAffected() != 1) {
        m_diagnostic = QStringLiteral("Событие уже исполнялось.");
        return {};
    }
    track.playbackId = uuid();
    track.scheduleId = m_document.scheduleId();
    track.publicationId = m_document.publicationId();
    track.volumePercent = event.volumePercent;
    for (const auto &value : m_document.object.value("eventRules").toArray())
        if (value.toObject().value("id").toString() == event.ruleId)
            track.name = value.toObject().value("name").toString();
    return track;
}

QString ScheduleV1Runtime::finishEvent(const Track &track, const QString &state)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE events SET state=? WHERE event_key=?"));
    query.addBindValue(state);
    query.addBindValue(track.eventKey);
    if (!query.exec()) {
        m_storageError = queryError(query);
        return m_storageError;
    }
    m_storageError.clear();
    return {};
}

} // namespace MediaBox
