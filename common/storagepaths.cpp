#include "storagepaths.h"

#include <QDir>
#include <QStandardPaths>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <ShlObj.h>
#endif

namespace MediaBox::StoragePaths {
namespace {
QString rootDirectory(bool configuration)
{
    // Qt's test paths include the test application/organization. Never touch
    // machine-wide ProgramData or another application's settings in tests.
    if (QStandardPaths::isTestModeEnabled()) {
        const auto location = configuration ? QStandardPaths::AppConfigLocation
                                            : QStandardPaths::AppLocalDataLocation;
        return QDir(QStandardPaths::writableLocation(location)).filePath(QStringLiteral("mediabox"));
    }
#if defined(Q_OS_ANDROID)
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
            .filePath(QStringLiteral("mediabox"));
#elif defined(Q_OS_WIN)
    QString base = QDir::fromNativeSeparators(qEnvironmentVariable("ProgramData"));
    if (base.isEmpty() || !QDir::isAbsolutePath(base)) {
        wchar_t path[MAX_PATH] = {};
        const HRESULT result = SHGetFolderPathW(nullptr, CSIDL_COMMON_APPDATA, nullptr,
                                               SHGFP_TYPE_CURRENT, path);
        if (FAILED(result))
            qFatal("Cannot determine the Windows ProgramData directory (0x%08lx).",
                   static_cast<unsigned long>(result));
        base = QDir::fromNativeSeparators(QString::fromWCharArray(path));
    }
    return QDir(QDir::cleanPath(base)).filePath(QStringLiteral("MediaBox"));
#elif defined(Q_OS_LINUX)
    return QStringLiteral("/etc/mediabox");
#else
    const auto location = configuration ? QStandardPaths::GenericConfigLocation
                                        : QStandardPaths::GenericDataLocation;
    return QDir(QStandardPaths::writableLocation(location)).filePath(QStringLiteral("mediabox"));
#endif
}

QString applicationName(Application application)
{
#ifdef Q_OS_WIN
    switch (application) {
    case Application::Manager: return QStringLiteral("MediaBoxManager");
    case Application::Player: return QStringLiteral("MediaBoxPlayer");
    case Application::VideoPlayer: return QStringLiteral("MediaBoxVPlayer");
    }
#else
    switch (application) {
    case Application::Manager: return QStringLiteral("mediaboxmanager");
    case Application::Player: return QStringLiteral("mediaboxplayer");
    case Application::VideoPlayer: return QStringLiteral("mediaboxvplayer");
    }
#endif
    Q_UNREACHABLE();
}
} // namespace

QString commonConfigurationDirectory()
{
    return rootDirectory(true);
}

QString commonDataDirectory()
{
    return rootDirectory(false);
}

QString configurationDirectory(Application application)
{
    return QDir(commonConfigurationDirectory()).filePath(applicationName(application));
}

QString dataDirectory(Application application)
{
    return QDir(rootDirectory(false)).filePath(applicationName(application));
}

} // namespace MediaBox::StoragePaths
