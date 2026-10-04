#include "schedulepreview.h"
#include "advertmodel.h"
#include "restyletheme.h"
#include "restylewidgets.h"

#include <QAbstractItemModel>
#include <QAbstractTableModel>
#include <QButtonGroup>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QItemSelectionModel>
#include <QLinearGradient>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QSignalBlocker>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTextEdit>
#include <QTimeEdit>
#include <QTimeZone>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
QVariant field(const QAbstractItemModel *model, int row, int column)
{
    const auto index = model->index(row, column);
    const QVariant edit = index.data(Qt::EditRole);
    return edit.isValid() ? edit : index.data(Qt::DisplayRole);
}

QTime timeValue(const QVariant &value)
{
    if (value.metaType().id() == QMetaType::QTime)
        return value.toTime();
    const QString text = value.toString();
    const QTime parsed = QTime::fromString(text, QStringLiteral("HH:mm"));
    return parsed.toString(QStringLiteral("HH:mm")) == text ? parsed : QTime();
}

QString weekdayLabel(const QString &text)
{
    const auto parsed = ScheduleCore::parseCalendar(text, 0, 6);
    if (!parsed.valid)
        return text;
    if (parsed.values.size() == 7)
        return QStringLiteral("Каждый день");
    const QStringList names{QStringLiteral("Вс"), QStringLiteral("Пн"), QStringLiteral("Вт"),
                            QStringLiteral("Ср"), QStringLiteral("Чт"), QStringLiteral("Пт"), QStringLiteral("Сб")};
    QStringList output;
    for (int index = 1; index <= 7; ++index)
        if (parsed.values.contains(index % 7))
            output.append(names.at(index % 7));
    return output.join(QStringLiteral(", "));
}

QString monthLabel(const QString &text)
{
    const auto parsed = ScheduleCore::parseCalendar(text, 1, 12);
    if (!parsed.valid)
        return text;
    if (parsed.values.size() == 12)
        return QStringLiteral("Все");
    const QStringList names{QStringLiteral("Янв"), QStringLiteral("Фев"), QStringLiteral("Мар"),
                           QStringLiteral("Апр"), QStringLiteral("Май"), QStringLiteral("Июн"),
                           QStringLiteral("Июл"), QStringLiteral("Авг"), QStringLiteral("Сен"),
                           QStringLiteral("Окт"), QStringLiteral("Ноя"), QStringLiteral("Дек")};
    QStringList output;
    for (int month = 1; month <= 12; ++month)
        if (parsed.values.contains(month))
            output.append(names.at(month - 1));
    return output.join(QStringLiteral(", "));
}

}

SchedulePreview::Snapshot SchedulePreview::evaluate(const QAbstractItemModel *channels,
                                                    const QAbstractItemModel *adverts,
                                                    const QDateTime &at, int horizonDays)
{
    QList<ScheduleCore::ChannelRule> channelRules;
    QList<ScheduleCore::AdvertRule> advertRules;
    for (int row = 0; channels && row < channels->rowCount(); ++row) {
        ScheduleCore::ChannelRule rule;
        rule.name = field(channels, row, 0).toString();
        rule.start = timeValue(field(channels, row, 1));
        rule.end = timeValue(field(channels, row, 2));
        rule.weekdays = field(channels, row, 3).toString();
        rule.days = field(channels, row, 4).toString();
        rule.months = field(channels, row, 5).toString();
        bool volumeOk = false;
        rule.volume = field(channels, row, 6).toInt(&volumeOk);
        if (!volumeOk)
            rule.volume = -1;
        channelRules.append(rule);
    }
    for (int row = 0; adverts && row < adverts->rowCount(); ++row) {
        ScheduleCore::AdvertRule rule;
        rule.name = field(adverts, row, 0).toString();
        rule.hours = field(adverts, row, 1).toString();
        rule.timing = field(adverts, row, 2).toString();
        rule.weekdays = field(adverts, row, 3).toString();
        rule.from = field(adverts, row, 4).toDate();
        rule.until = field(adverts, row, 5).toDate();
        const QVariant published = adverts->index(row, 0).data(AdvertModel::CompiledMinutesRole);
        for (const QVariant &minute : published.toList()) {
            bool minuteOk = false;
            const int value = minute.toInt(&minuteOk);
            rule.compiledMinutes.append(minuteOk ? value : -1);
        }
        bool volumeOk = false;
        rule.volume = field(adverts, row, 6).toInt(&volumeOk);
        if (!volumeOk)
            rule.volume = -1;
        advertRules.append(rule);
    }
    return ScheduleCore::evaluate(channelRules, advertRules, at, horizonDays);
}

