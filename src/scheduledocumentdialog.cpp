#include "scheduledocumentdialog.h"
#include "schedulecore/schedulev1.h"
#include "restyletheme.h"

#include <QComboBox>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScreen>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTimeZone>
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
    if (!error.isEmpty()) return QStringLiteral("Проект требует исправления:\n") + error;
    return describe(document, at);
}

QString ScheduleDocumentUi::describe(const ScheduleV1::Document &document, const QDateTime &at)
{
    if (!document.compiled) return QStringLiteral("Проект расписания не проверен.");
    const auto &object = document.object;
    const QTimeZone zone(object.value("timeZone").toString().toUtf8());
    const QDateTime local(at.date(), at.time(), zone, QDateTime::TransitionResolution::PreferBefore);
    if (!local.isValid() || local.date() != at.date() || local.time() != at.time())
        return QStringLiteral("Выбранное местное время отсутствует при переводе часов.");
    const auto plan = ScheduleV1::evaluate(document, local);
    QStringList lines{QStringLiteral("%1 · %2").arg(local.toString("dd.MM.yyyy HH:mm:ss"), QString::fromUtf8(zone.id()))};
    if (!plan.withinValidity)
        lines << QStringLiteral("Вне срока действия проекта. Применяется резервный источник.");
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
    if (events.isEmpty()) lines << QStringLiteral("В ближайшие сутки событий нет.");
    else {
        lines << QStringLiteral("Ближайшие события:");
        for (qsizetype i = 0; i < qMin(qsizetype(8), events.size()); ++i)
            lines << QStringLiteral("%1 · %2").arg(events.at(i).scheduledUtc.toTimeZone(zone).toString("dd.MM HH:mm:ss"),
                        entityName(object, "eventRules", events.at(i).ruleId));
    }
    lines << plan.diagnostics;
    lines << QStringLiteral("Это расчёт плана. Фактическое воспроизведение подтверждает плеер.");
    return lines.join('\n');
}

