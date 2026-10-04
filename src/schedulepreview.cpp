#include "schedulepreview.h"
#include "advertmodel.h"
#include "channelmodel.h"
#include "scheduledocumentdialog.h"
#include "scheduledocumentpreview.h"
#include "schedulecore/schedulev1.h"
#include "restyletheme.h"
#include "restylewidgets.h"

#include <QAbstractItemModel>
#include <QButtonGroup>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPointer>
#include <QPushButton>
#include <QScreen>
#include <QSignalBlocker>
#include <QTextEdit>
#include <QTimeEdit>
#include <QTimer>
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

QTimeZone previewZone(const QJsonObject &document)
{
    const QTimeZone zone(document.value(QStringLiteral("timeZone")).toString().toUtf8());
    return zone.isValid() ? zone : QTimeZone::systemTimeZone();
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
        rule.untilDayOffset = channels->index(row, 0).data(ChannelModel::UntilDayOffsetRole).toInt();
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
    QPointer<QAbstractItemModel> channels;
    QJsonObject document;
    ScheduleV1::Document compiledDocument;
    QString documentError, documentDescription;
    bool followingCurrentTime = true;
    ScheduleDocumentPreview *documentPreview;
    QList<QMetaObject::Connection> connections;
    SchedulePreview::Snapshot plan;
    QDateEdit *date;
    QTimeEdit *time;
    QButtonGroup *days;
    QList<QPushButton *> dayButtons;
    RestyleLabel *status;
    int selected = -1;
    QString selectedName, selectedId;
    QPersistentModelIndex selectedSource;

    int channelRow(const QString &playlistId) const {
        if (playlistId.isEmpty()) return -1;
        for (int row = 0; channels && row < channels->rowCount(); ++row)
            if (channels->index(row, 0).data(ChannelModel::RuleIdRole).toString() == playlistId)
                return row;
        return -1;
    }
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
    heading->addWidget(new RestyleLabel(tr("Расписание"), 14, QFont::DemiBold, panel));
    heading->addStretch();
    d->date = new QDateEdit(QDate::currentDate(), panel);
    d->date->setObjectName(QStringLiteral("previewDate"));
    d->date->setCalendarPopup(true);
    d->date->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    d->date->setAccessibleName(tr("Дата просмотра плана"));
    d->date->setFixedWidth(114);
    d->time = new QTimeEdit(QTime::currentTime(), panel);
    d->time->setObjectName(QStringLiteral("previewTime"));
    d->time->setDisplayFormat(QStringLiteral("HH:mm"));
    d->time->setAccessibleName(tr("Время просмотра плана"));
    d->time->setFixedWidth(72);
    heading->addWidget(d->date);
    heading->addWidget(d->time);
    auto *details = new QPushButton(tr("Подробности"), panel);
    details->setObjectName(QStringLiteral("scheduleDetailsButton"));
    details->setIcon(Restyle::icon(QStringLiteral("info")));
    Restyle::button(details, QStringLiteral("quiet"));
    heading->addWidget(details);
    heading->addSpacing(8);
    auto *dayStrip = new RestylePanel(panel, QStringLiteral("inset"));
    dayStrip->setFixedSize(298, 32);
    auto *toolbar = new QHBoxLayout(dayStrip);
    toolbar->setSpacing(3);
    toolbar->setContentsMargins(3, 3, 3, 3);
    d->days = new QButtonGroup(this);
    d->days->setExclusive(true);
    for (int day = 0; day < 7; ++day) {
        auto *button = new QPushButton(panel);
        button->setCheckable(true);
        button->setFixedSize(39, 26);
        Restyle::button(button, QStringLiteral("day"));
        button->setFont(Restyle::font(9));
        d->days->addButton(button, day);
        d->dayButtons.append(button);
        toolbar->addWidget(button);
    }
    heading->addWidget(dayStrip);
    layout->addLayout(heading);
    layout->addSpacing(10);
    d->documentPreview = new ScheduleDocumentPreview(panel);
    layout->addWidget(d->documentPreview, 1);
    layout->addSpacing(7);
    d->status = new ScheduleStatus(panel);
    d->status->setObjectName(QStringLiteral("scheduleStatus"));
    d->status->setFixedHeight(33);
    d->status->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(d->status);
    connect(details, &QPushButton::clicked, this, &SchedulePreviewWidget::showConditions);
    const auto editPreviewTime = [this] {
        // Keep wall-clock fields intact, including a nonexistent DST time.
        setPreviewDateTime(QDateTime(d->date->date(), d->time->time(), QTimeZone::UTC));
    };
    connect(d->date, &QDateEdit::dateChanged, this, editPreviewTime);
    connect(d->time, &QTimeEdit::timeChanged, this, editPreviewTime);
    connect(d->days, &QButtonGroup::idClicked, this, [this](int day) {
        const QDate date = d->date->date().addDays(day + 1 - d->date->date().dayOfWeek());
        if (date == QDateTime::currentDateTimeUtc().toTimeZone(previewZone(d->document)).date())
            showCurrentTime();
        else
            d->date->setDate(date);
    });
    connect(d->documentPreview, &ScheduleDocumentPreview::playlistSelected, this, [this](const QString &id) {
        const int row = d->channelRow(id);
        if (row < 0) return;
        setSelectedRow(row);
        emit selectedRowChanged(row);
    });
    connect(d->documentPreview, &ScheduleDocumentPreview::playlistEditRequested, this, [this](const QString &id) {
        const int row = d->channelRow(id);
        if (row >= 0) emit editRequested(row);
    });
    auto *clock = new QTimer(this);
    clock->setObjectName(QStringLiteral("scheduleClock"));
    clock->setInterval(60 * 1000);
    clock->setTimerType(Qt::PreciseTimer);
    connect(clock, &QTimer::timeout, this, [this] {
        if (d->followingCurrentTime) showCurrentTime();
    });
    clock->start();
    refresh();
}

