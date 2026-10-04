#include "scheduledocumentpreview.h"
#include "restyletheme.h"

#include <QAbstractTableModel>
#include <QFileInfo>
#include <QHeaderView>
#include <QHelpEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QTableView>
#include <QTimeZone>
#include <QToolTip>
#include <QVBoxLayout>
#include <algorithm>
#include <functional>
#include <optional>

namespace {
QString name(const QJsonObject &document, const QString &section, const QString &id)
{
    for (const auto &value : document.value(section).toArray()) {
        const auto item = value.toObject();
        if (item.value("id").toString() == id)
            return item.value("name").toString(QFileInfo(item.value("path").toString()).fileName());
    }
    return id;
}

QString sourceName(const QJsonObject &document, const ScheduleV1::Evaluation &plan)
{
    const QString base = plan.silence ? QStringLiteral("Тишина") : name(document, "playlists", plan.playlistId);
    if (plan.mixRuleId.isEmpty() || plan.silence) return base;
    QStringList sources;
    for (const auto &value : plan.pattern) {
        const auto source = value.toObject();
        sources << (source.value("type") == QJsonValue("active_base")
                    ? base : name(document, "playlists", source.value("playlistId").toString()));
    }
    return sources.join(QStringLiteral(" → "));
}

struct Row {
    QDateTime from, until;
    QString time, source, mode, rule, tooltip;
    QString playlistId, eventRuleId, eventAssetId;
    QStringList sourcePlaylistIds;
    QList<QPair<QDateTime, QDateTime>> conflicts;
    int volume = 0, priority = 0;
    bool event = false, active = false, fallback = false, silence = false, mixed = false, oneToOne = false;
    bool diagnosticMix = false;
};

class GridModel final : public QAbstractTableModel
{
public:
    explicit GridModel(QObject *parent) : QAbstractTableModel(parent) {}
    QList<Row> rows;
    void replace(QList<Row> value) { beginResetModel(); rows = std::move(value); endResetModel(); }
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : int(rows.size()); }
    int columnCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : 5; }
    QVariant headerData(int column, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
        return QStringList{QStringLiteral("Время"), QStringLiteral("Плейлист / вставка"),
                           QStringLiteral("Режим"), QStringLiteral("Звук"), QStringLiteral("Правило")}.value(column);
    }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() >= rows.size()) return {};
        const auto &row = rows.at(index.row());
        if (role == Qt::UserRole) return row.playlistId;
        if (role == Qt::UserRole + 1) return row.sourcePlaylistIds;
        if (role == Qt::ToolTipRole || role == Qt::AccessibleDescriptionRole) return row.tooltip;
        if (role == Qt::BackgroundRole && row.active) return Restyle::tokens().successBg;
        if (role == Qt::ForegroundRole && row.event) return Restyle::tokens().accentText;
        if (role == Qt::FontRole && row.active) return Restyle::font(11, QFont::DemiBold);
        if (role != Qt::DisplayRole) return {};
        switch (index.column()) {
        case 0: return row.time;
        case 1: return row.source;
        case 2: return row.mode;
        case 3: return row.volume < 0 ? QStringLiteral("—") : QStringLiteral("%1%").arg(row.volume);
        case 4: return row.rule;
        }
        return {};
    }
};

