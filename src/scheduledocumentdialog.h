#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QDateTime>
#include <memory>

namespace ScheduleV1 { struct Document; }

// Settings for the single schedule: common additions are available as forms,
// and the complete document editor opens on demand. Publication is caller-owned.
class ScheduledDocumentDialog final : public QDialog
{
public:
    explicit ScheduledDocumentDialog(const QJsonObject &document, QWidget *parent = nullptr,
                                     const QString &mediaType = QStringLiteral("audio"));
    ~ScheduledDocumentDialog() override;
    QJsonObject document() const;
    void accept() override;
private:
    struct Private;
    std::unique_ptr<Private> d;
};

namespace ScheduleDocumentUi {
// Uses the document's own time zone; contains calculated plan, not telemetry.
QString describe(const QJsonObject &document, const QDateTime &at);
QString describe(const ScheduleV1::Document &document, const QDateTime &at);
}
