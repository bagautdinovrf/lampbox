#pragma once
#include "schedulecore/schedulecore.h"
#include <QByteArray>

namespace ProjectRepository {
struct Paths {
    QString stationDirectory;
    QString musicDirectory;
    QString videoDirectory;
};
struct Project {
    QList<ScheduleCore::ChannelRule> music;
    QList<ScheduleCore::ChannelRule> video;
    QList<ScheduleCore::AdvertRule> advert;
};

QString filePath(const Paths &paths);
bool validFileName(const QString &name, bool channel);
bool decode(const QByteArray &bytes, Project *project, QString *error);
QByteArray encode(const Project &project);
// Missing project.json creates an empty project in the current schema.
bool load(const Paths &paths, Project *project, QString *error);
// Each operation rereads project.json under a lock and preserves other sections.
bool replaceChannels(const Paths &paths, bool video, const QList<ScheduleCore::ChannelRule> &rules,
                     const QString &renameFrom, const QString &renameTo, QString *error);
bool replaceAdverts(const Paths &paths, const QList<ScheduleCore::AdvertRule> &rules, QString *error);
}