class DayTimeline final : public QWidget
{
public:
    struct Lane {
        QString playlistId, title;
        QList<int> rows;
        bool events = false, service = false;
    };
    DayTimeline(GridModel *model, QWidget *parent) : QWidget(parent), mModel(model)
    {
        setObjectName(QStringLiteral("scheduleDocumentTimeline"));
        setAccessibleName(QStringLiteral("Сетка вещания: отдельная временная дорожка каждого канала"));
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    QDateTime from, until, at;
    int selected = -1;
    QString selectedPlaylist;
    bool planValid = false;
    std::function<void(int, const QString &)> rowSelected, rowEditRequested;

    void setChannels(const QList<QPair<QString, QString>> &channels)
    {
        mChannels = channels;
        rebuildLanes();
    }
    void setDocument(const QJsonObject &document)
    {
        mDocument = document;
        rebuildLanes();
    }
    bool hasPlaylist(const QString &id) const
    {
        if (id.isEmpty()) return false;
        for (const auto &lane : mLanes)
            if (lane.playlistId == id) return true;
        return false;
    }
    void rebuildLanes()
    {
        mLanes.clear();
        auto addPlaylist = [this](const QString &id, const QString &title) {
            if (id.isEmpty()) return;
            for (const auto &lane : mLanes)
                if (lane.playlistId == id) return;
            mLanes << Lane{id, title.isEmpty() ? id : title, {}};
        };
        for (const auto &channel : mChannels) addPlaylist(channel.first, channel.second);
        for (const auto &row : mModel->rows) {
            if (row.diagnosticMix) continue;
            for (const auto &id : row.sourcePlaylistIds) addPlaylist(id, name(mDocument, "playlists", id));
        }
        Lane service{{}, QStringLiteral("Резерв / тишина"), {}, false, true};
        Lane events{{}, QStringLiteral("Вставки"), {}, true};
        for (int i = 0; i < mModel->rows.size(); ++i) {
            const auto &row = mModel->rows.at(i);
            if (row.event) { events.rows << i; continue; }
            // A raw mix rule has no resolved base source. Its own lane avoids
            // extending a channel beyond that channel's actual window.
            if (row.diagnosticMix) { mLanes << Lane{{}, row.rule, {i}}; continue; }
            for (auto &lane : mLanes)
                if (row.sourcePlaylistIds.contains(lane.playlistId)) lane.rows << i;
            if (row.silence || row.fallback) service.rows << i;
        }
        if (!service.rows.isEmpty()) mLanes << service;
        if (!events.rows.isEmpty()) mLanes << events;
        if (mLanes.isEmpty()) mLanes << Lane{{}, QStringLiteral("Расписание"), {}};
        QStringList titles, ids;
        for (const auto &lane : mLanes) { titles << lane.title; ids << lane.playlistId; }
        setProperty("laneCount", int(mLanes.size()));
        setProperty("laneNames", titles);
        setProperty("lanePlaylistIds", ids);
        int conflictingIntervals = 0;
        for (const auto &row : mModel->rows)
            if (!row.conflicts.isEmpty()) ++conflictingIntervals;
        setProperty("conflictingIntervalCount", conflictingIntervals);
        setFixedHeight(top + int(mLanes.size()) * laneHeight + 6);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto &t = Restyle::tokens();
        const bool hasTimes = from.isValid() && until.isValid();
        p.setFont(Restyle::font(9));
        for (int hour : {0, 6, 12, 18, 24}) {
            const auto tick = hour == 24 ? until : QDateTime(from.date(), QTime(hour, 0), from.timeZone());
            if (hasTimes && !tick.isValid()) continue;
            const qreal x = hasTimes ? position(tick) : trackLeft() + trackWidth() * hour / 24.0;
            const QString label = QStringLiteral("%1:00").arg(hour, 2, 10, QLatin1Char('0'));
            const int w = p.fontMetrics().horizontalAdvance(label);
            p.setPen(t.muted);
            p.drawText(QPointF(x - (hour == 0 ? 0 : hour == 24 ? w : w / 2), 16), label);
        }
        for (int laneIndex = 0; laneIndex < mLanes.size(); ++laneIndex) {
            const auto &lane = mLanes.at(laneIndex);
            const bool laneSelected = !lane.playlistId.isEmpty() && lane.playlistId == selectedPlaylist;
            const QRectF titleRect(0, top + laneIndex * laneHeight, trackLeft() - 12, trackHeight);
            const QRectF track(trackLeft(), titleRect.top(), trackWidth(), trackHeight);
            p.setFont(Restyle::font(10, laneSelected ? QFont::DemiBold : QFont::Normal));
            p.setPen(laneSelected ? t.accentText : t.secondary);
            p.drawText(titleRect, Qt::AlignVCenter,
                       p.fontMetrics().elidedText(lane.title, Qt::ElideRight, int(titleRect.width())));
            p.setPen(Qt::NoPen);
            p.setBrush(t.field);
            p.drawRoundedRect(track, t.trackRadius, t.trackRadius);
            if (t.relief) {
                p.setPen(QColor(0, 0, 0, t.dark ? 65 : 13));
                p.drawLine(track.topLeft() + QPointF(4, 1), track.topRight() + QPointF(-4, 1));
            }
            for (int hour : {6, 12, 18}) {
                const auto tick = QDateTime(from.date(), QTime(hour, 0), from.timeZone());
                const qreal x = hasTimes ? position(tick) : trackLeft() + trackWidth() * hour / 24.0;
                p.setPen(t.line);
                p.drawLine(QPointF(x, track.top()), QPointF(x, track.bottom()));
            }
            if (lane.rows.isEmpty()) {
                p.setPen(t.muted);
                p.setFont(Restyle::font(9));
                p.drawText(track.adjusted(8, 0, -8, 0), Qt::AlignVCenter,
                           planValid ? QStringLiteral("Нет выхода") : QStringLiteral("Не рассчитано"));
            }
            for (const int rowIndex : lane.rows) {
                const auto &row = mModel->rows.at(rowIndex);
                const auto r = rowRect(row, laneIndex);
                const bool intervalSelected = rowIndex == selected && (laneSelected || selectedPlaylist.isEmpty());
                if (row.event) {
                    const qreal x = r.center().x(), y = r.center().y();
                    p.setPen(QPen(t.accentText, 1));
                    p.setBrush(intervalSelected ? t.accentSoft : t.surface);
                    p.drawPolygon(QPolygonF{QPointF(x, y - 7), QPointF(x + 5, y), QPointF(x, y + 7), QPointF(x - 5, y)});
                    continue;
                }
                // Match the original channel grid: raised, pale intervals and
                // a quiet selection tint. Plan activity belongs in the status,
                // rather than turning the selected channel into a green block.
                const bool highlighted = laneSelected || intervalSelected;
                const QRectF interval = r.adjusted(0.5, 0.5, -0.5, -0.5);
                const QColor fill = highlighted ? t.accentSoft : t.surface2;
                if (t.relief) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(0, 0, 0, t.dark ? 90 : 20));
                    p.drawRoundedRect(interval.translated(1, 1), t.intervalRadius, t.intervalRadius);
                    QLinearGradient gradient(interval.topLeft(), interval.bottomRight());
                    gradient.setColorAt(0, t.dark ? t.buttonTop : t.surface);
                    gradient.setColorAt(1, fill);
                    p.setBrush(gradient);
                } else {
                    p.setBrush(fill);
                }
                p.setPen(highlighted ? t.accentLine : t.line);
                p.drawRoundedRect(interval, t.intervalRadius, t.intervalRadius);
            }
            // Paint conflict marks after all bars, including when several
            // rules share a playlist and therefore the same lane.
            for (const int rowIndex : lane.rows) {
                const auto &row = mModel->rows.at(rowIndex);
                if (row.event) continue;
                const auto r = rowRect(row, laneIndex);
                // Mark the actual intersection on every participating lane.
                // Hatching keeps the conflict recognisable without relying on colour.
                for (const auto &conflict : row.conflicts) {
                    const QRectF overlap(position(conflict.first), r.top() + 1,
                                         qMax(qreal(1), position(conflict.second) - position(conflict.first)), r.height() - 2);
                    p.save();
                    p.setClipRect(overlap);
                    p.fillRect(overlap, t.errorBg);
                    QColor hatch = t.error;
                    hatch.setAlpha(65);
                    p.setPen(QPen(hatch, 1));
                    for (qreal x = overlap.left() - overlap.height(); x < overlap.right(); x += 8)
                        p.drawLine(QPointF(x, overlap.bottom()), QPointF(x + overlap.height(), overlap.top()));
                    p.restore();
                    p.setBrush(Qt::NoBrush);
                    p.setPen(QPen(t.error, 1.5));
                    p.drawRect(overlap.adjusted(0.5, 0.5, -0.5, -0.5));
                }
                if (r.width() > 28) {
                    p.setPen(!row.conflicts.isEmpty() ? t.error : laneSelected ? t.accentText : t.secondary);
                    p.setFont(Restyle::font(9));
                    QString label = lane.service ? row.source : row.mixed
                            ? (row.oneToOne ? QStringLiteral("1:1 · ") : QStringLiteral("Чередование · ")) + row.time : row.time;
                    if (!row.conflicts.isEmpty()) label.prepend(QStringLiteral("! "));
                    p.drawText(r.adjusted(6, 0, -6, 0), Qt::AlignCenter,
                               p.fontMetrics().elidedText(label, Qt::ElideRight, qMax(0, int(r.width()) - 12)));
                }
            }
            if (hasFocus() && laneSelected) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(t.focus, 1, Qt::DotLine));
                p.drawRoundedRect(QRectF(1, titleRect.top() - 1, width() - 2, laneHeight - 1), 3, 3);
            }
        }
        if (hasTimes && at >= from && at < until) {
            const qreal x = position(at);
            p.setPen(QPen(t.error, 1));
            p.drawLine(QPointF(x, top - 2), QPointF(x, height() - 6));
            p.fillRect(QRectF(x - 2, top - 2, 4, 4), t.error);
        }
    }
    void mousePressEvent(QMouseEvent *event) override
    {
        const int lane = laneAt(event->position());
        if (event->button() == Qt::LeftButton && lane >= 0 && rowSelected) {
            const QString playlistId = mLanes.at(lane).playlistId;
            rowSelected(rowAt(event->position(), lane), playlistId);
            event->accept();
            return;
        }
        QWidget::mousePressEvent(event);
    }
    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        const int lane = laneAt(event->position());
        if (event->button() == Qt::LeftButton && lane >= 0 && rowEditRequested) {
            const QString playlistId = mLanes.at(lane).playlistId;
            rowEditRequested(rowAt(event->position(), lane), playlistId);
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }
    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::ToolTip) {
            const auto *help = static_cast<QHelpEvent *>(event);
            QStringList hits;
            const int lane = laneAt(help->pos());
            if (lane >= 0) {
                const auto &track = mLanes.at(lane);
                hits << track.title;
                for (const auto rowIndex : track.rows) {
                    const auto &row = mModel->rows.at(rowIndex);
                    if (rowRect(row, lane).contains(help->pos())) hits << row.tooltip;
                }
            }
            if (hits.isEmpty()) QToolTip::hideText();
            else QToolTip::showText(help->globalPos(), hits.join(QStringLiteral("\n\n")), this);
            return true;
        }
        return QWidget::event(event);
    }

