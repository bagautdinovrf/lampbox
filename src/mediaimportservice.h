#pragma once

#include "mediadata.h"

#include <QList>
#include <QObject>
#include <QStringList>
#include <atomic>
#include <functional>
#include <memory>

class QThread;

// Value-only work description: workers never access managers or Qt item models.
struct MediaImportRequest
{
    QStringList paths;
    QString targetDirectory;
    QStringList acceptedFormats;
    QStringList libraryFormats;
    QList<MediaData> initialSnapshot;
    int maximumFiles = 0;
    uint maximumDurationSeconds = 0;
};

struct MediaImportResult
{
    QList<MediaData> snapshot;
    QStringList errors;
    int imported = 0;
    bool cancelled = false;
};
Q_DECLARE_METATYPE(MediaImportResult)

class MediaImportService final : public QObject
{
    Q_OBJECT
public:
    using Cancellation = std::shared_ptr<std::atomic_bool>;
    using Progress = std::function<void(int completed, int total, const QString &file,
                                        qint64 copiedBytes, qint64 totalBytes)>;
    explicit MediaImportService(QObject *parent = nullptr);
    ~MediaImportService() override;
    bool start(MediaImportRequest request);
    bool isRunning() const { return m_thread != nullptr; }
    void cancel();

    // Read-only helpers also support callers that deliberately work synchronously.
    static QList<MediaData> scanDirectory(const QString &directory, const QStringList &formats);
    static MediaImportResult run(const MediaImportRequest &request,
                                 const Cancellation &cancelled = {}, const Progress &progress = {});

signals:
    void progress(int completed, int total, QString file, qint64 copiedBytes, qint64 totalBytes);
    void finished(const MediaImportResult &result);

private:
    QThread *m_thread = nullptr;
    Cancellation m_cancelled;
};