SchedulePreviewWidget::~SchedulePreviewWidget()
{
    for (const auto &connection : std::as_const(d->connections)) disconnect(connection);
    delete d;
}

void SchedulePreviewWidget::setModels(QAbstractItemModel *channels, QAbstractItemModel *adverts)
{
    for (const auto &connection : std::as_const(d->connections)) disconnect(connection);
    d->connections.clear();
    d->channels = channels;
    d->selected = -1;
    d->selectedName.clear();
    d->selectedId.clear();
    d->selectedSource = QPersistentModelIndex();
    // Models provide channel identity/selection only. The compiled document is
    // always the source of the plan, including after edits to channel settings.
    for (auto *model : {channels, adverts}) {
        if (!model) continue;
        d->connections.append(connect(model, &QAbstractItemModel::dataChanged, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QAbstractItemModel::modelReset, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QAbstractItemModel::rowsInserted, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QAbstractItemModel::rowsRemoved, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QAbstractItemModel::layoutChanged, this, &SchedulePreviewWidget::refresh));
        d->connections.append(connect(model, &QObject::destroyed, this, &SchedulePreviewWidget::refresh));
    }
    refresh();
}

void SchedulePreviewWidget::setDocument(const QJsonObject &document)
{
    if (!d->compiledDocument.compiled || !d->documentError.isEmpty() || d->document != document) {
        d->compiledDocument = {};
        d->documentError = ScheduleV1::decode(document, &d->compiledDocument);
        // Keep the current draft available for diagnostic drawing. A rejected
        // draft has no compiled plan and must never reuse the previous plan.
        if (!d->documentError.isEmpty()) d->compiledDocument.object = document;
    }
    d->document = document;
    if (d->followingCurrentTime) showCurrentTime();
    else refresh();
}

void SchedulePreviewWidget::setDocumentError(const QString &error)
{
    d->document = {};
    d->compiledDocument = {};
    d->documentError = error.trimmed().isEmpty() ? tr("Не удалось обновить расписание.") : error;
    refresh();
}

void SchedulePreviewWidget::clearDocument()
{
    d->document = {};
    d->compiledDocument = {};
    d->documentError.clear();
    d->documentDescription.clear();
    if (d->followingCurrentTime) showCurrentTime();
    else refresh();
}