private:
    int laneAt(const QPointF &point) const
    {
        if (point.y() < top) return -1;
        const int lane = int(point.y() - top) / laneHeight;
        return lane < mLanes.size() ? lane : -1;
    }
    int rowAt(const QPointF &point, int lane) const
    {
        const auto &rows = mLanes.at(lane).rows;
        for (auto i = rows.crbegin(); i != rows.crend(); ++i)
            if (rowRect(mModel->rows.at(*i), lane).contains(point)) return *i;
        return -1;
    }
    qreal trackLeft() const { return qMin(156, qMax(90, width() / 4)); }
    qreal trackWidth() const { return qMax(qreal(1), width() - trackLeft() - 6); }
    qreal position(const QDateTime &time) const
    {
        const auto duration = from.msecsTo(until);
        return trackLeft() + trackWidth() * qreal(from.msecsTo(time)) / qMax(qint64(1), duration);
    }
    QRectF rowRect(const Row &row, int lane) const
    {
        const qreal x = position(row.from);
        const qreal y = top + lane * laneHeight;
        return row.event ? QRectF(x - 6, y + 1, 12, trackHeight - 2)
                         : QRectF(x, y + 1, qMax(qreal(1), position(row.until) - x), trackHeight - 2);
    }
    static constexpr int top = 26, laneHeight = 29, trackHeight = 23;
    GridModel *mModel;
    QJsonObject mDocument;
    QList<QPair<QString, QString>> mChannels;
    QList<Lane> mLanes;
};
}

