#include "schedulepublication.h"
#include "schedulecore/schedulev1.h"
#include "schedulecore/schedulecompiler.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLockFile>
#include <QSaveFile>
#include <QTimeZone>
#include <QUuid>
#include <algorithm>
#include <limits>

namespace SchedulePublication {
namespace {
bool fail(QString *error, const QString &message)
{
    if (error) *error = message;
    return false;
}
QString uuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QByteArray json(const QJsonObject &object) { return QJsonDocument(object).toJson(QJsonDocument::Indented); }
bool read(const QString &path, QByteArray *bytes, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return fail(error, path + ": " + file.errorString());
    *bytes = file.readAll();
    return file.error() == QFileDevice::NoError || fail(error, path + ": " + file.errorString());
}
bool write(const QString &path, const QByteArray &bytes, QString *error)
{
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
        return fail(error, path + ": " + file.errorString());
    return true;
}
bool loadProject(const QString &directory, QJsonObject *project, QString *error)
{
    if (!QFileInfo::exists(projectPath(directory))) {
        *project = {{"format", "mediabox.schedule-project"}, {"schemaVersion", 1},
                    {"mode", "channels"}, {"lastRevision", 0}};
        return true;
    }
    QByteArray bytes;
    if (!read(projectPath(directory), &bytes, error)) return false;
    const QString reason = ScheduleV1::strictJsonObject(bytes, project);
    if (!reason.isEmpty()) return fail(error, reason);
    const auto mode = project->value("mode").toString();
    if (project->value("format") != QJsonValue("mediabox.schedule-project")
            || project->value("schemaVersion") != QJsonValue(1)
            || (mode != "channels" && mode != "advanced") || !project->value("document").isObject()
            || !project->value("lastRevision").isDouble()
            || project->value("lastRevision").toDouble() != project->value("lastRevision").toInt(-1)
            || project->value("lastRevision").toInt(-1) < 0)
        return fail(error, QStringLiteral("Некорректный проект расписания: %1").arg(projectPath(directory)));
    return true;
}
QJsonObject comparable(QJsonObject document)
{
    document.remove("publicationId"); document.remove("revision"); document.remove("publishedAt");
    return document;
}

}

QString projectPath(const QString &directory) { return QDir(directory).filePath("schedule-project.json"); }

bool draft(const QString &directory, const QString &contentRoot, const QJsonObject &channels,
           QJsonObject *document, bool *advanced, QString *error, const QString &mediaType)
{
    if (error) error->clear();
    if (!QDir().mkpath(directory)) return fail(error, QStringLiteral("Не удалось создать каталог проекта расписания."));
    QLockFile lock(projectPath(directory) + ".lock");
    if (!lock.tryLock(3000)) return fail(error, QStringLiteral("Проект расписания занят другим редактором."));
    QJsonObject project;
    if (!loadProject(directory, &project, error)) return false;
    const bool saved = project.value("mode") == QJsonValue("advanced");
    if (advanced) *advanced = saved;
    if (saved) { *document = project.value("document").toObject(); return true; }
    QJsonObject result;
    if (!ScheduleCompiler::fromChannels(channels, contentRoot, project.value("document").toObject(), &result, error, mediaType)) return false;
    project.insert("document", result);
    if (!write(projectPath(directory), json(project), error)) return false;
    *document = result;
    return true;
}

bool saveDraft(const QString &directory, const QJsonObject &document, QString *error)
{
    ScheduleV1::Document verified;
    const QString reason = ScheduleV1::decode(document, &verified);
    if (!reason.isEmpty()) return fail(error, reason);
    QLockFile lock(projectPath(directory) + ".lock");
    if (!lock.tryLock(3000)) return fail(error, QStringLiteral("Проект расписания занят другим редактором."));
    QJsonObject project;
    if (!loadProject(directory, &project, error)) return false;
    const auto existing = project.value("document").toObject();
    auto saved = document;
    // Import copies rules into this station's project; it does not silently
    // retarget the station or reset the monotonically increasing publication.
    if (!existing.isEmpty()) {
        saved.insert("scheduleId", existing.value("scheduleId"));
        saved.insert("stationId", existing.value("stationId"));
    }
    const QString savedError = ScheduleV1::decode(saved, &verified);
    if (!savedError.isEmpty()) return fail(error, savedError);
    project.insert("document", saved); project.insert("mode", "advanced");
    return write(projectPath(directory), json(project), error);
}

bool publish(const QString &directory, const QString &contentRoot, const QJsonObject &document,
             Publication *publication, QString *error)
{
    if (error) error->clear();
    QLockFile lock(projectPath(directory) + ".lock");
    if (!lock.tryLock(3000)) return fail(error, QStringLiteral("Проект расписания занят другим редактором."));
    QJsonObject project;
    if (!loadProject(directory, &project, error)) return false;
    const auto expected = project.value("document").toObject();
    if (!expected.isEmpty() && comparable(expected) != comparable(document))
        return fail(error, QStringLiteral("Проект изменён другим редактором. Откройте его заново перед публикацией."));
    const QDir dir(directory);
    const QString activePath = dir.filePath("active.json");
    int previousRevision = project.value("lastRevision").toInt();
    if (QFileInfo::exists(activePath)) {
        QByteArray activeBytes, previousBytes;
        QJsonObject active;
        if (!read(activePath, &activeBytes, error)) return false;
        const auto reason = ScheduleV1::strictJsonObject(activeBytes, &active);
        if (!reason.isEmpty()) return fail(error, reason);
        const QString relative = QStringLiteral("snapshots/%1.json").arg(active.value("publicationId").toString());
        if (QUuid(active.value("publicationId").toString()).isNull()
                || QUuid(active.value("publicationId").toString()).toString(QUuid::WithoutBraces) != active.value("publicationId").toString()
                || active.value("snapshotPath").toString() != relative)
            return fail(error, QStringLiteral("Некорректный путь опубликованного снимка."));
        if (!read(dir.filePath(relative), &previousBytes, error)) return false;
        if (QString::fromLatin1(QCryptographicHash::hash(previousBytes, QCryptographicHash::Sha256).toHex())
                != active.value("sha256").toString()) return fail(error, QStringLiteral("Контрольная сумма предыдущего снимка не совпадает."));
        ScheduleV1::Document previous;
        const QString priorError = ScheduleV1::parse(previousBytes, &previous);
        if (!priorError.isEmpty()) return fail(error, priorError);
        if (active.size() != 8 || active.value("format") != QJsonValue("mediabox.active")
                || active.value("schemaVersion") != QJsonValue(1)
                || active.value("scheduleId") != previous.object.value("scheduleId")
                || active.value("stationId") != previous.object.value("stationId")
                || active.value("publicationId") != previous.object.value("publicationId")
                || active.value("revision") != previous.object.value("revision"))
            return fail(error, QStringLiteral("Указатель выпуска не соответствует снимку расписания."));
        previousRevision = qMax(previousRevision, previous.revision());
        if (comparable(previous.object) == comparable(document)) {
            *publication = {active, previousBytes, activePath, contentRoot};
            return true;
        }
    }
    if (previousRevision == std::numeric_limits<int>::max()) return fail(error, QStringLiteral("Исчерпан номер выпуска."));
    QJsonObject released = document;
    released.insert("publicationId", uuid()); released.insert("revision", previousRevision + 1);
    released.insert("publishedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
    released.insert("requiredCapabilities", QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(released)));
    ScheduleV1::Document verified;
    const QString reason = ScheduleV1::decode(released, &verified);
    if (!reason.isEmpty()) return fail(error, reason);
    const QByteArray bytes = json(released);
    const QString digest = QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex());
    const QString id = released.value("publicationId").toString(), relative = "snapshots/" + id + ".json";
    const QJsonObject active{{"format", "mediabox.active"}, {"schemaVersion", 1},
        {"scheduleId", released.value("scheduleId")}, {"stationId", released.value("stationId")},
        {"publicationId", id}, {"revision", previousRevision + 1}, {"snapshotPath", relative}, {"sha256", digest}};
    if (!dir.mkpath("publications") || !dir.mkpath("snapshots"))
        return fail(error, QStringLiteral("Не удалось создать каталог выпусков расписания."));
    // Persist the exact release before exposing its pointer. Failed publication
    // can leave a recoverable inactive release, never a partially written one.
    project.insert("document", released); project.insert("lastRevision", previousRevision + 1);
    if (!write(projectPath(directory), json(project), error)
            || !write(dir.filePath("publications/" + id + ".json"),
                json(active), error)) return false;
    if (QFileInfo::exists(dir.filePath(relative))) return fail(error, QStringLiteral("Снимок с таким ID уже существует."));
    if (!write(dir.filePath(relative), bytes, error)) return false;
    QByteArray readback;
    if (!read(dir.filePath(relative), &readback, error) || readback != bytes)
        return fail(error, QStringLiteral("Не удалось подтвердить байты записанного снимка."));
    if (!write(activePath, json(active), error)) return false;
    *publication = {active, bytes, activePath, contentRoot};
    return true;
}
}
