#include "scheduledocumentpreview.h"
#include "restyletheme.h"

#include <QAbstractTableModel>
#include <QFileInfo>
#include <QHeaderView>
#include <QHelpEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
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
    int volume = 0, priority = 0;
    bool event = false, active = false, fallback = false, silence = false, mixed = false, oneToOne = false;
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
        case 3: return QStringLiteral("%1%").arg(row.volume);
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
        for (const auto &row : mModel->rows)
            for (const auto &id : row.sourcePlaylistIds) addPlaylist(id, name(mDocument, "playlists", id));
        Lane service{{}, QStringLiteral("Резерв / тишина"), {}, false, true};
        Lane events{{}, QStringLiteral("Вставки"), {}, true};
        for (int i = 0; i < mModel->rows.size(); ++i) {
            const auto &row = mModel->rows.at(i);
            if (row.event) { events.rows << i; continue; }
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
            const QRectF titleRect(4, top + laneIndex * laneHeight, trackLeft() - 12, 26);
            const QRectF track(trackLeft(), titleRect.top(), trackWidth(), 26);
            p.setFont(Restyle::font(10, laneSelected ? QFont::DemiBold : QFont::Normal));
            p.setPen(laneSelected ? t.accentText : t.secondary);
            p.drawText(titleRect, Qt::AlignVCenter,
                       p.fontMetrics().elidedText(lane.title, Qt::ElideRight, int(titleRect.width())));
            p.setPen(laneSelected ? t.accentLine : t.line);
            p.setBrush(t.field);
            p.drawRoundedRect(track, 4, 4);
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
                    p.setPen(QPen(t.accent, intervalSelected ? 2 : 1));
                    p.setBrush(intervalSelected ? t.accent : t.accentSoft);
                    p.drawPolygon(QPolygonF{QPointF(x, y - 7), QPointF(x + 5, y), QPointF(x, y + 7), QPointF(x - 5, y)});
                    continue;
                }
                p.setBrush(row.active ? t.successBg : row.fallback ? t.surface2 : t.accentSoft);
                p.setPen(QPen(intervalSelected ? t.accent : row.active ? t.success : t.accentLine, intervalSelected ? 2 : 1));
                p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 3, 3);
                if (r.width() > 28) {
                    p.setPen(row.active ? t.success : t.text);
                    p.setFont(Restyle::font(9));
                    const QString label = lane.service ? row.source : row.mixed
                            ? (row.oneToOne ? QStringLiteral("1:1 · ") : QStringLiteral("Чередование · ")) + row.time : row.time;
                    p.drawText(r.adjusted(6, 0, -6, 0), Qt::AlignVCenter,
                               p.fontMetrics().elidedText(label, Qt::ElideRight, qMax(0, int(r.width()) - 12)));
                }
            }
        }
        if (hasTimes && at >= from && at < until) {
            const qreal x = position(at);
            p.setPen(QPen(t.accent, 1.5));
            p.drawLine(QPointF(x, top - 3), QPointF(x, height() - 6));
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
    qreal trackLeft() const { return qMin(164, qMax(90, width() / 4)); }
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
        return row.event ? QRectF(x - 6, y + 2, 12, 22)
                         : QRectF(x, y, qMax(qreal(1), position(row.until) - x), 26);
    }
    static constexpr int top = 26, laneHeight = 29;
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
    auto *timelineScroll = d->timelineScroll = new QScrollArea(this);
    timelineScroll->setObjectName(QStringLiteral("scheduleDocumentTimelineScroll"));
    timelineScroll->setFrameShape(QFrame::NoFrame);
    timelineScroll->setWidgetResizable(true);
    timelineScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    timelineScroll->setMinimumHeight(92);
    timelineScroll->setMaximumHeight(184);
    timelineScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    timelineScroll->setWidget(d->timeline);
    d->timeline->rebuildLanes();
    layout->addWidget(timelineScroll);
    d->table = new QTableView(this);
    d->table->setObjectName(QStringLiteral("scheduleDocumentTable"));
    d->table->setAccessibleName(QStringLiteral("Интервалы вещания и запланированные вставки"));
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
            d->table->setFocus(Qt::MouseFocusReason);
            if (row >= 0) {
                d->table->selectRow(row);
                d->table->scrollTo(d->model->index(row, 0));
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
    d->timelineScroll->setFixedHeight(qBound(92,
        qMin(d->timeline->height(), height() - d->table->minimumHeight()), 184));
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
    if (watched == d->table && event->type() == QEvent::KeyPress) {
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
    d->timeline->from = d->timeline->until = d->timeline->at = {};
    d->timeline->selected = -1;
    d->timeline->planValid = false;
    d->timeline->setDocument(document.object);
    adjustTimelineHeight();
    if (!document.compiled || !wallTime.isValid()) {
        d->snapshot.issues << tr("Проект расписания не проверен.");
        d->snapshot.hasUnresolvedRules = true;
        return;
    }
    const QTimeZone zone(document.object.value("timeZone").toString().toUtf8());
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
    const auto current = ScheduleV1::evaluate(document, at);
    d->snapshot.currentSummary = tr("По плану: %1 · %2%").arg(sourceName(document.object, current)).arg(current.volumePercent);
    d->snapshot.issues = current.diagnostics;
    const auto intervals = ScheduleV1::intervals(document, from, until);
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
        row.mixed = !plan.mixRuleId.isEmpty() && !plan.silence;
        if (row.mixed) {
            for (const auto &value : plan.pattern) {
                const auto source = value.toObject();
                const auto id = source.value("type") == QJsonValue("active_base")
                        ? plan.playlistId : source.value("playlistId").toString();
                if (!id.isEmpty() && !row.sourcePlaylistIds.contains(id)) row.sourcePlaylistIds << id;
            }
            row.oneToOne = plan.pattern.size() == 2 && row.sourcePlaylistIds.size() == 2;
        } else if (!plan.silence && !plan.playlistId.isEmpty()) {
            row.sourcePlaylistIds << plan.playlistId;
        }
        row.volume = plan.volumePercent;
        row.active = row.from <= at && at < row.until;
        row.mode = plan.silence ? tr("Тишина") : plan.mixRuleId.isEmpty() ? tr("Плейлист") : tr("Чередование");
        if (plan.usingFallback) row.mode += tr(" · резерв");
        row.rule = plan.usingFallback ? tr("Резервный источник") : name(document.object, "baseRules", plan.baseRuleId);
        if (!plan.mixRuleId.isEmpty()) row.rule += QStringLiteral(" / ") + name(document.object, "mixRules", plan.mixRuleId);
        row.tooltip = QStringLiteral("%1 · %2\n%3 · %4%\n%5").arg(row.time, row.source, row.mode).arg(row.volume).arg(row.rule);
        row.tooltip += tr("\nСмена источника — после завершения текущего трека.");
        if (!plan.mixRuleId.isEmpty() && !plan.silence) row.tooltip += tr("\nИсточники чередуются по трекам в указанном порядке.");
        if (!plan.withinValidity) row.tooltip += tr("\nВне срока действия проекта.");
        rows << row;
        if (row.from > at && !d->snapshot.nextChannelTime.isValid()) {
            d->snapshot.nextChannelTime = row.from;
            d->snapshot.nextChannelNames = {row.source};
            d->snapshot.nextChannelSummary = timeText(row.from) + QStringLiteral(" · ") + row.source;
        }
    }
    auto events = ScheduleV1::events(document, from, until.addMSecs(-1));
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
        if (row.from >= at && !d->snapshot.nextAdvertTime.isValid()) {
            d->snapshot.nextAdvertTime = row.from;
            d->snapshot.nextAdvertNames = {row.rule};
            d->snapshot.nextAdvertSummary = row.time + QStringLiteral(" · ") + row.rule;
        }
        if (row.from.toSecsSinceEpoch() / 60 == at.toSecsSinceEpoch() / 60)
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
    d->timeline->from = from; d->timeline->until = until; d->timeline->at = at;
    d->timeline->planValid = true;
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