struct ScheduleDocumentPreview::Private {
    SchedulePreview::Snapshot snapshot;
    GridModel *model;
    DayTimeline *timeline;
    QTableView *table;
    QScrollArea *timelineScroll;
};

ScheduleDocumentPreview::ScheduleDocumentPreview(QWidget *parent) : QWidget(parent), d(std::make_unique<Private>())
{
    setObjectName(QStringLiteral("scheduleDocumentGrid"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    d->model = new GridModel(this);
    d->timeline = new DayTimeline(d->model, this);
    d->timeline->installEventFilter(this);
    auto *timelineScroll = d->timelineScroll = new QScrollArea(this);
    timelineScroll->setObjectName(QStringLiteral("scheduleDocumentTimelineScroll"));
    timelineScroll->setFrameShape(QFrame::NoFrame);
    timelineScroll->setWidgetResizable(true);
    timelineScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timelineScroll->setMinimumHeight(92);
    timelineScroll->setMaximumHeight(184);
    timelineScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    timelineScroll->setWidget(d->timeline);
    d->timeline->setAutoFillBackground(false);
    timelineScroll->viewport()->setAutoFillBackground(false);
    d->timeline->rebuildLanes();
    layout->addWidget(timelineScroll);
    d->table = new QTableView(this);
    d->table->setObjectName(QStringLiteral("scheduleDocumentTable"));
    d->table->setAccessibleName(QStringLiteral("Вставки и чередование: время и условия запуска"));
    d->table->setModel(d->model);
    d->table->setFrameShape(QFrame::NoFrame);
    d->table->setShowGrid(false);
    d->table->setWordWrap(false);
    d->table->setFont(Restyle::font(11));
    d->table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    d->table->installEventFilter(this);
    d->table->setSelectionBehavior(QAbstractItemView::SelectRows);
    d->table->setSelectionMode(QAbstractItemView::SingleSelection);
    d->table->verticalHeader()->hide();
    d->table->verticalHeader()->setDefaultSectionSize(28);
    d->table->horizontalHeader()->setFont(Restyle::font(10));
    d->table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d->table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    d->table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    d->table->horizontalHeader()->setStretchLastSection(false);
    d->table->setColumnWidth(0, 160);
    d->table->setColumnWidth(2, 190);
    d->table->setColumnWidth(3, 58);
    d->table->setColumnWidth(4, 175);
    d->table->setMinimumHeight(58);
    d->table->hide();
    layout->addWidget(d->table, 1);
    connect(d->table->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
            [this](const QModelIndex &index) {
        d->timeline->selected = index.row();
        const auto playlistId = index.data(Qt::UserRole).toString();
        d->timeline->selectedPlaylist = playlistId;
        d->timeline->update();
        if (!playlistId.isEmpty()) emit playlistSelected(playlistId);
    });
    connect(d->table, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        const auto playlistId = index.data(Qt::UserRole).toString();
        if (!playlistId.isEmpty()) emit playlistEditRequested(playlistId);
    });
    d->timeline->rowSelected = [this](int row, const QString &playlistId) {
        {
            const QSignalBlocker blocker(d->table->selectionModel());
            const bool tableRowVisible = !d->table->isHidden() && row >= 0 && !d->table->isRowHidden(row);
            (tableRowVisible ? static_cast<QWidget *>(d->table) : d->timeline)->setFocus(Qt::MouseFocusReason);
            if (row >= 0) {
                d->table->selectRow(row);
                if (tableRowVisible) d->table->scrollTo(d->model->index(row, 0));
                d->timeline->selected = row;
                d->timeline->selectedPlaylist = playlistId;
            } else {
                selectPlaylist(playlistId);
            }
            d->timeline->update();
        }
        if (!playlistId.isEmpty()) emit playlistSelected(playlistId);
    };
    d->timeline->rowEditRequested = [this](int row, const QString &playlistId) {
        d->timeline->rowSelected(row, playlistId);
        if (!playlistId.isEmpty()) emit playlistEditRequested(playlistId);
    };
}

ScheduleDocumentPreview::~ScheduleDocumentPreview() = default;

void ScheduleDocumentPreview::adjustTimelineHeight()
{
    // Give channel lanes priority; the exact-event table scrolls in the space left.
    const int tableHeight = d->table->isHidden() ? 0 : d->table->minimumHeight();
    const int availableHeight = qMax(92, height() - tableHeight);
    d->timelineScroll->setFixedHeight(qBound(92,
        qMin(d->timeline->height(), availableHeight), d->table->isHidden() ? availableHeight : 184));
    static_cast<QVBoxLayout *>(layout())->setAlignment(d->timelineScroll, Qt::AlignTop);
}

void ScheduleDocumentPreview::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    adjustTimelineHeight();
}
const SchedulePreview::Snapshot &ScheduleDocumentPreview::snapshot() const { return d->snapshot; }