void SchedulePreviewWidget::setSelectedRow(int row)
{
    d->selected = d->channels && row >= 0 && row < d->channels->rowCount() ? row : -1;
    d->selectedSource = d->channels && d->selected >= 0 ? d->channels->index(d->selected, 0) : QModelIndex();
    d->selectedName = d->selectedSource.isValid() ? field(d->channels, d->selected, 0).toString() : QString();
    d->selectedId = d->selectedSource.data(ChannelModel::RuleIdRole).toString();
    d->documentPreview->selectPlaylist(d->selectedId);
}

int SchedulePreviewWidget::selectedRow() const { return d->selected; }
void SchedulePreviewWidget::setPreviewDateTime(const QDateTime &dateTime)
{
    if (!dateTime.isValid()) return;
    d->followingCurrentTime = false;
    const QSignalBlocker dateBlocker(d->date), timeBlocker(d->time);
    d->date->setDate(dateTime.date());
    d->time->setTime(dateTime.time());
    refresh();
}
void SchedulePreviewWidget::showCurrentTime()
{
    d->followingCurrentTime = true;
    const QDateTime now = QDateTime::currentDateTimeUtc().toTimeZone(previewZone(d->document));
    const QSignalBlocker dateBlocker(d->date), timeBlocker(d->time);
    d->date->setDate(now.date());
    d->time->setTime(now.time());
    refresh();
}
QDateTime SchedulePreviewWidget::previewDateTime() const
{
    return QDateTime(d->date->date(), d->time->time(), previewZone(d->document));
}
const SchedulePreview::Snapshot &SchedulePreviewWidget::snapshot() const { return d->plan; }
QSize SchedulePreviewWidget::sizeHint() const { return QSize(1177, 349); }

