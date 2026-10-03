#pragma once

#include <QString>
#include <QStringList>

namespace MediaBox::StoragePaths {

enum class Application { Manager, Player };

// Path queries have no filesystem side effects. Both applications use the same
// roots; Android roots belong to the current APK's private sandbox.
QString commonConfigurationDirectory();
QString commonDataDirectory();
QString configurationDirectory(Application application);
QString dataDirectory(Application application);

// Copy the first existing source only when the destination is absent. Keep the
// source, preserve its permissions and never overwrite a newer destination.
bool migrateFile(const QString &destination, const QStringList &sources, QString *error = nullptr);

} // namespace MediaBox::StoragePaths
