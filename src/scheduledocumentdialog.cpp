#include "scheduledocumentdialog.h"
#include "schedulecore/schedulev1.h"
#include "restyletheme.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimeZone>
#include <QToolButton>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>
#include <utility>

namespace {
const QStringList sectionKeys{QString(), QStringLiteral("assets"), QStringLiteral("playlists"),
    QStringLiteral("calendars"), QStringLiteral("dayTemplates"), QStringLiteral("baseRules"),
    QStringLiteral("mixRules"), QStringLiteral("eventRules")};

QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString titleFor(const QJsonObject &item)
{
    return item.value("name").toString(item.value("path").toString(item.value("id").toString()));
}
QString entityName(const QJsonObject &object, const QString &key, const QString &id)
{
    for (const auto value : object.value(key).toArray()) {
        const auto item = value.toObject();
        if (item.value("id").toString() == id) return titleFor(item);
    }
    return id;
}
QString firstId(const QJsonObject &object, const QString &key)
{
    const auto array = object.value(key).toArray();
    return array.isEmpty() ? QString() : array.first().toObject().value("id").toString();
}
QJsonObject allDays()
{
    return {{"select", QJsonObject{{"type", "all"}}}, {"excludeDates", QJsonArray{}}};
}
QJsonObject fullDay()
{
    return {{"from", "00:00:00"}, {"until", "00:00:00"}, {"untilDayOffset", 1}};
}
}

QString ScheduleDocumentUi::describe(const QJsonObject &object, const QDateTime &at)
{
    ScheduleV1::Document document;
    const QString error = ScheduleV1::decode(object, &document);
    if (!error.isEmpty()) return QStringLiteral("Расписание требует исправления:\n") + error;
    return describe(document, at);
}

QString ScheduleDocumentUi::describe(const ScheduleV1::Document &document, const QDateTime &at)
{
    if (!document.compiled) return QStringLiteral("Расписание не проверено.");
    const auto &object = document.object;
    const QTimeZone zone(object.value("timeZone").toString().toUtf8());
    const QDateTime local(at.date(), at.time(), zone, QDateTime::TransitionResolution::PreferBefore);
    if (!local.isValid() || local.date() != at.date() || local.time() != at.time())
        return QStringLiteral("Выбранное местное время отсутствует при переводе часов.");
    const auto plan = ScheduleV1::evaluate(document, local);
    QStringList lines{QStringLiteral("%1 · %2").arg(local.toString("dd.MM.yyyy HH:mm:ss"), QString::fromUtf8(zone.id()))};
    if (!plan.withinValidity)
        lines << QStringLiteral("Вне срока действия расписания. Применяется резервный источник.");
    lines << (plan.usingFallback ? QStringLiteral("Резервный источник")
              : QStringLiteral("Базовое правило: ") + entityName(object, "baseRules", plan.baseRuleId));
    lines << (plan.silence ? QStringLiteral("Тишина")
              : QStringLiteral("Плейлист: ") + entityName(object, "playlists", plan.playlistId));
    lines << QStringLiteral("Громкость: %1%").arg(plan.volumePercent);
    if (!plan.mixRuleId.isEmpty()) {
        lines << QStringLiteral("Подмешивание: ") + entityName(object, "mixRules", plan.mixRuleId);
        QStringList pattern;
        for (const auto value : plan.pattern) {
            const auto source = value.toObject();
            pattern << (source.value("type") == "active_base" ? QStringLiteral("Основной")
                         : entityName(object, "playlists", source.value("playlistId").toString()));
        }
        lines << QStringLiteral("Чередование: ") + pattern.join(QStringLiteral(" → "));
    }
    auto events = ScheduleV1::events(document, local, local.addDays(1));
    std::sort(events.begin(), events.end(), [](const auto &left, const auto &right) {
        if (left.scheduledUtc != right.scheduledUtc) return left.scheduledUtc < right.scheduledUtc;
        if (left.priority != right.priority) return left.priority > right.priority;
        return left.ruleId < right.ruleId;
    });
    if (events.isEmpty()) lines << QStringLiteral("В ближайшие сутки отдельных вставок не запланировано.");
    else {
        lines << QStringLiteral("Ближайшие вставки по времени:");
        for (qsizetype i = 0; i < qMin(qsizetype(8), events.size()); ++i)
            lines << QStringLiteral("%1 · %2").arg(events.at(i).scheduledUtc.toTimeZone(zone).toString("dd.MM HH:mm:ss"),
                        entityName(object, "eventRules", events.at(i).ruleId));
    }
    lines << plan.diagnostics;
    lines << QStringLiteral("Это расчёт плана. Фактическое воспроизведение подтверждает плеер.");
    return lines.join('\n');
}

