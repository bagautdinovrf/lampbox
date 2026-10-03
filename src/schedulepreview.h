#ifndef SCHEDULEPREVIEW_H
#define SCHEDULEPREVIEW_H

#include <QDateTime>
#include <QList>
#include <QStringList>
#include <QWidget>

class QAbstractItemModel;

// A read-only interpretation of the existing ChannelModel / AdvertModel
// columns. These results describe a plan, never player telemetry.
namespace SchedulePreview {
struct Channel {
    int sourceRow = -1;
    QString name, weekdays, days, months, reason, status;
    QTime start, end;
    int volume = 0;
    bool valid = false;
    bool calendarMatches = false;
    bool active = false;
};

struct Snapshot {
    QDateTime at;
    int horizonDays = 0;
    QList<Channel> channels;
    QList<int> activeRows;
    QStringList issues;
    QDateTime nextChannelTime, nextAdvertTime;
    QStringList nextChannelNames, nextAdvertNames;
    QStringList exactAdvertsNow, frequencyAdvertsNow;
    QString currentSummary, nextChannelSummary, nextAdvertSummary;
    bool hasUnresolvedRules = false;
};

// Searches at most 366 calendar days, including the preview date. Invalid,
// overnight and equal-time windows are explained, not assigned invented meaning.
Snapshot evaluate(const QAbstractItemModel *channels, const QAbstractItemModel *adverts,
                  const QDateTime &at, int horizonDays = 366);
}

class SchedulePreviewWidget : public QWidget
{
    Q_OBJECT
public:
    explicit SchedulePreviewWidget(QWidget *parent = nullptr);
    ~SchedulePreviewWidget() override;
    void setModels(QAbstractItemModel *channels, QAbstractItemModel *adverts = nullptr);
    void setSelectedRow(int row);
    int selectedRow() const;
    void setPreviewDateTime(const QDateTime &dateTime);
    QDateTime previewDateTime() const;
    const SchedulePreview::Snapshot &snapshot() const;
    QSize sizeHint() const override;

public slots:
    void refresh();
    void showConditions();

signals:
    void selectedRowChanged(int row);
    void editRequested(int row);
    void snapshotChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Private;
    Private *d;
};

#endif
