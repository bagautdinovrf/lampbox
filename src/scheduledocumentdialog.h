#pragma once

#include <QDialog>
#include <QJsonObject>
#include <QDateTime>
#include <memory>

namespace ScheduleV1 { struct Document; }

// Expert editor for the complete, versioned schedule. Saving a draft and
// publishing it are separate caller-owned operations.
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
