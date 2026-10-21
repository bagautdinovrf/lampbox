#pragma once

#include "schedulepreview.h"
#include "schedulecore/schedulev1.h"
#include <QPair>
#include <memory>

// Daily grid of the compiled document, linked to editable channels by playlist ID.
class ScheduleDocumentPreview final : public QWidget
{
    Q_OBJECT
public:
    explicit ScheduleDocumentPreview(QWidget *parent = nullptr);
    ~ScheduleDocumentPreview() override;
    void setPlan(const ScheduleV1::Document &document, const QDateTime &wallTime);
    void setChannelPlaylists(const QList<QPair<QString, QString>> &playlists);
    void selectPlaylist(const QString &playlistId);
    bool hasSelection() const;
    const SchedulePreview::Snapshot &snapshot() const;
signals:
    void playlistSelected(const QString &playlistId);
    void playlistEditRequested(const QString &playlistId);
    // A click on the hour scale above the lanes; local wall time of the shown day.
    void timeRequested(const QTime &time);
protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    void adjustTimelineHeight();
    struct Private;
    std::unique_ptr<Private> d;
};