struct ScheduledDocumentDialog::Private {
    QString mediaType;
    QJsonObject object;
    ScheduleV1::Document compiled;
    QComboBox *section = nullptr;
    QListWidget *items = nullptr;
    QPlainTextEdit *editor = nullptr, *preview = nullptr;
    QLabel *error = nullptr, *help = nullptr;
    QDateTimeEdit *at = nullptr;
    QPushButton *add = nullptr, *remove = nullptr;
    QLineEdit *holidayName = nullptr;
    QComboBox *holidayPlaylist = nullptr;
    QDateEdit *holidayFrom = nullptr, *holidayUntil = nullptr;
    QListWidget *holidayRules = nullptr;
    QPushButton *holidayAdd = nullptr, *holidayRemove = nullptr;
    int sectionIndex = 0, row = 0;
    bool loading = false;

    void refreshHolidayRules()
    {
        const QString selected = holidayPlaylist->currentData().toString();
        holidayPlaylist->clear();
        for (const auto value : object.value("playlists").toArray()) {
            const auto playlist = value.toObject();
            holidayPlaylist->addItem(titleFor(playlist), playlist.value("id").toString());
        }
        const int index = holidayPlaylist->findData(selected);
        if (index >= 0) holidayPlaylist->setCurrentIndex(index);
        holidayAdd->setEnabled(holidayPlaylist->count() > 0);
        holidayPlaylist->setToolTip(holidayPlaylist->count() > 0 ? QString()
            : QStringLiteral("Сначала добавьте канал с медиафайлами или плейлист в подробных настройках."));
        holidayRules->clear();
        for (const auto value : object.value("mixRules").toArray()) {
            const auto rule = value.toObject();
            QString description = titleFor(rule);
            const auto range = rule.value("when").toObject().value("range").toObject();
            if (!range.isEmpty()) {
                const auto from = QDate::fromString(range.value("from").toString(), Qt::ISODate);
                const auto until = QDate::fromString(range.value("until").toString(), Qt::ISODate).addDays(-1);
                description += QStringLiteral(" · %1 — %2").arg(from.toString("dd.MM.yyyy"), until.toString("dd.MM.yyyy"));
            }
            if (!rule.value("enabled").toBool()) description += QStringLiteral(" · выключено");
            auto *item = new QListWidgetItem(description, holidayRules);
            item->setData(Qt::UserRole, rule.value("id").toString());
        }
        holidayRules->setVisible(holidayRules->count() > 0);
        holidayRemove->setVisible(holidayRules->count() > 0);
        holidayRemove->setEnabled(false);
    }