namespace {
class PreviewTableModel final : public QAbstractTableModel
{
public:
    explicit PreviewTableModel(bool timeline, QObject *parent) : QAbstractTableModel(parent), mTimeline(timeline) {}
    SchedulePreview::Snapshot plan;
    void setPlan(const SchedulePreview::Snapshot &value) { beginResetModel(); plan = value; endResetModel(); }
    int rowCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : int(plan.channels.size()); }
    int columnCount(const QModelIndex &parent = {}) const override { return parent.isValid() ? 0 : mTimeline ? 1 : 8; }
    QVariant data(const QModelIndex &index, int role) const override
    {
        if (!index.isValid() || index.row() >= plan.channels.size())
            return {};
        const auto &channel = plan.channels.at(index.row());
        if (role == Qt::AccessibleTextRole && index.column() == 7)
            return QStringLiteral("Изменить правило канала ") + channel.name;
        if (role == Qt::DecorationRole && index.column() == 7)
            return Restyle::icon(QStringLiteral("edit"));
        if (role == Qt::ToolTipRole)
            return channel.name + QStringLiteral("\n") + channel.reason;
        if (role != Qt::DisplayRole)
            return {};
        switch (index.column()) {
        case 0: return channel.name;
        case 1: return channel.start.toString(QStringLiteral("HH:mm")) + QStringLiteral("–") + channel.end.toString(QStringLiteral("HH:mm"));
        case 2: return weekdayLabel(channel.weekdays);
        case 3: return channel.days == QLatin1String("*") ? QStringLiteral("Все") : channel.days;
        case 4: return monthLabel(channel.months);
        case 5: return QStringLiteral("%1%").arg(channel.volume);
        case 6: return channel.status;
        default: return QString();
        }
    }
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override
    {
        if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
            return {};
        const QStringList headings{QStringLiteral("Канал"), QStringLiteral("Время"), QStringLiteral("Дни недели"),
                                   QStringLiteral("Дни"), QStringLiteral("Месяцы"), QStringLiteral("Звук"), QStringLiteral("Статус плана"), QString()};
        return headings.value(section);
    }
private:
    bool mTimeline;
};

