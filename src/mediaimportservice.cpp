#include "mediaimportservice.h"

#include "audioproperties.h"
#include "fileref.h"
#include "tag.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTemporaryFile>
#include <QThread>
#include <algorithm>

namespace {
bool cancelled(const MediaImportService::Cancellation &flag)
{
    return flag && flag->load(std::memory_order_relaxed);
}

bool accepted(const QString &name, const QStringList &formats)
{
    return formats.isEmpty() || QDir::match(formats, name);
}

MediaData readMedia(const QString &path)
{
    const QFileInfo info(path);
    MediaData media;
    media.setFileName(info.fileName());
    media.setFileSize(info.size());
    media.setLength(0);
    media.setYear(0);
#ifdef Q_OS_WIN
    const auto native = path.toStdWString();
#else
    const auto native = QFile::encodeName(path);
#endif
    TagLib::FileRef file(native.data());
    if (file.isNull())
        return media;
    if (const auto *tag = file.tag()) {
        media.setAlbum(QString::fromStdWString(tag->album().toWString()));
        media.setArtist(QString::fromStdWString(tag->artist().toWString()));
        media.setTitle(QString::fromStdWString(tag->title().toWString()));
        media.setGenre(QString::fromStdWString(tag->genre().toWString()));
        media.setYear(tag->year());
    }
    if (const auto *properties = file.audioProperties())
        media.setLength(qMax(0, properties->lengthInSeconds()));
    return media;
}

QList<MediaData> scan(const QString &directory, const QStringList &formats,
                     const MediaImportService::Cancellation &flag, bool *complete,
                     const MediaImportService::Progress &progress = {})
{
    QList<MediaData> snapshot;
    const auto entries = QDir(directory).entryInfoList(QDir::Files, QDir::Name);
    for (const QFileInfo &entry : entries) {
        if (cancelled(flag)) {
            *complete = false;
            return {};
        }
        if (!accepted(entry.fileName(), formats))
            continue;
        if (progress)
            progress(0, 0, entry.fileName(), 0, 0);
        snapshot.append(readMedia(entry.absoluteFilePath()));
    }
    *complete = !cancelled(flag);
    return snapshot;
}
}

MediaImportService::MediaImportService(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<MediaImportResult>();
}

MediaImportService::~MediaImportService()
{
    cancel();
    // The worker owns only copied values. Waiting here also makes direct
    // destruction safe; the normal window-close path cancels asynchronously.
    if (m_thread)
        m_thread->wait();
}

bool MediaImportService::start(MediaImportRequest request)
{
    if (m_thread)
        return false;
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    const auto flag = m_cancelled;
    const auto result = std::make_shared<MediaImportResult>();
    m_thread = QThread::create([this, request = std::move(request), flag, result] {
        *result = run(request, flag, [this](int completed, int total, const QString &file,
                                          qint64 copied, qint64 bytes) {
            emit progress(completed, total, file, copied, bytes);
        });
    });
    m_thread->setParent(this);
    connect(m_thread, &QThread::finished, this, [this, result] {
        auto *thread = m_thread;
        m_thread = nullptr;
        thread->deleteLater();
        emit finished(*result);
    });
    m_thread->start();
    return true;
}

void MediaImportService::cancel()
{
    if (m_cancelled)
        m_cancelled->store(true, std::memory_order_relaxed);
}

QList<MediaData> MediaImportService::scanDirectory(const QString &directory, const QStringList &formats)
{
    bool complete = false;
    return scan(directory, formats, {}, &complete);
}

