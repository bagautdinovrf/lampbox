#pragma once

#include <QString>

namespace MediaBox::StoragePaths {

enum class Application { Manager, Player, VideoPlayer };

// Path queries have no filesystem side effects. All applications use the same
// roots; Android roots belong to the current APK's private sandbox.
QString commonConfigurationDirectory();
QString commonDataDirectory();
QString configurationDirectory(Application application);
QString dataDirectory(Application application);

} // namespace MediaBox::StoragePaths