    bool addHolidayRule()
    {
        if (!commit()) return false;
        const auto name = holidayName->text().trimmed();
        if (name.isEmpty()) {
            error->setText(QStringLiteral("Введите название чередования."));
            holidayName->setFocus();
            return false;
        }
        const auto from = holidayFrom->date(), last = holidayUntil->date();
        const auto validity = object.value("validity").toObject();
        const auto firstValid = QDate::fromString(validity.value("from").toString(), Qt::ISODate);
        const auto untilValid = QDate::fromString(validity.value("until").toString(), Qt::ISODate);
        if (last < from) {
            error->setText(QStringLiteral("Дата окончания должна быть не раньше даты начала."));
            return false;
        }
        if (from < firstValid || last >= untilValid) {
            error->setText(QStringLiteral("Период должен входить в срок расписания: %1 — %2. Срок можно изменить в подробных настройках.")
                .arg(firstValid.toString("dd.MM.yyyy"), untilValid.addDays(-1).toString("dd.MM.yyyy")));
            return false;
        }
        const auto playlistId = holidayPlaylist->currentData().toString();
        if (playlistId.isEmpty()) {
            error->setText(QStringLiteral("Выберите дополнительный плейлист."));
            return false;
        }
        auto condition = allDays();
        condition.insert("range", QJsonObject{{"from", from.toString(Qt::ISODate)},
                                            {"until", last.addDays(1).toString(Qt::ISODate)}});
        const QJsonObject rule{{"id", newId()}, {"name", name}, {"enabled", true}, {"priority", 10},
            {"when", condition}, {"windows", QJsonArray{fullDay()}},
            {"pattern", QJsonArray{QJsonObject{{"type", "active_base"}},
                                   QJsonObject{{"type", "playlist"}, {"playlistId", playlistId}}}},
            {"emptyAdditionalSource", "use_base"}};
        auto candidate = object;
        auto rules = candidate.value("mixRules").toArray();
        rules.append(rule);
        candidate.insert("mixRules", rules);
        candidate.insert("requiredCapabilities", QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(candidate)));
        ScheduleV1::Document checked;
        const auto validationError = ScheduleV1::decode(candidate, &checked);
        if (!validationError.isEmpty()) {
            error->setText(validationError);
            return false;
        }
        object = candidate;
        compiled = std::move(checked);
        loadSection(sectionIndex);
        refreshHolidayRules();
        holidayRules->setCurrentRow(holidayRules->count() - 1);
        at->setDateTime(QDateTime(from, QTime(12, 0)));
        preview->setPlainText(ScheduleDocumentUi::describe(compiled, at->dateTime()));
        error->clear();
        return true;
    }

    bool commit()
    {
        if (loading || row < 0) return true;
        QJsonObject value;
        const QString parseError = ScheduleV1::strictJsonObject(editor->toPlainText().toUtf8(), &value);
        if (!parseError.isEmpty()) {
            error->setText(QStringLiteral("Исправьте JSON выбранного элемента: ") + parseError);
            return false;
        }
        if (sectionIndex == 0) {
            for (int i = 1; i < sectionKeys.size(); ++i) value.insert(sectionKeys[i], object.value(sectionKeys[i]));
            object = value;
        } else {
            const QString key = sectionKeys[sectionIndex];
            QJsonArray array = object.value(key).toArray();
            if (row >= array.size()) return false;
            array.replace(row, value);
            object.insert(key, array);
            if (auto *item = items->item(row)) item->setText(titleFor(value));
        }
        error->clear();
        return true;
    }

    void loadItem(int nextRow)
    {
        loading = true;
        row = nextRow;
        QJsonObject value;
        if (sectionIndex == 0) {
            value = object;
            for (int i = 1; i < sectionKeys.size(); ++i) value.remove(sectionKeys[i]);
        } else if (row >= 0) value = object.value(sectionKeys[sectionIndex]).toArray().at(row).toObject();
        editor->setPlainText(row >= 0 ? QString::fromUtf8(QJsonDocument(value).toJson(QJsonDocument::Indented)) : QString());
        editor->setEnabled(row >= 0);
        remove->setEnabled(sectionIndex > 0 && row >= 0);
        loading = false;
    }

    void loadSection(int index)
    {
        loading = true;
        sectionIndex = index;
        items->clear();
        if (index == 0) items->addItem(QStringLiteral("Параметры расписания"));
        else for (const auto value : object.value(sectionKeys[index]).toArray()) items->addItem(titleFor(value.toObject()));
        add->setEnabled(index > 0);
        static const QStringList hints{
            QStringLiteral("Часовой пояс timeZone, срок validity [from, until), резерв fallback. ID и ревизию выпуска обновляет публикация."),
            QStringLiteral("Медиафайлы: относительный путь path от медиакаталога станции. Ссылки из плейлистов и событий используют id."),
            QStringLiteral("Плейлисты: order — sequential или shuffle_cycle. Каждый элемент entries имеет собственный id и assetId."),
            QStringLiteral("Календарь: dates — конкретные даты YYYY-MM-DD. coverage должен охватывать весь срок расписания и соседний день для ночных правил."),
            QStringLiteral("Шаблоны дня: slots задают время, источник и громкость. Полные сутки: 00:00:00 → 00:00:00, untilDayOffset: 1."),
            QStringLiteral("Базовые правила: when.select — all, weekdays, dates, calendar или annual_filter. Побеждает больший priority; равные приоритеты конфликтуют."),
            QStringLiteral("Подмешивание: pattern — последовательность active_base и playlist. Для чередования через один оставьте два элемента. Окна windows относятся к дню начала."),
            QStringLiteral("События: times — точное местное время. delivery.start — after_track или interrupt; maxLateSeconds задаёт допустимую задержку.")};
        help->setText(hints.value(index));
        const int nextRow = items->count() ? 0 : -1;
        items->setCurrentRow(nextRow);
        loading = false;
        loadItem(nextRow);
    }

    QJsonObject newItem() const
    {
        const QJsonObject source{{"type", "playlist"}, {"playlistId", firstId(object, "playlists")}};
        QJsonObject item{{"id", newId()}, {"name", QStringLiteral("Новый элемент")}};
        switch (sectionIndex) {
        case 1: return {{"id", newId()},
            {"path", mediaType == "video" ? "video/channel/file.mp4" : "music/channel/file.mp3"},
            {"mediaType", mediaType}};
        case 2:
            item.insert("revision", 1); item.insert("order", "shuffle_cycle"); item.insert("entries", QJsonArray{}); break;
        case 3:
            item.insert("revision", 1); item.insert("coverage", object.value("validity")); item.insert("dates", QJsonArray{}); break;
        case 4:
            item.insert("slots", QJsonArray{QJsonObject{{"id", newId()}, {"window", fullDay()},
                           {"source", source}, {"volumePercent", 70}}}); break;
        default:
            item.insert("enabled", true); item.insert("priority", 10); item.insert("when", allDays());
            if (sectionIndex == 5) item.insert("templateId", firstId(object, "dayTemplates"));
            if (sectionIndex == 6) {
                item.insert("windows", QJsonArray{fullDay()});
                item.insert("pattern", QJsonArray{QJsonObject{{"type", "active_base"}}, source});
                item.insert("emptyAdditionalSource", "use_base");
            }
            if (sectionIndex == 7) {
                item.insert("times", QJsonArray{"12:00:00"});
                item.insert("action", QJsonObject{{"assetId", firstId(object, "assets")}, {"volumePercent", 75}});
                item.insert("delivery", QJsonObject{{"start", "after_track"}, {"maxLateSeconds", 300},
                            {"expired", "skip"}, {"after", "resume_music"}});
            }
        }
        return item;
    }

    QString validate()
    {
        if (!commit()) return error->text();
        object.insert("requiredCapabilities", QJsonArray::fromStringList(ScheduleV1::requiredCapabilities(object)));
        if (compiled.compiled && compiled.object == object) return {};
        return ScheduleV1::decode(object, &compiled);
    }
};