MediaImportResult MediaImportService::run(const MediaImportRequest &request,
                                        const Cancellation &flag, const Progress &progress)
{
    MediaImportResult result;
    result.snapshot = request.initialSnapshot;
    const QDir target(request.targetDirectory);
    if (request.targetDirectory.isEmpty()) {
        result.errors << tr("Не указан каталог медиатеки.");
        return result;
    }
    const QFileInfo targetInfo(target.absolutePath());
    if (targetInfo.exists() && (!targetInfo.isDir() || !targetInfo.isReadable())) {
        // A failed listing is not an empty library. Keep the last complete
        // snapshot and leave any obstructing file untouched.
        result.errors << tr("Каталог медиатеки недоступен: %1").arg(target.absolutePath());
        return result;
    }
    bool complete = false;
    auto snapshot = scan(target.absolutePath(), request.libraryFormats, flag, &complete, progress);
    if (!complete) {
        result.cancelled = true;
        return result;
    }
    result.snapshot = std::move(snapshot);

    QStringList files;
    QSet<QString> seen;
    const QString targetPath = QDir::cleanPath(target.absolutePath());
    const auto append = [&](const QString &path) {
        const QFileInfo info(path);
        const QString identity = info.canonicalFilePath();
        if (!identity.isEmpty() && !seen.contains(identity)) {
            seen.insert(identity);
            files.append(info.absoluteFilePath());
        }
    };
    for (const QString &path : request.paths) {
        if (cancelled(flag))
            break;
        const QFileInfo info(path);
        if (info.isFile()) {
            append(path);
        } else if (info.isDir()) {
            // Do not follow directory symlinks or descend into the destination.
            QStringList pending{info.absoluteFilePath()};
            while (!pending.isEmpty() && !cancelled(flag)) {
                const QString current = pending.takeLast();
                if (QDir::cleanPath(current) == targetPath)
                    continue;
                QDirIterator iterator(current, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
                while (iterator.hasNext() && !cancelled(flag)) {
                    iterator.next();
                    if (iterator.fileInfo().isDir())
                        pending.append(iterator.filePath());
                    else if (accepted(iterator.fileName(), request.acceptedFormats))
                        append(iterator.filePath());
                }
                if (progress)
                    progress(0, 0, current, 0, 0);
            }
        } else {
            result.errors << tr("Файл не найден: %1").arg(path);
        }
    }
    for (int i = 0; i < files.size() && !cancelled(flag); ++i) {
        const QString sourcePath = files.at(i);
        const QFileInfo sourceInfo(sourcePath);
        QString name = sourceInfo.fileName();
        name.remove(QLatin1Char('{')).remove(QLatin1Char('}'));
        if (progress)
            progress(i, files.size(), name, 0, sourceInfo.size());
        if (cancelled(flag))
            break;
        if (!accepted(name, request.acceptedFormats)) {
            result.errors << tr("Неподдерживаемый формат: %1").arg(name);
            continue;
        }
        if (request.maximumFiles > 0 && result.snapshot.size() >= request.maximumFiles) {
            result.errors << tr("Можно добавить не больше %1 файлов.").arg(request.maximumFiles);
            break;
        }
        const QString destination = target.filePath(name);
        if (QFileInfo::exists(destination) || QFileInfo(destination).isSymLink()) {
            result.errors << tr("Файл уже существует: %1").arg(name);
            continue;
        }
        MediaData media = readMedia(sourcePath);
        if (request.maximumDurationSeconds && media.length() > request.maximumDurationSeconds) {
            result.errors << tr("Длительность файла превышает %1 секунд: %2")
                                 .arg(request.maximumDurationSeconds).arg(name);
            continue;
        }
        if (cancelled(flag))
            break;
        if (!QDir().mkpath(target.absolutePath())) {
            result.errors << tr("Не удалось создать каталог медиатеки: %1").arg(target.absolutePath());
            break;
        }
        QFile source(sourcePath);
        QTemporaryFile temporary(target.filePath(QStringLiteral(".mediabox-import-XXXXXX")));
        if (!source.open(QIODevice::ReadOnly) || !temporary.open()) {
            result.errors << tr("Не удалось открыть файл для копирования: %1").arg(name);
            continue;
        }
        bool copied = true;
        qint64 bytes = 0;
        QElapsedTimer update;
        update.start();
        while (!source.atEnd() && !cancelled(flag)) {
            const QByteArray block = source.read(256 * 1024);
            if (block.isEmpty() || temporary.write(block) != block.size()) {
                copied = false;
                break;
            }
            bytes += block.size();
            if (progress && (bytes == block.size() || update.elapsed() >= 50)) {
                progress(i, files.size(), name, bytes, sourceInfo.size());
                update.restart();
            }
        }
        if (cancelled(flag))
            break; // QTemporaryFile removes only this unfinished new file.
        if (!copied || !temporary.flush()) {
            result.errors << tr("Ошибка копирования: %1").arg(name);
            continue;
        }
        temporary.close();
        // QFile::rename refuses an existing destination, including a file that
        // appeared since the initial collision check. Never replace user data.
        if (!temporary.rename(destination)) {
            result.errors << tr("Не удалось добавить файл (возможно, имя уже занято): %1").arg(name);
            continue;
        }
        temporary.setAutoRemove(false);
        media.setFileName(name);
        media.setFileSize(bytes);
        result.snapshot.append(media);
        ++result.imported;
        if (progress)
            progress(i + 1, files.size(), name, bytes, bytes);
    }
    result.cancelled = cancelled(flag);
    std::sort(result.snapshot.begin(), result.snapshot.end(), [](const MediaData &a, const MediaData &b) {
        return QString::localeAwareCompare(a.fileName(), b.fileName()) < 0;
    });
    return result;
}
