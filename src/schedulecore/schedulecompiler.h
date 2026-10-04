#pragma once

#include <QJsonObject>
#include <QString>

// Compiles editor channel rules into the executable schedule contract.
// This input stays inside Manager; players read the published snapshot.
namespace ScheduleCompiler {
bool fromChannels(const QJsonObject &channels, const QString &contentRoot,
                  const QJsonObject &previous, QJsonObject *document, QString *error,
                  const QString &mediaType = QStringLiteral("audio"));
}