ScheduledDocumentDialog::ScheduledDocumentDialog(const QJsonObject &document, QWidget *parent,
                                               const QString &mediaType)
    : QDialog(parent), d(std::make_unique<Private>())
{
    d->object = document;
    d->mediaType = mediaType;
    setObjectName(QStringLiteral("scheduledDocumentDialog"));
    setWindowTitle(QStringLiteral("Настройки расписания"));
    setFont(Restyle::font(11));
    auto *layout = new QVBoxLayout(this);
    auto *headingRow = new QHBoxLayout;
    auto *heading = new QLabel(QStringLiteral("Настройки расписания"));
    heading->setFont(Restyle::font(20, QFont::DemiBold));
    headingRow->addWidget(heading, 1);
    auto *close = new QPushButton;
    close->setIcon(Restyle::icon("close"));
    close->setAccessibleName(QStringLiteral("Закрыть без сохранения"));
    Restyle::button(close, "icon");
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    headingRow->addWidget(close);
    layout->addLayout(headingRow);
    auto *intro = new QLabel(QStringLiteral("Добавляйте праздничное чередование и другие условия к обычной программе. Все правила работают в одном расписании и отображаются в общей сетке."));
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *holiday = new QGroupBox(QStringLiteral("Праздничное чередование 1:1"));
    auto *holidayLayout = new QVBoxLayout(holiday);
    auto *holidayHint = new QLabel(QStringLiteral("Основной трек → праздничный → основной → праздничный. Основной плейлист меняется по обычному расписанию. В период тишины чередование не включается."));
    holidayHint->setWordWrap(true);
    holidayLayout->addWidget(holidayHint);
    auto *holidayForm = new QGridLayout;
    d->holidayName = new QLineEdit;
    d->holidayName->setObjectName(QStringLiteral("scheduleHolidayName"));
    d->holidayName->setPlaceholderText(QStringLiteral("Например, Новогодняя музыка"));
    d->holidayPlaylist = new QComboBox;
    d->holidayPlaylist->setObjectName(QStringLiteral("scheduleHolidayPlaylist"));
    auto *nameLabel = new QLabel(QStringLiteral("Название"));
    nameLabel->setBuddy(d->holidayName);
    auto *playlistLabel = new QLabel(QStringLiteral("Дополнительный плейлист"));
    playlistLabel->setBuddy(d->holidayPlaylist);
    holidayForm->addWidget(nameLabel, 0, 0);
    holidayForm->addWidget(d->holidayName, 0, 1);
    holidayForm->addWidget(playlistLabel, 0, 2);
    holidayForm->addWidget(d->holidayPlaylist, 0, 3);
    d->holidayFrom = new QDateEdit;
    d->holidayUntil = new QDateEdit;
    d->holidayFrom->setObjectName(QStringLiteral("scheduleHolidayFrom"));
    d->holidayUntil->setObjectName(QStringLiteral("scheduleHolidayUntil"));
    const auto validity = document.value("validity").toObject();
    const auto firstValid = QDate::fromString(validity.value("from").toString(), Qt::ISODate);
    const auto lastValid = QDate::fromString(validity.value("until").toString(), Qt::ISODate).addDays(-1);
    QDate initialDate = QDate::currentDate();
    if (firstValid.isValid() && initialDate < firstValid) initialDate = firstValid;
    if (lastValid.isValid() && initialDate > lastValid) initialDate = lastValid;
    for (auto *date : {d->holidayFrom, d->holidayUntil}) {
        date->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
        date->setCalendarPopup(true);
        date->setDate(initialDate);
    }
    auto *fromLabel = new QLabel(QStringLiteral("С"));
    fromLabel->setBuddy(d->holidayFrom);
    auto *untilLabel = new QLabel(QStringLiteral("По включительно"));
    untilLabel->setBuddy(d->holidayUntil);
    holidayForm->addWidget(fromLabel, 1, 0);
    holidayForm->addWidget(d->holidayFrom, 1, 1);
    holidayForm->addWidget(untilLabel, 1, 2);
    holidayForm->addWidget(d->holidayUntil, 1, 3);
    holidayForm->setColumnStretch(1, 1);
    holidayForm->setColumnStretch(3, 1);
    holidayLayout->addLayout(holidayForm);
    d->holidayRules = new QListWidget;
    d->holidayRules->setObjectName(QStringLiteral("scheduleHolidayRules"));
    d->holidayRules->setAccessibleName(QStringLiteral("Настроенные чередования"));
    d->holidayRules->setMaximumHeight(76);
    holidayLayout->addWidget(d->holidayRules);
    auto *holidayActions = new QHBoxLayout;
    d->holidayRemove = new QPushButton(QStringLiteral("Удалить выбранное"));
    d->holidayRemove->setObjectName(QStringLiteral("scheduleHolidayRemove"));
    d->holidayAdd = new QPushButton(QStringLiteral("Добавить чередование"));
    d->holidayAdd->setObjectName(QStringLiteral("scheduleHolidayAdd"));
    holidayActions->addWidget(d->holidayRemove);
    holidayActions->addStretch();
    holidayActions->addWidget(d->holidayAdd);
    holidayLayout->addLayout(holidayActions);
    layout->addWidget(holiday);
    auto *details = new QToolButton;
    details->setObjectName(QStringLiteral("scheduleDocumentDetails"));
    details->setText(QStringLiteral("Подробные настройки"));
    details->setCheckable(true);
    details->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    details->setArrowType(Qt::RightArrow);
    layout->addWidget(details, 0, Qt::AlignLeft);
    auto *detailsPanel = new QWidget;
    detailsPanel->setObjectName(QStringLiteral("scheduleDocumentDetailsPanel"));
    auto *detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    auto *toolbar = new QHBoxLayout;
    auto *importButton = new QPushButton(QStringLiteral("Импорт JSON…"));
    auto *exportButton = new QPushButton(QStringLiteral("Экспорт JSON…"));
    d->section = new QComboBox;
    d->section->setObjectName(QStringLiteral("scheduleDocumentSection"));
    d->section->addItems({QStringLiteral("Общее"), QStringLiteral("Медиафайлы"), QStringLiteral("Плейлисты"),
         QStringLiteral("Календари"), QStringLiteral("Шаблоны дня"), QStringLiteral("Базовые правила"),
         QStringLiteral("Подмешивание"), QStringLiteral("События")});
    toolbar->addWidget(d->section, 1); toolbar->addWidget(importButton); toolbar->addWidget(exportButton);
    detailsLayout->addLayout(toolbar);
    d->help = new QLabel;
    d->help->setWordWrap(true);
    d->help->setMinimumHeight(42);
    detailsLayout->addWidget(d->help);
    auto *split = new QSplitter(Qt::Horizontal);
    auto *navigation = new QWidget;
    auto *nav = new QVBoxLayout(navigation);
    nav->setContentsMargins(0, 0, 0, 0);
    d->items = new QListWidget;
    d->items->setObjectName(QStringLiteral("scheduleDocumentItems"));
    nav->addWidget(d->items);
    auto *actions = new QHBoxLayout;
    d->add = new QPushButton(QStringLiteral("Добавить"));
    d->remove = new QPushButton(QStringLiteral("Удалить"));
    actions->addWidget(d->add); actions->addWidget(d->remove); nav->addLayout(actions);
    split->addWidget(navigation);
    d->editor = new QPlainTextEdit;
    d->editor->setObjectName(QStringLiteral("scheduleDocumentJson"));
    d->editor->setAccessibleName(QStringLiteral("JSON выбранного элемента расписания"));
    d->editor->setFont(Restyle::font(11));
    split->addWidget(d->editor);
    split->setStretchFactor(1, 2);
    split->setSizes({260, 600});
    split->setMinimumHeight(170);
    detailsLayout->addWidget(split, 1);
    layout->addWidget(detailsPanel, 2);
    detailsPanel->hide();
    connect(details, &QToolButton::toggled, this, [this, details, detailsPanel](bool expanded) {
        if (!expanded && !d->commit()) {
            const QSignalBlocker blocker(details);
            details->setChecked(true);
            return;
        }
        if (!expanded) d->refreshHolidayRules();
        detailsPanel->setVisible(expanded);
        details->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        resize((expanded ? QSize(1040, 900) : QSize(980, 660))
            .boundedTo(screen()->availableGeometry().size() - QSize(48, 64)));
    });
    auto *previewBar = new QHBoxLayout;
    previewBar->addWidget(new QLabel(QStringLiteral("Проверить план на дату:")));
    d->at = new QDateTimeEdit(QDateTime::currentDateTime());
    d->at->setObjectName(QStringLiteral("scheduleDocumentAt"));
    d->at->setDisplayFormat(QStringLiteral("dd.MM.yyyy HH:mm:ss"));
    d->at->setCalendarPopup(true);
    d->at->setMinimumWidth(180);
    previewBar->addWidget(d->at);
    auto *check = new QPushButton(QStringLiteral("Проверить и рассчитать"));
    check->setObjectName(QStringLiteral("scheduleDocumentCheck"));
    previewBar->addWidget(check); previewBar->addStretch();
    layout->addLayout(previewBar);
    d->preview = new QPlainTextEdit;
    d->preview->setObjectName(QStringLiteral("scheduleDocumentPreview"));
    d->preview->setReadOnly(true);
    d->preview->setMaximumHeight(160);
    layout->addWidget(d->preview, 1);
    d->error = new QLabel;
    d->error->setObjectName(QStringLiteral("scheduleDocumentError"));
    d->error->setWordWrap(true);
    QPalette errorPalette = d->error->palette();
    errorPalette.setColor(QPalette::WindowText, Restyle::tokens().error);
    d->error->setPalette(errorPalette);
    layout->addWidget(d->error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Сохранить расписание"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ScheduledDocumentDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(d->holidayAdd, &QPushButton::clicked, this, [this] { d->addHolidayRule(); });
    connect(d->holidayRules, &QListWidget::currentRowChanged, this, [this](int row) {
        d->holidayRemove->setEnabled(row >= 0);
    });
    connect(d->holidayRemove, &QPushButton::clicked, this, [this] {
        const auto *item = d->holidayRules->currentItem();
        if (!item || !d->commit()) return;
        const auto id = item->data(Qt::UserRole).toString();
        auto rules = d->object.value("mixRules").toArray();
        for (qsizetype i = 0; i < rules.size(); ++i) {
            if (rules.at(i).toObject().value("id").toString() == id) { rules.removeAt(i); break; }
        }
        d->object.insert("mixRules", rules);
        d->loadSection(d->sectionIndex);
        d->refreshHolidayRules();
        const auto error = d->validate();
        d->error->setText(error);
        d->preview->setPlainText(error.isEmpty() ? ScheduleDocumentUi::describe(d->compiled, d->at->dateTime()) : error);
    });
    connect(d->section, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!d->commit()) { const QSignalBlocker blocker(d->section); d->section->setCurrentIndex(d->sectionIndex); return; }
        d->loadSection(index);
        d->refreshHolidayRules();
    });
    connect(d->items, &QListWidget::currentRowChanged, this, [this](int row) {
        if (d->loading) return;
        if (!d->commit()) { const QSignalBlocker blocker(d->items); d->items->setCurrentRow(d->row); return; }
        d->loadItem(row);
    });
    connect(d->add, &QPushButton::clicked, this, [this] {
        if (!d->commit()) return;
        const auto item = d->newItem();
        auto array = d->object.value(sectionKeys[d->sectionIndex]).toArray();
        array.append(item); d->object.insert(sectionKeys[d->sectionIndex], array);
        d->loadSection(d->sectionIndex); d->items->setCurrentRow(int(array.size()) - 1);
        d->refreshHolidayRules();
    });
    connect(d->remove, &QPushButton::clicked, this, [this] {
        if (d->sectionIndex == 0 || d->row < 0) return;
        auto array = d->object.value(sectionKeys[d->sectionIndex]).toArray();
        array.removeAt(d->row); d->object.insert(sectionKeys[d->sectionIndex], array);
        d->loadSection(d->sectionIndex);
        d->refreshHolidayRules();
    });
    connect(check, &QPushButton::clicked, this, [this] {
        const QString error = d->validate();
        d->error->setText(error);
        d->preview->setPlainText(error.isEmpty() ? ScheduleDocumentUi::describe(d->compiled, d->at->dateTime()) : error);
        if (error.isEmpty()) d->refreshHolidayRules();
    });
    connect(importButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Импорт расписания"), {}, QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { d->error->setText(file.errorString()); return; }
        ScheduleV1::Document imported;
        const QString error = ScheduleV1::parse(file.readAll(), &imported);
        if (!error.isEmpty()) { d->error->setText(error); return; }
        d->object = imported.object;
        d->compiled = std::move(imported);
        d->error->clear(); d->loadSection(d->sectionIndex);
        d->refreshHolidayRules();
        d->preview->setPlainText(ScheduleDocumentUi::describe(d->compiled, d->at->dateTime()));
    });
    connect(exportButton, &QPushButton::clicked, this, [this] {
        const QString error = d->validate();
        if (!error.isEmpty()) { d->error->setText(error); return; }
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Экспорт расписания"), QStringLiteral("schedule.json"), QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) return;
        QSaveFile file(path); file.setDirectWriteFallback(false);
        const auto bytes = QJsonDocument(d->object).toJson(QJsonDocument::Indented);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) d->error->setText(file.errorString());
    });
    d->loadSection(0);
    d->refreshHolidayRules();
    const QString initialError = ScheduleV1::decode(document, &d->compiled);
    d->preview->setPlainText(initialError.isEmpty() ? ScheduleDocumentUi::describe(d->compiled, d->at->dateTime())
                                                   : QStringLiteral("Расписание требует исправления:\n") + initialError);
    resize(QSize(980, 660).boundedTo(screen()->availableGeometry().size() - QSize(48, 64)));
}

ScheduledDocumentDialog::~ScheduledDocumentDialog() = default;
QJsonObject ScheduledDocumentDialog::document() const { return d->object; }
void ScheduledDocumentDialog::accept()
{
    const QString error = d->validate();
    d->error->setText(error);
    if (error.isEmpty()) QDialog::accept();
}