struct ScheduledDocumentDialog::Private {
    QJsonObject object;
    ScheduleV1::Document compiled;
    QComboBox *section = nullptr;
    QListWidget *items = nullptr;
    QPlainTextEdit *editor = nullptr, *preview = nullptr;
    QLabel *error = nullptr, *help = nullptr;
    QDateTimeEdit *at = nullptr;
    QPushButton *add = nullptr, *remove = nullptr;
    int sectionIndex = 0, row = 0;
    bool loading = false;

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
        if (index == 0) items->addItem(QStringLiteral("Параметры проекта"));
        else for (const auto value : object.value(sectionKeys[index]).toArray()) items->addItem(titleFor(value.toObject()));
        add->setEnabled(index > 0);
        static const QStringList hints{
            QStringLiteral("Часовой пояс timeZone, срок validity [from, until), резерв fallback. ID и ревизию выпуска обновляет публикация."),
            QStringLiteral("Медиафайлы: относительный путь path от медиакаталога станции. Ссылки из плейлистов и событий используют id."),
            QStringLiteral("Плейлисты: order — sequential или shuffle_cycle. Каждый элемент entries имеет собственный id и assetId."),
            QStringLiteral("Календарь: dates — конкретные даты YYYY-MM-DD. coverage должен охватывать весь срок проекта и соседний день для ночных правил."),
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
        case 1: return {{"id", newId()}, {"path", "music/channel/file.mp3"}, {"mediaType", "audio"}};
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

ScheduledDocumentDialog::ScheduledDocumentDialog(const QJsonObject &document, QWidget *parent)
    : QDialog(parent), d(std::make_unique<Private>())
{
    d->object = document;
    setObjectName(QStringLiteral("scheduledDocumentDialog"));
    setWindowTitle(QStringLiteral("Проект расписания"));
    setFont(Restyle::font(11));
    auto *layout = new QVBoxLayout(this);
    auto *headingRow = new QHBoxLayout;
    auto *heading = new QLabel(QStringLiteral("Проект расписания"));
    heading->setFont(Restyle::font(20, QFont::DemiBold));
    headingRow->addWidget(heading, 1);
    auto *close = new QPushButton;
    close->setIcon(Restyle::icon("close"));
    close->setAccessibleName(QStringLiteral("Закрыть без сохранения"));
    Restyle::button(close, "icon");
    connect(close, &QPushButton::clicked, this, &QDialog::reject);
    headingRow->addWidget(close);
    layout->addLayout(headingRow);
    auto *intro = new QLabel(QStringLiteral("Календари, программа дня, подмешивание и события. После сохранения правила задаются здесь; вкладки каналов управляют медиатекой."));
    intro->setWordWrap(true);
    layout->addWidget(intro);
    auto *toolbar = new QHBoxLayout;
    auto *importButton = new QPushButton(QStringLiteral("Импорт JSON…"));
    auto *exportButton = new QPushButton(QStringLiteral("Экспорт JSON…"));
    d->section = new QComboBox;
    d->section->setObjectName(QStringLiteral("scheduleDocumentSection"));
    d->section->addItems({QStringLiteral("Общее"), QStringLiteral("Медиафайлы"), QStringLiteral("Плейлисты"),
         QStringLiteral("Календари"), QStringLiteral("Шаблоны дня"), QStringLiteral("Базовые правила"),
         QStringLiteral("Подмешивание"), QStringLiteral("События")});
    toolbar->addWidget(d->section, 1); toolbar->addWidget(importButton); toolbar->addWidget(exportButton);
    layout->addLayout(toolbar);
    d->help = new QLabel;
    d->help->setWordWrap(true);
    d->help->setMinimumHeight(42);
    layout->addWidget(d->help);
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
    layout->addWidget(split, 3);
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
    buttons->button(QDialogButtonBox::Save)->setText(QStringLiteral("Сохранить проект"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Отмена"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &ScheduledDocumentDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(d->section, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!d->commit()) { const QSignalBlocker blocker(d->section); d->section->setCurrentIndex(d->sectionIndex); return; }
        d->loadSection(index);
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
    });
    connect(d->remove, &QPushButton::clicked, this, [this] {
        if (d->sectionIndex == 0 || d->row < 0) return;
        auto array = d->object.value(sectionKeys[d->sectionIndex]).toArray();
        array.removeAt(d->row); d->object.insert(sectionKeys[d->sectionIndex], array);
        d->loadSection(d->sectionIndex);
    });
    connect(check, &QPushButton::clicked, this, [this] {
        const QString error = d->validate();
        d->error->setText(error);
        d->preview->setPlainText(error.isEmpty() ? ScheduleDocumentUi::describe(d->compiled, d->at->dateTime()) : error);
    });
    connect(importButton, &QPushButton::clicked, this, [this] {
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Импорт проекта расписания"), {}, QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) return;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) { d->error->setText(file.errorString()); return; }
        ScheduleV1::Document imported;
        const QString error = ScheduleV1::parse(file.readAll(), &imported);
        if (!error.isEmpty()) { d->error->setText(error); return; }
        d->object = imported.object;
        d->compiled = std::move(imported);
        d->error->clear(); d->loadSection(d->sectionIndex);
        d->preview->setPlainText(ScheduleDocumentUi::describe(d->compiled, d->at->dateTime()));
    });
    connect(exportButton, &QPushButton::clicked, this, [this] {
        const QString error = d->validate();
        if (!error.isEmpty()) { d->error->setText(error); return; }
        const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Экспорт проекта расписания"), QStringLiteral("schedule.json"), QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) return;
        QSaveFile file(path); file.setDirectWriteFallback(false);
        const auto bytes = QJsonDocument(d->object).toJson(QJsonDocument::Indented);
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) d->error->setText(file.errorString());
    });
    d->loadSection(0);
    const QString initialError = ScheduleV1::decode(document, &d->compiled);
    d->preview->setPlainText(initialError.isEmpty() ? ScheduleDocumentUi::describe(d->compiled, d->at->dateTime())
                                                   : QStringLiteral("Проект требует исправления:\n") + initialError);
    resize(QSize(1040, 820).boundedTo(screen()->availableGeometry().size() - QSize(48, 64)));
}

ScheduledDocumentDialog::~ScheduledDocumentDialog() = default;
QJsonObject ScheduledDocumentDialog::document() const { return d->object; }
void ScheduledDocumentDialog::accept()
{
    const QString error = d->validate();
    d->error->setText(error);
    if (error.isEmpty()) QDialog::accept();
}