void SchedulePreviewWidget::refresh()
{
    QList<QPair<QString, QString>> playlists;
    for (int row = 0; d->channels && row < d->channels->rowCount(); ++row) {
        const auto id = d->channels->index(row, 0).data(ChannelModel::RuleIdRole).toString();
        if (!id.isEmpty()) playlists.append({id, field(d->channels, row, 0).toString()});
    }
    d->documentPreview->setChannelPlaylists(playlists);
    const QDateTime wallTime(d->date->date(), d->time->time(), QTimeZone::UTC);
    d->documentPreview->setPlan(d->compiledDocument, wallTime);
    d->plan = d->documentPreview->snapshot();
    d->documentDescription = d->documentError.isEmpty()
            ? ScheduleDocumentUi::describe(d->compiledDocument, wallTime)
            : tr("Расписание требует исправления:\n") + d->documentError;
    if (!d->documentError.isEmpty()) {
        d->plan = {};
        d->plan.at = previewDateTime();
        d->plan.issues = {d->documentError};
        d->plan.hasUnresolvedRules = true;
    }
    if (d->plan.hasUnresolvedRules) {
        d->plan.currentSummary = tr("Расчёт недоступен: %1").arg(d->plan.issues.value(0));
        d->plan.nextChannelSummary = tr("Расчёт смен недоступен");
        d->plan.nextAdvertSummary = tr("Расчёт вставок недоступен");
    } else {
        // Sidebar badges use the same resolved rules as the grid and player.
        const auto sources = [](const ScheduleV1::Evaluation &plan) {
            QSet<QString> ids;
            if (plan.silence) return ids;
            if (plan.mixRuleId.isEmpty()) ids.insert(plan.playlistId);
            else for (const auto &value : plan.pattern) {
                const auto source = value.toObject();
                ids.insert(source.value("type") == QJsonValue("active_base")
                           ? plan.playlistId : source.value("playlistId").toString());
            }
            return ids;
        };
        const auto current = sources(ScheduleV1::evaluate(d->compiledDocument, d->plan.at));
        const auto zone = previewZone(d->document);
        const auto intervals = ScheduleV1::intervals(d->compiledDocument,
                d->date->date().startOfDay(zone), d->date->date().addDays(1).startOfDay(zone));
        for (int row = 0; d->channels && row < d->channels->rowCount(); ++row) {
            const QString id = d->channels->index(row, 0).data(ChannelModel::RuleIdRole).toString();
            if (id.isEmpty()) continue;
            ScheduleCore::Channel channel;
            channel.sourceRow = row;
            channel.name = field(d->channels, row, 0).toString();
            channel.valid = true;
            channel.active = current.contains(id);
            bool later = false;
            for (const auto &interval : intervals) {
                if (!sources(interval.plan).contains(id)) continue;
                channel.calendarMatches = true;
                later = later || interval.from > d->plan.at;
            }
            channel.status = channel.active ? tr("По плану сейчас") : later ? tr("Позже")
                    : channel.calendarMatches ? tr("Уже завершён") : tr("Нет выхода");
            channel.reason = channel.status;
            d->plan.channels.append(channel);
            if (channel.active) d->plan.activeRows.append(row);
        }
    }
    if (d->selectedSource.isValid()) d->selected = d->selectedSource.row();
    else if (!d->selectedId.isEmpty()) d->selected = d->channelRow(d->selectedId);
    else if (!d->selectedName.isEmpty()) {
        d->selected = -1;
        for (int row = 0; d->channels && row < d->channels->rowCount(); ++row)
            if (field(d->channels, row, 0).toString() == d->selectedName) { d->selected = row; break; }
    }
    const auto previousId = d->selectedId;
    d->selectedSource = d->channels && d->selected >= 0 ? d->channels->index(d->selected, 0) : QModelIndex();
    d->selectedName = d->selectedSource.isValid() ? field(d->channels, d->selected, 0).toString() : QString();
    d->selectedId = d->selectedSource.data(ChannelModel::RuleIdRole).toString();
    if (previousId != d->selectedId || !d->documentPreview->hasSelection())
        d->documentPreview->selectPlaylist(d->selectedId);
    const QDate monday = d->date->date().addDays(1 - d->date->date().dayOfWeek());
    const QDate today = QDateTime::currentDateTimeUtc().toTimeZone(previewZone(d->document)).date();
    const QStringList days{tr("Пн"), tr("Вт"), tr("Ср"), tr("Чт"), tr("Пт"), tr("Сб"), tr("Вс")};
    for (int day = 0; day < 7; ++day) {
        const QDate date = monday.addDays(day);
        auto *button = d->dayButtons.at(day);
        button->setText(days.at(day) + QStringLiteral(" %1").arg(date.day()));
        button->setToolTip(date.toString(QStringLiteral("dd.MM.yyyy"))
                          + (date == today ? tr(" · Текущее время, обновление раз в минуту") : QString()));
        button->setAccessibleName(days.at(day) + QStringLiteral(" ") + date.toString(QStringLiteral("dd.MM.yyyy")));
        button->setChecked(date == d->date->date());
    }
    QString status = d->plan.issues.isEmpty() ? d->plan.currentSummary : d->plan.issues.first();
    if (d->plan.issues.size() > 1) status += tr(" · ещё %1").arg(d->plan.issues.size() - 1);
    const QString zoneName = d->document.value(QStringLiteral("timeZone")).toString();
    if (!zoneName.isEmpty()) status += QStringLiteral(" · ") + zoneName;
    d->date->setToolTip(zoneName.isEmpty() ? QString() : tr("Дата и время в часовом поясе %1").arg(zoneName));
    d->time->setToolTip(d->date->toolTip());
    d->status->setColorRole(d->plan.issues.isEmpty() ? QStringLiteral("secondary") : QStringLiteral("warning"));
    d->status->setProperty("scheduleIssue", !d->plan.issues.isEmpty());
    d->status->setText(d->status->fontMetrics().elidedText(status, Qt::ElideRight, std::max(350, width() - 40)));
    d->status->setToolTip(status);
    emit snapshotChanged();
}

void SchedulePreviewWidget::showConditions()
{
    QStringList lines{d->documentDescription};
    if (d->documentError.isEmpty() && d->compiledDocument.compiled)
        lines << tr("Срок действия и часовой пояс берутся из настроек расписания.");
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