class TimelineDelegate final : public QStyledItemDelegate
{
public:
    explicit TimelineDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const auto *model = static_cast<const PreviewTableModel *>(index.model());
        const auto &channel = model->plan.channels.at(index.row());
        const auto &theme = Restyle::tokens();
        const bool selected = option.state.testFlag(QStyle::State_Selected);
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->fillRect(option.rect, theme.panel);
        painter->setFont(Restyle::font(10, selected ? QFont::DemiBold : QFont::Normal));
        painter->setPen(selected ? theme.accentText : theme.secondary);
        const QRect label = option.rect.adjusted(0, 0, 0, 0);
        painter->drawText(QRect(label.left(), label.top(), 146, label.height()), Qt::AlignVCenter,
                          painter->fontMetrics().elidedText(channel.name, Qt::ElideRight, 142));
        const QRectF track(option.rect.left() + 156, option.rect.top() + 1, option.rect.width() - 158, 23);
        if (track.width() <= 0) { painter->restore(); return; }
        painter->setPen(Qt::NoPen);
        painter->setBrush(theme.field);
        painter->drawRoundedRect(track, theme.trackRadius, theme.trackRadius);
        if (theme.relief) {
            painter->setPen(QColor(0, 0, 0, theme.dark ? 65 : 13));
            painter->drawLine(track.topLeft() + QPointF(4, 1), track.topRight() - QPointF(4, -1));
        }
        painter->setPen(theme.line);
        for (int quarter = 1; quarter < 4; ++quarter) {
            const qreal x = track.left() + track.width() * quarter / 4;
            painter->drawLine(QPointF(x, track.top()), QPointF(x, track.bottom()));
        }
        if (channel.valid && channel.calendarMatches) {
            const int start = channel.start.hour() * 60 + channel.start.minute();
            const int end = channel.end.hour() * 60 + channel.end.minute();
            QRectF interval(track.left() + track.width() * start / 1440, track.top() + 1,
                            track.width() * (end - start) / 1440, 21);
            if (theme.relief) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(QColor(0, 0, 0, theme.dark ? 90 : 20));
                painter->drawRoundedRect(interval.translated(1, 1), theme.intervalRadius, theme.intervalRadius);
                QLinearGradient gradient(interval.topLeft(), interval.bottomRight());
                gradient.setColorAt(0, theme.dark ? theme.buttonTop : theme.surface);
                gradient.setColorAt(1, selected ? theme.accentSoft : theme.surface2);
                painter->setBrush(gradient);
            } else {
                painter->setBrush(selected ? theme.accentSoft : theme.surface2);
            }
            painter->setPen(selected ? theme.accentLine : theme.line);
            painter->drawRoundedRect(interval, theme.intervalRadius, theme.intervalRadius);
            painter->setClipRect(interval);
            painter->setFont(Restyle::font(9));
            painter->setPen(selected ? theme.accentText : theme.secondary);
            painter->drawText(interval.adjusted(5, 0, -3, 0), Qt::AlignCenter,
                              channel.start.toString(QStringLiteral("HH:mm")) + QStringLiteral("–") + channel.end.toString(QStringLiteral("HH:mm")));
            painter->setClipping(false);
        } else {
            painter->setFont(Restyle::font(9));
            painter->setPen(channel.valid ? theme.muted : theme.warning);
            painter->drawText(track.adjusted(7, 0, -4, 0), Qt::AlignVCenter,
                              channel.valid ? QStringLiteral("Нет выхода в этот день") : QStringLiteral("Условия требуют проверки"));
        }
        const QTime at = model->plan.at.time();
        const qreal cursor = track.left() + track.width() * (at.hour() * 60 + at.minute()) / 1440;
        painter->setPen(QPen(theme.error, 1));
        painter->drawLine(QPointF(cursor, track.top() - 1), QPointF(cursor, track.bottom() + 1));
        painter->fillRect(QRectF(cursor - 2, track.top() - 1, 4, 4), theme.error);
        if (option.state.testFlag(QStyle::State_HasFocus)) {
            painter->setBrush(Qt::NoBrush);
            painter->setPen(QPen(theme.focus, 1, Qt::DotLine));
            painter->drawRoundedRect(option.rect.adjusted(1, 1, -1, -1), 3, 3);
        }
        painter->restore();
    }
};

class ConditionsDelegate final : public QStyledItemDelegate
{
public:
    explicit ConditionsDelegate(QObject *parent) : QStyledItemDelegate(parent) {}
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        if (index.column() != 6 && index.column() != 7) {
            QStyledItemDelegate::paint(painter, option, index);
            return;
        }
        const auto *model = static_cast<const PreviewTableModel *>(index.model());
        const auto &channel = model->plan.channels.at(index.row());
        const auto &theme = Restyle::tokens();
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->fillRect(option.rect, option.state.testFlag(QStyle::State_Selected) ? theme.selection : theme.panel);
        if (index.column() == 6) {
            painter->setFont(Restyle::font(9));
            const int width = std::min(option.rect.width() - 12, painter->fontMetrics().horizontalAdvance(channel.status) + 10);
            const QRectF badge(option.rect.left() + 6, option.rect.center().y() - 8, width, 17);
            painter->setPen(channel.active ? theme.successBg.darker(110) : theme.line);
            painter->setBrush(!channel.valid ? theme.warningBg : channel.active ? theme.successBg : theme.surface);
            painter->drawRoundedRect(badge, 2, 2);
            painter->setPen(!channel.valid ? theme.warning : channel.active ? theme.success : theme.secondary);
            painter->drawText(badge, Qt::AlignCenter, painter->fontMetrics().elidedText(channel.status, Qt::ElideRight, width - 8));
        } else {
            const QRectF button(option.rect.center().x() - 11, option.rect.center().y() - 11, 23, 23);
            painter->setPen(theme.relief ? theme.surface : theme.line);
            if (theme.relief) {
                QLinearGradient gradient(button.topLeft(), button.bottomRight());
                gradient.setColorAt(0, theme.buttonTop);
                gradient.setColorAt(1, theme.buttonBottom);
                painter->setBrush(gradient);
            } else {
                painter->setBrush(theme.surface);
            }
            painter->drawRoundedRect(button, theme.buttonRadius, theme.buttonRadius);
            Restyle::paintIcon(*painter, button.adjusted(5, 5, -5, -5), QStringLiteral("edit"), theme.secondary);
        }
        painter->restore();
    }
};

