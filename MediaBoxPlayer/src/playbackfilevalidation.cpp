#include "playbackfilevalidation.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace MediaBox {
namespace {
QString pathShapeError(const QString &path)
{
    if (path.isEmpty() || path.size() > 4096 || path.contains(QChar::Null)
        || !QDir::isAbsolutePath(path))
        return QStringLiteral("Each path must be an absolute local path of at most 4096 characters.");
    return {};
}

} // namespace

QString playbackFileError(const QString &path)
{
    const QString shapeError = pathShapeError(path);
    if (!shapeError.isEmpty())
        return shapeError;
    const QFileInfo info(path);
    if (!info.exists() || !info.isFile() || !info.isReadable())
        return QStringLiteral("Media file does not exist or is not a readable regular file: %1").arg(path);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return QStringLiteral("Cannot read media file: %1").arg(path);
    return {};
}

} // namespace MediaBox