bool ScheduleDocumentPreview::hasSelection() const
{
    return d->table->currentIndex().isValid() || d->timeline->hasPlaylist(d->timeline->selectedPlaylist);
}

bool ScheduleDocumentPreview::eventFilter(QObject *watched, QEvent *event)
{
    if ((watched == d->table || watched == d->timeline) && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            const auto playlistId = d->timeline->selectedPlaylist;
            if (!playlistId.isEmpty()) emit playlistEditRequested(playlistId);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void ScheduleDocumentPreview::selectPlaylist(const QString &playlistId)
{
    const QSignalBlocker blocker(d->table->selectionModel());
    const auto current = d->table->currentIndex();
    d->timeline->selectedPlaylist = playlistId;
    if (!playlistId.isEmpty() && current.isValid() && current.data(Qt::UserRole + 1).toStringList().contains(playlistId)) {
        d->timeline->update();
        return;
    }
    int selected = -1;
    if (!playlistId.isEmpty()) {
        for (int i = 0; i < d->model->rows.size(); ++i) {
            const auto &row = d->model->rows.at(i);
            if (!row.sourcePlaylistIds.contains(playlistId)) continue;
            if (selected < 0) selected = i;
            if (row.active) { selected = i; break; }
        }
    }
    if (selected >= 0) {
        d->table->selectRow(selected);
        if (!d->table->isHidden() && !d->table->isRowHidden(selected))
            d->table->scrollTo(d->model->index(selected, 0));
    } else {
        d->table->selectionModel()->clear();
    }
    d->timeline->selected = selected;
    d->timeline->update();
}

void ScheduleDocumentPreview::setChannelPlaylists(const QList<QPair<QString, QString>> &playlists)
{
    d->timeline->setChannels(playlists);
}

void ScheduleDocumentPreview::setPlan(const ScheduleV1::Document &document, const QDateTime &wallTime)
{
    const QString selectedPlaylist = d->timeline->selectedPlaylist;
    std::optional<Row> selection;
    const int selectedRow = d->table->currentIndex().row();
    if (selectedRow >= 0 && selectedRow < d->model->rows.size()) selection = d->model->rows.at(selectedRow);
    const QSignalBlocker selectionBlocker(d->table->selectionModel());
    d->snapshot = {};
    d->snapshot.at = wallTime;
    d->snapshot.horizonDays = 1;
    d->model->replace({});
    d->table->hide();
    d->timeline->from = d->timeline->until = d->timeline->at = {};
    d->timeline->selected = -1;
    d->timeline->planValid = false;
    d->timeline->setDocument(document.object);
    adjustTimelineHeight();
    if (document.object.isEmpty() || !wallTime.isValid()) {
        d->snapshot.issues << tr("Проект расписания не проверен.");
        d->snapshot.hasUnresolvedRules = true;
        return;
    }
    const QTimeZone zone(document.object.value("timeZone").toString().toUtf8());
    if (!zone.isValid()) {
        d->snapshot.issues << tr("Не задан корректный часовой пояс расписания.");
        d->snapshot.hasUnresolvedRules = true;
        return;
    }
    const QDateTime at(wallTime.date(), wallTime.time(), zone, QDateTime::TransitionResolution::PreferBefore);
    const auto from = wallTime.date().startOfDay(zone);
    const auto until = wallTime.date().addDays(1).startOfDay(zone);
    if (!at.isValid() || at.date() != wallTime.date() || at.time() != wallTime.time()
            || !from.isValid() || !until.isValid()) {
        d->snapshot.issues << tr("Выбранное местное время отсутствует при переводе часов.");
        d->snapshot.hasUnresolvedRules = true;
        return;
    }
    d->snapshot.at = at;
    const bool diagnostic = !document.compiled;
    ScheduleV1::DiagnosticPreview preview;
    QList<ScheduleV1::PlanInterval> intervals;
    if (diagnostic) {
        d->snapshot.hasUnresolvedRules = true;
        preview = ScheduleV1::diagnosticPreview(document.object, from, until);
        if (!preview.error.isEmpty()) {
            d->snapshot.issues << preview.error;
            return;
        }
        d->snapshot.issues << tr("Показаны интервалы правил. Расписание требует исправления.");
        intervals = preview.intervals;
        intervals.append(preview.mixIntervals);
    } else {
        const auto current = ScheduleV1::evaluate(document, at);
        d->snapshot.currentSummary = tr("По плану: %1 · %2%").arg(sourceName(document.object, current)).arg(current.volumePercent);
        d->snapshot.issues = current.diagnostics;
        intervals = ScheduleV1::intervals(document, from, until);
    }
    const bool offsets = from.offsetFromUtc() != until.addMSecs(-1).offsetFromUtc();
    auto timeText = [&](const QDateTime &time) {
        const auto local = time.toTimeZone(zone);
        if (time == until) return QStringLiteral("24:00");
        QString text = local.toString(local.time().second() ? "HH:mm:ss" : "HH:mm");
        if (offsets) text += QStringLiteral(" ") + local.toString("ttt");
        return text;
    };
    QList<Row> rows;
    for (const auto &interval : intervals) {
        const auto &plan = interval.plan;
        Row row;
        row.from = interval.from; row.until = interval.until;
        row.time = timeText(row.from) + QStringLiteral("–") + timeText(row.until);
        row.source = sourceName(document.object, plan);
        row.playlistId = plan.playlistId;
        row.fallback = plan.usingFallback;
        row.silence = plan.silence;
        row.mixed = !plan.mixRuleId.isEmpty() && (diagnostic || !plan.silence);
        row.diagnosticMix = diagnostic && row.mixed;
        if (row.mixed) {
            row.silence = false;
            for (const auto &value : plan.pattern) {
                const auto source = value.toObject();
                if (diagnostic && source.value("type") == QJsonValue("active_base")) {
                    continue;
                }
                const auto id = source.value("type") == QJsonValue("active_base")
                        ? plan.playlistId : source.value("playlistId").toString();
                if (!id.isEmpty() && !row.sourcePlaylistIds.contains(id)) row.sourcePlaylistIds << id;
            }
            row.oneToOne = plan.pattern.size() == 2 && row.sourcePlaylistIds.size() == 2;
            if (diagnostic) {
                QStringList sources;
                for (const auto &value : plan.pattern) {
                    const auto source = value.toObject();
                    sources << (source.value("type") == QJsonValue("active_base") ? tr("Основной канал")
                        : name(document.object, "playlists", source.value("playlistId").toString()));
                }
                row.source = sources.join(QStringLiteral(" → "));
                row.oneToOne = plan.pattern.size() == 2;
            }
        } else if (!plan.silence && !plan.playlistId.isEmpty()) {
            row.sourcePlaylistIds << plan.playlistId;
        }
        row.volume = row.diagnosticMix ? -1 : plan.volumePercent;
        row.active = !diagnostic && row.from <= at && at < row.until;
        row.mode = row.silence ? tr("Тишина") : plan.mixRuleId.isEmpty() ? tr("Плейлист") : tr("Чередование");
        if (plan.usingFallback) row.mode += tr(" · резерв");
        row.rule = plan.usingFallback ? tr("Резервный источник") : name(document.object, "baseRules", plan.baseRuleId);
        if (!plan.mixRuleId.isEmpty()) {
            if (!row.rule.isEmpty()) row.rule += QStringLiteral(" / ");
            row.rule += name(document.object, "mixRules", plan.mixRuleId);
        }
        row.tooltip = QStringLiteral("%1 · %2\n%3 · %4\n%5").arg(row.time, row.source, row.mode,
                row.volume < 0 ? tr("Звук определяется основным каналом") : QStringLiteral("%1%").arg(row.volume), row.rule);
        row.tooltip += tr("\nСмена источника — после завершения текущего трека.");
        if (!plan.mixRuleId.isEmpty() && !plan.silence) row.tooltip += tr("\nИсточники чередуются по трекам в указанном порядке.");
        if (!plan.withinValidity) row.tooltip += tr("\nВне срока действия проекта.");
        if (diagnostic) {
            row.tooltip += tr("\nИнтервал правила; исполнение плана не определено.");
            for (const auto &conflict : preview.conflicts) {
                const auto ruleId = conflict.group == QLatin1String("baseRules") ? plan.baseRuleId : plan.mixRuleId;
                const auto begin = qMax(row.from, conflict.from), end = qMin(row.until, conflict.until);
                if (!conflict.ruleIds.contains(ruleId) || begin >= end) continue;
                row.conflicts.append({begin, end});
                QStringList rules;
                for (const auto &id : conflict.ruleIds) rules << name(document.object, conflict.group, id);
                row.tooltip += tr("\nПересечение %1–%2: %3").arg(timeText(begin), timeText(end), rules.join(QStringLiteral(" / ")));
            }
        }
        rows << row;
        if (!diagnostic && row.from > at && !d->snapshot.nextChannelTime.isValid()) {
            d->snapshot.nextChannelTime = row.from;
            d->snapshot.nextChannelNames = {row.source};
            d->snapshot.nextChannelSummary = timeText(row.from) + QStringLiteral(" · ") + row.source;
        }
    }
    auto events = diagnostic ? preview.events : ScheduleV1::events(document, from, until.addMSecs(-1));
    std::sort(events.begin(), events.end(), [](const auto &a, const auto &b) {
        if (a.scheduledUtc != b.scheduledUtc) return a.scheduledUtc < b.scheduledUtc;
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.ruleId < b.ruleId;
    });
    for (const auto &event : events) {
        Row row;
        row.event = true; row.from = event.scheduledUtc.toTimeZone(zone); row.priority = event.priority;
        row.eventRuleId = event.ruleId; row.eventAssetId = event.assetId;
        row.time = timeText(row.from);
        row.source = name(document.object, "assets", event.assetId);
        row.rule = name(document.object, "eventRules", event.ruleId);
        row.volume = event.volumePercent;
        row.mode = event.start == "after_track" ? tr("Вставка · после трека") : tr("Вставка · прерывание");
        row.tooltip = QStringLiteral("%1 · %2\n%3 · %4%\n%5").arg(row.time, row.source, row.mode).arg(row.volume).arg(row.rule);
        row.tooltip += tr("\nДопустимая задержка: %1 с. Приоритет: %2.").arg(event.maxLateSeconds).arg(event.priority);
        row.tooltip += tr("\nОтметка показывает назначенное время. Фактический запуск подтверждает плеер.");
        rows << row;
        if (!diagnostic && row.from >= at && !d->snapshot.nextAdvertTime.isValid()) {
            d->snapshot.nextAdvertTime = row.from;
            d->snapshot.nextAdvertNames = {row.rule};
            d->snapshot.nextAdvertSummary = row.time + QStringLiteral(" · ") + row.rule;
        }
        if (!diagnostic && row.from.toSecsSinceEpoch() / 60 == at.toSecsSinceEpoch() / 60)
            d->snapshot.exactAdvertsNow << row.rule;
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row &a, const Row &b) {
        if (a.from != b.from) return a.from < b.from;
        if (a.event != b.event) return !a.event;
        return a.priority > b.priority;
    });
    if (!d->snapshot.nextChannelTime.isValid()) d->snapshot.nextChannelSummary = tr("В выбранные сутки смен больше нет");
    if (!d->snapshot.nextAdvertTime.isValid()) d->snapshot.nextAdvertSummary = tr("В выбранные сутки вставок больше нет");
    d->table->setColumnWidth(0, offsets ? 250 : 160);
    d->model->replace(std::move(rows));
    bool hasSpecialRows = false;
    for (int i = 0; i < d->model->rows.size(); ++i) {
        const auto &row = d->model->rows.at(i);
        const bool special = row.event || row.mixed;
        d->table->setRowHidden(i, !special);
        hasSpecialRows |= special;
    }
    d->table->setVisible(hasSpecialRows);
    d->timeline->from = from; d->timeline->until = until; d->timeline->at = at;
    d->timeline->planValid = !diagnostic;
    d->timeline->rebuildLanes();
    if (selection) {
        int match = -1;
        for (int i = 0; i < d->model->rows.size(); ++i) {
            const auto &row = d->model->rows.at(i);
            if (row.event == selection->event && row.playlistId == selection->playlistId
                    && row.from == selection->from && row.until == selection->until
                    && row.eventRuleId == selection->eventRuleId && row.eventAssetId == selection->eventAssetId) {
                match = i;
                break;
            }
        }
        if (match >= 0) {
            d->table->selectRow(match);
            d->timeline->selected = match;
        } else if (!selectedPlaylist.isEmpty()) {
            selectPlaylist(selectedPlaylist);
        }
    } else if (!selectedPlaylist.isEmpty()) {
        selectPlaylist(selectedPlaylist);
    }
    adjustTimelineHeight();
    d->timeline->update();
}