class TimelineScale final : public QWidget
{
public:
    explicit TimelineScale(QWidget *parent) : QWidget(parent) { setFixedHeight(17); }
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setFont(Restyle::font(9));
        painter.setPen(Restyle::tokens().muted);
        for (int quarter = 0; quarter <= 4; ++quarter) {
            const QString label = QStringLiteral("%1:00").arg(quarter * 6, 2, 10, QLatin1Char('0'));
            const qreal x = 156 + (width() - 158) * quarter / 4.0;
            const int textWidth = painter.fontMetrics().horizontalAdvance(label);
            painter.drawText(QPointF(x - (quarter == 4 ? textWidth : quarter == 0 ? 0 : textWidth / 2), 12), label);
        }
    }
};

class ScheduleStatus final : public RestyleLabel
{
public:
    explicit ScheduleStatus(QWidget *parent) : RestyleLabel({}, 10, QFont::Normal, parent)
    {
        setContentsMargins(20, 0, 5, 0);
    }
    void paintEvent(QPaintEvent *event) override
    {
        RestyleLabel::paintEvent(event);
        QPainter painter(this);
        const auto &theme = Restyle::tokens();
        painter.setPen(theme.line);
        painter.drawLine(0, 0, width(), 0);
        painter.drawLine(0, height() - 1, width(), height() - 1);
        const bool issue = property("scheduleIssue").toBool();
        Restyle::paintIcon(painter, QRectF(0, (height() - 12) / 2, 12, 12),
                           issue ? QStringLiteral("info") : QStringLiteral("check"),
                           issue ? theme.warning : theme.success);
    }
};
}

struct SchedulePreviewWidget::Private {
    QPointer<QAbstractItemModel> channels, adverts;
    QList<QMetaObject::Connection> connections;
    SchedulePreview::Snapshot plan;
    QDateEdit *date;
    QTimeEdit *time;
    QButtonGroup *days;
    QList<QPushButton *> dayButtons;
    QTableView *timeline, *conditions;
    PreviewTableModel *timelineModel, *conditionsModel;
    RestyleLabel *status, *legend;
    int selected = -1;
    QPersistentModelIndex selectedSource;
};

