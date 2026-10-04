#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

// Manager-owned editable document and immutable publication files. The JSON
// project is a single-writer implementation of the v1 storage contract.
namespace SchedulePublication {
struct Publication {
    QJsonObject active;
    QByteArray bytes;
    QString activePath;
    QString contentRoot;
};

// Channel input belongs to the editor/compiler only, including media paths.
// In channel mode it is compiled afresh; an explicitly edited project uses its
// saved document. Opening/validating never marks a publication as accepted.
bool draft(const QString &directory, const QString &contentRoot, const QJsonObject &channels,
           QJsonObject *document, bool *advanced, QString *error,
           const QString &mediaType = QStringLiteral("audio"));
bool saveDraft(const QString &directory, const QJsonObject &document, QString *error);
bool publish(const QString &directory, const QString &contentRoot, const QJsonObject &document,
             Publication *publication, QString *error);
QString projectPath(const QString &directory);
}