SchedulePreviewWidget::SchedulePreviewWidget(QWidget *parent) : QWidget(parent), d(new Private)
{
    setObjectName(QStringLiteral("schedulePreview"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *panel = new RestylePanel(this);
    outer->addWidget(panel);
    auto *layout = new QVBoxLayout(panel);
    layout->setContentsMargins(16, 16, 16, 15);
    layout->setSpacing(0);
    auto *heading = new QHBoxLayout;
    heading->setSpacing(6);
    heading->addWidget(new RestyleLabel(tr("Расписание каналов"), 14, QFont::DemiBold, panel));
    heading->addStretch();
    d->date = new QDateEdit(QDate::currentDate(), panel);
    d->date->setObjectName(QStringLiteral("previewDate"));
    d->date->setCalendarPopup(true);
    d->date->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    d->date->setAccessibleName(tr("Дата просмотра плана"));
    d->date->setFixedWidth(114);
    d->date->hide();
    d->time = new QTimeEdit(QTime::currentTime(), panel);
    d->time->setObjectName(QStringLiteral("previewTime"));
    d->time->setDisplayFormat(QStringLiteral("HH:mm"));
    d->time->setAccessibleName(tr("Время просмотра плана"));
    d->time->setFixedWidth(72);
    d->time->hide();
    auto *details = new QPushButton(tr("Условия"), panel);
    details->setIcon(Restyle::icon(QStringLiteral("info")));
    Restyle::button(details, QStringLiteral("quiet"));
    heading->addWidget(details);
    heading->addSpacing(8);
    auto *dayStrip = new RestylePanel(panel, QStringLiteral("inset"));
    dayStrip->setFixedSize(298, 32);
    auto *toolbar = new QHBoxLayout;
    toolbar->setSpacing(3);
    toolbar->setContentsMargins(3, 3, 3, 3);
    dayStrip->setLayout(toolbar);
    d->days = new QButtonGroup(this);
    d->days->setExclusive(true);
    for (int day = 0; day < 7; ++day) {
        auto *button = new QPushButton(panel);
        button->setCheckable(true);
        button->setFixedSize(39, 26);
        button->setFont(Restyle::font(9));
        Restyle::button(button, QStringLiteral("day"));
        button->setFont(Restyle::font(9));
        d->days->addButton(button, day);
        d->dayButtons.append(button);
        toolbar->addWidget(button);
    }
    heading->addWidget(dayStrip);
    d->legend = new RestyleLabel({}, 10, QFont::Normal, panel);
    d->legend->setColorRole(QStringLiteral("secondary"));
    d->legend->hide();
    layout->addLayout(heading);
    layout->addSpacing(10);
    layout->addWidget(new TimelineScale(panel));
    layout->addSpacing(4);

    d->timeline = new QTableView(panel);
    d->timeline->setObjectName(QStringLiteral("scheduleTimeline"));
    d->conditions = new QTableView(panel);
    d->conditions->setObjectName(QStringLiteral("scheduleConditions"));
    d->conditions->setIconSize(QSize(14, 14));
    d->timelineModel = new PreviewTableModel(true, this);
    d->conditionsModel = new PreviewTableModel(false, this);
    d->timeline->setModel(d->timelineModel);
    d->conditions->setModel(d->conditionsModel);
    d->timeline->setItemDelegate(new TimelineDelegate(d->timeline));
    d->conditions->setItemDelegate(new ConditionsDelegate(d->conditions));
    for (auto *view : {d->timeline, d->conditions}) {
        view->setFrameShape(QFrame::NoFrame);
        view->setShowGrid(false);
        view->setEditTriggers(QAbstractItemView::NoEditTriggers);
        view->setSelectionBehavior(QAbstractItemView::SelectRows);
        view->setSelectionMode(QAbstractItemView::SingleSelection);
        view->verticalHeader()->hide();
        view->verticalHeader()->setMinimumSectionSize(25);
        view->verticalHeader()->setDefaultSectionSize(view == d->timeline ? 29 : 31);
        view->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
        view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        view->setFont(Restyle::font(10));
    }
    d->timeline->horizontalHeader()->hide();
    d->timeline->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    d->timeline->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    d->timeline->setMinimumHeight(29);
    d->timeline->setMaximumHeight(87);
    d->timeline->setAccessibleName(tr("Временная шкала каналов; стрелки выбирают канал"));
    layout->addWidget(d->timeline, 1);
    layout->addSpacing(9);
    d->status = new ScheduleStatus(panel);
    d->status->setFixedHeight(33);
    d->status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(d->status);
    layout->addSpacing(7);
    d->conditions->horizontalHeader()->setFixedHeight(26);
    d->conditions->horizontalHeader()->setFont(Restyle::font(10));
    d->conditions->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    d->conditions->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int widths[]{150, 100, 130, 65, 85, 50, 135, 68};
    for (int column = 0; column < 8; ++column)
        d->conditions->setColumnWidth(column, widths[column]);
    d->conditions->horizontalHeader()->setSectionResizeMode(6, QHeaderView::Stretch);
    d->conditions->setMinimumHeight(57);
    d->conditions->setAccessibleName(tr("Календарные условия каналов"));
    layout->addWidget(d->conditions, 1);
    connect(details, &QPushButton::clicked, this, &SchedulePreviewWidget::showConditions);
    connect(d->date, &QDateEdit::dateChanged, this, &SchedulePreviewWidget::refresh);
    connect(d->time, &QTimeEdit::timeChanged, this, &SchedulePreviewWidget::refresh);
    connect(d->days, &QButtonGroup::idClicked, this, [this](int day) {
        d->date->setDate(d->date->date().addDays(day + 1 - d->date->date().dayOfWeek()));
    });
    for (auto *view : {d->timeline, d->conditions}) {
        connect(view->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                [this](const QModelIndex &current) {
            if (current.isValid()) {
                setSelectedRow(current.row());
                emit selectedRowChanged(current.row());
            }
        });
    }
    connect(d->conditions, &QTableView::clicked, this, [this](const QModelIndex &index) {
        if (index.column() == 7)
            emit editRequested(index.row());
    });
    connect(d->conditions, &QTableView::doubleClicked, this, [this](const QModelIndex &index) { emit editRequested(index.row()); });
    refresh();
}

SchedulePreviewWidget::~SchedulePreviewWidget()
{
    for (const auto &connection : std::as_const(d->connections))
        disconnect(connection);
    delete d;
}

void SchedulePreviewWidget::setModels(QAbstractItemModel *channels, QAbstractItemModel *adverts)
{
    for (const auto &connection : std::as_const(d->connections))
        disconnect(connection);
    d->connections.clear();
    d->channels = channels;
    d->adverts = adverts;
    d->selected = -1;
    d->selectedSource = QPersistentModelIndex();
    for (auto *model : {channels, adverts}) {
        if (!model)
            continue;
        d->connections.append(connect(model, &QAbstractItemModel::dataChanged, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QAbstractItemModel::modelReset, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QAbstractItemModel::rowsInserted, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QAbstractItemModel::rowsRemoved, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QAbstractItemModel::layoutChanged, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QObject::destroyed, this, &SchedulePreviewWidget::refresh));
    }
    refresh();
}

void SchedulePreviewWidget::setSelectedRow(int row)
{
    d->selected = row >= 0 && row < d->plan.channels.size() ? row : -1;
    d->selectedSource = d->channels && d->selected >= 0 ? d->channels->index(d->selected, 0) : QModelIndex();
    for (auto *view : {d->timeline, d->conditions}) {
        const QSignalBlocker blocker(view->selectionModel());
        if (d->selected < 0) {
            view->clearSelection();
            view->setCurrentIndex({});
        } else {
            view->setCurrentIndex(view->model()->index(d->selected, 0));
            view->selectRow(d->selected);
        }
    }
}

int SchedulePreviewWidget::selectedRow() const { return d->selected; }
void SchedulePreviewWidget::setPreviewDateTime(const QDateTime &dateTime)
{
    if (!dateTime.isValid())
        return;
    const QSignalBlocker dateBlocker(d->date), timeBlocker(d->time);
    d->date->setDate(dateTime.date());
    d->time->setTime(dateTime.time());
    refresh();
}
QDateTime SchedulePreviewWidget::previewDateTime() const { return QDateTime(d->date->date(), d->time->time()); }
const SchedulePreview::Snapshot &SchedulePreviewWidget::snapshot() const { return d->plan; }
QSize SchedulePreviewWidget::sizeHint() const { return QSize(1177, 349); }

void SchedulePreviewWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const int width = std::max(700, event->size().width() - 32);
    const qreal proportions[]{.227, .146, .166, .075, .095, .069};
    for (int column = 0; column < 6; ++column)
        d->conditions->setColumnWidth(column, qRound(width * proportions[column]));
    d->conditions->setColumnWidth(7, 31);
}

void SchedulePreviewWidget::refresh()
{
    const QString previousName = d->selected >= 0 && d->selected < d->plan.channels.size()
            ? d->plan.channels.at(d->selected).name : QString();
    d->plan = SchedulePreview::evaluate(d->channels, d->adverts, previewDateTime());
    // Persistent indexes follow insertions/removals. ChannelModel resets after
    // reloading its storage; unique channel names recover selection in that case.
    if (d->selectedSource.isValid()) {
        d->selected = d->selectedSource.row();
    } else if (!previousName.isEmpty()) {
        d->selected = -1;
        for (const auto &channel : std::as_const(d->plan.channels)) {
            if (channel.name == previousName) {
                d->selected = channel.sourceRow;
                break;
            }
        }
    }
    // Resetting view models clears selection. Preserve the independently chosen
    // channel and suppress selection feedback while refreshing computed rows.
    const QSignalBlocker timelineBlocker(d->timeline->selectionModel());
    const QSignalBlocker conditionsBlocker(d->conditions->selectionModel());
    d->timelineModel->setPlan(d->plan);
    d->conditionsModel->setPlan(d->plan);
    setSelectedRow(d->selected);
    const QDate monday = d->date->date().addDays(1 - d->date->date().dayOfWeek());
    const QStringList days{tr("Пн"), tr("Вт"), tr("Ср"), tr("Чт"), tr("Пт"), tr("Сб"), tr("Вс")};
    for (int day = 0; day < 7; ++day) {
        const QDate date = monday.addDays(day);
        auto *button = d->dayButtons.at(day);
        button->setText(days.at(day) + QStringLiteral(" %1").arg(date.day()));
        button->setToolTip(date.toString(QStringLiteral("dd.MM.yyyy")));
        button->setAccessibleName(days.at(day) + QStringLiteral(" ") + date.toString(QStringLiteral("dd.MM.yyyy")));
        button->setChecked(date == d->date->date());
    }
    d->legend->setText(d->time->time().toString(QStringLiteral("HH:mm")) + tr(" · просмотр плана"));
    QString status;
    if (!d->plan.issues.isEmpty())
        status = d->plan.issues.first();
    else if (d->plan.channels.isEmpty())
        status = tr("Каналы пока не созданы");
    else if (std::none_of(d->plan.channels.cbegin(), d->plan.channels.cend(), [](const auto &channel) {
                 return channel.valid && channel.calendarMatches;
             }))
        status = tr("На этот день нет назначенных каналов");
    else
        status = tr("Нет пересечений и внутренних разрывов");
    if (d->plan.issues.size() > 1)
        status += tr(" · ещё %1").arg(d->plan.issues.size() - 1);
    d->status->setColorRole(d->plan.issues.isEmpty() ? QStringLiteral("secondary") : QStringLiteral("warning"));
    d->status->setProperty("scheduleIssue", !d->plan.issues.isEmpty());
    d->status->setText(d->status->fontMetrics().elidedText(status, Qt::ElideRight, std::max(350, width() - 40)));
    d->status->setToolTip(status);
    emit snapshotChanged();
}

void SchedulePreviewWidget::showConditions()
{
    QStringList lines;
    lines << tr("Просмотр: %1").arg(d->plan.at.toString(QStringLiteral("dd.MM.yyyy HH:mm"))) << d->plan.currentSummary;
    if (d->selected >= 0)
        lines << QString() << d->plan.channels.at(d->selected).name + QStringLiteral(": ") + d->plan.channels.at(d->selected).reason;
    lines << QString() << tr("Следующая смена канала: %1").arg(d->plan.nextChannelSummary)
          << tr("Следующий точный выход рекламы: %1").arg(d->plan.nextAdvertSummary);
    if (!d->plan.exactAdvertsNow.isEmpty())
        lines << tr("Реклама в выбранную минуту: %1").arg(d->plan.exactAdvertsNow.join(QStringLiteral(", ")));
    lines << d->plan.frequencyAdvertsNow;
    lines << QString() << d->plan.issues << QString()
          << tr("Расчёт использует существующие календарные условия и дневные интервалы [начало, окончание). Начало включено, окончание исключено.")
          << tr("Ночные интервалы и одинаковые начало/конец требуют проверки: их семантика в действующем движке не подтверждена.")
          << tr("При переводе часов несуществующее или неоднозначное локальное время исключено из поиска точных событий; поведение внешнего плеера не предполагается.")
          << tr("Минуты частотной рекламы рассчитаны тем же правилом, что и экспорт. Текущий файл и факт воспроизведения этим расчётом не определяются.")
          << tr("Поиск ограничен %1 календарными днями. Просмотр не изменяет расписание и системное время.").arg(d->plan.horizonDays);
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Проверка плана"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *explanation = new QTextEdit(&dialog);
    explanation->setReadOnly(true);
    explanation->setPlainText(lines.join(QLatin1Char('\n')));
    explanation->setAccessibleName(tr("Объяснение календарных условий и расчёта плана"));
    layout->addWidget(explanation);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    layout->addWidget(buttons);
    dialog.resize(QSize(670, 490).boundedTo(screen()->availableGeometry().size() - QSize(40, 80)));
    dialog.exec();
}
