#include "ruleeditors.h"
#include "restyletheme.h"

#include <QAbstractItemModel>
#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QEvent>
#include <QGridLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMessageBox>
#include <QPainter>
#include <QPersistentModelIndex>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QSet>
#include <QSlider>
#include <QSpinBox>
#include <QStyleOptionButton>
#include <QTimeEdit>
#include <QVBoxLayout>
#include <algorithm>

namespace RuleEditors {
namespace {

// Expand the existing editor's ranges before writing; a descending hour range
// wraps through midnight, while calendar ranges must remain ascending.
bool expand(const QString &text, int first, int last, QString *result,
            bool wildcard = true, bool empty = false, bool wrap = false)
{
    QString source = text.trimmed();
    source.replace(QChar(0x2013), QLatin1Char('-'));
    source.replace(QChar(0x2014), QLatin1Char('-'));
    if ((source == QLatin1String("*") && wildcard) || (source.isEmpty() && empty)) {
        if (result) *result = source;
        return true;
    }
    static const QRegularExpression item(QStringLiteral("^([0-9]{1,2})(?:\\s*-\\s*([0-9]{1,2}))?$"));
    QSet<int> values;
    for (const QString &part : source.split(QLatin1Char(','))) {
        const auto match = item.match(part.trimmed());
        if (!match.hasMatch()) return false;
        const int a = match.captured(1).toInt();
        const int b = match.captured(2).isEmpty() ? a : match.captured(2).toInt();
        if (a < first || a > last || b < first || b > last || (a > b && !wrap)) return false;
        if (a > b) {
            for (int i = a; i <= last; ++i) values.insert(i);
            for (int i = first; i <= b; ++i) values.insert(i);
        } else {
            for (int i = a; i <= b; ++i) values.insert(i);
        }
    }
    auto ordered = values.values();
    std::sort(ordered.begin(), ordered.end());
    QStringList list;
    for (int value : ordered) list << QString::number(value);
    if (result) *result = list.join(QLatin1Char(','));
    return !list.isEmpty();
}

bool exactMinutes(const QString &text, QString *result, bool stored)
{
    QString source = text.trimmed();
    if (stored) {
        // Require m on every stored item: a bare number is a frequency.
        static const QRegularExpression minute(QStringLiteral("^[0-9]{1,2}m$"));
        for (const QString &part : source.split(QLatin1Char(',')))
            if (!minute.match(part.trimmed()).hasMatch()) return false;
        source.remove(QLatin1Char('m'));
    }
    QString expanded;
    if (!expand(source, 0, 59, &expanded, false)) return false;
    QStringList list;
    for (const QString &item : expanded.split(QLatin1Char(',')))
        list << QStringLiteral("%1m").arg(item.toInt(), 2, 10, QLatin1Char('0'));
    if (result) *result = list.join(QLatin1Char(','));
    return true;
}

QString compact(const QString &value, int first, int last)
{
    QString normalized;
    if (!expand(value, first, last, &normalized) || normalized == QLatin1String("*")) return value;
    const QStringList values = normalized.split(QLatin1Char(','));
    QStringList result;
    for (int i = 0; i < values.size(); ++i) {
        const int start = values[i].toInt();
        int end = start;
        while (i + 1 < values.size() && values[i + 1].toInt() == end + 1) end = values[++i].toInt();
        result << (start == end ? QString::number(start) : QStringLiteral("%1–%2").arg(start).arg(end));
    }
    return result.join(QStringLiteral(", "));
}

QString validate(ChannelRuleValues &v, const QStringList &names = {})
{
    static const QRegularExpression name(QStringLiteral("^\\w{1,15}$"),
                                         QRegularExpression::UseUnicodePropertiesOption);
    if (!name.match(v.name).hasMatch())
        return QStringLiteral("Название: от 1 до 15 букв, цифр или знаков подчёркивания, без пробелов.");
    if (names.contains(v.name, Qt::CaseInsensitive)) return QStringLiteral("Канал с таким названием уже существует.");
    if (!v.start.isValid() || !v.end.isValid()) return QStringLiteral("Укажите корректное время начала и окончания.");
    if (!expand(v.weekdays, 0, 6, &v.weekdays)) return QStringLiteral("Выберите хотя бы один день недели.");
    if (!expand(v.days, 1, 31, &v.days)) return QStringLiteral("Укажите дни от 1 до 31: например, 1–15, 20, 25. * — все дни.");
    if (!expand(v.months, 1, 12, &v.months)) return QStringLiteral("Выберите хотя бы один месяц.");
    if (v.volume < 0 || v.volume > 100) return QStringLiteral("Громкость должна быть от 0 до 100%.");
    return {};
}

QString validate(AdvertRuleValues &v)
{
    if (v.fileName.isEmpty()) return QStringLiteral("Сначала выберите рекламный файл.");
    if (!expand(v.hours, 0, 23, &v.hours, true, false, true)) return QStringLiteral("Укажите часы от 0 до 23: например, 8–22 или 8, 12, 18.");
    if (v.minutes != QLatin1String("*")) {
        if (v.minutes.contains(QLatin1Char('m'))) {
            if (!exactMinutes(v.minutes, &v.minutes, true)) return QStringLiteral("Укажите минуты от 0 до 59: например, 00, 15, 30, 45.");
        } else {
            bool ok = false;
            const int frequency = v.minutes.toInt(&ok);
            if (!ok || frequency < 1 || frequency > 20) return QStringLiteral("Частота должна быть от 1 до 20 выходов в час.");
            v.minutes = QString::number(frequency);
        }
    }
    if (!expand(v.weekdays, 0, 6, &v.weekdays)) return QStringLiteral("Выберите хотя бы один день недели. Для отключения выхода выберите режим «Никогда».");
    if (!v.start.isValid() || !v.end.isValid()) return QStringLiteral("Укажите корректные даты начала и окончания.");
    if (v.end < v.start) return QStringLiteral("Дата окончания должна быть не раньше даты начала.");
    if (v.volume < 0 || v.volume > 100) return QStringLiteral("Громкость должна быть от 0 до 100%.");
    return {};
}

QWidget *field(const QString &label, QWidget *control, const QString &note = {})
{
    auto *w = new QWidget;
    auto *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(7);
    auto *caption = new QLabel(label);
    caption->setFont(Restyle::font(11));
    caption->setFixedHeight(16);
    caption->setForegroundRole(QPalette::PlaceholderText);
    caption->setBuddy(control);
    control->setAccessibleName(label);
    layout->addWidget(caption);
    layout->addWidget(control);
    if (!note.isEmpty()) {
        auto *hint = new QLabel(note);
        hint->setFont(Restyle::font(12));
        hint->setMinimumHeight(17);
        hint->setForegroundRole(QPalette::PlaceholderText);
        hint->setWordWrap(true);
        layout->addWidget(hint);
    }
    return w;
}

QLineEdit *line(const QString &value, const QString &name)
{
    auto *edit = new QLineEdit(value);
    edit->setObjectName(name);
    edit->setFont(Restyle::font(11));
    edit->setFixedHeight(35);
    return edit;
}

class CalendarButton final : public QPushButton {
public:
    using QPushButton::QPushButton;
protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
            click();
            event->accept();
            return;
        }
        if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right
            || event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
            const auto siblings = parentWidget()->findChildren<QPushButton *>(QString(), Qt::FindDirectChildrenOnly);
            const int current = siblings.indexOf(this);
            const int step = event->key() == Qt::Key_Left || event->key() == Qt::Key_Up ? -1 : 1;
            if (current >= 0 && !siblings.isEmpty())
                siblings[(current + step + siblings.size()) % siblings.size()]->setFocus(Qt::TabFocusReason);
            event->accept();
            return;
        }
        QPushButton::keyPressEvent(event);
    }
    void paintEvent(QPaintEvent *) override
    {
        const auto &t = Restyle::tokens();
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const QRectF bounds = QRectF(rect()).adjusted(.5, .5, -.5, -.5);
        const QColor fill = t.dark ? t.surface2 : t.relief ? QColor("#f4f7fa") : t.surface;
        p.setBrush(underMouse() ? Restyle::mix(t.accent, .04, fill) : fill);
        p.setPen(t.relief && !t.dark ? QColor(Qt::white) : t.line);
        p.drawRoundedRect(bounds, 8, 8);
        if (t.relief && isChecked()) {
            p.setPen(QPen(t.dark ? QColor(0, 0, 0, 65) : QColor(37, 61, 89, 20), 2));
            p.drawLine(QPointF(7, 2), QPointF(width() - 7, 2));
            p.drawLine(QPointF(2, 7), QPointF(2, height() - 7));
        }
        QStyleOptionButton check;
        check.initFrom(this);
        check.rect = QRect(11, (height() - 15) / 2, 15, 15);
        check.state.setFlag(QStyle::State_On, isChecked());
        check.state.setFlag(QStyle::State_Off, !isChecked());
        style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &check, &p, this);
        p.setFont(Restyle::font(11));
        p.setPen(isChecked() ? t.accentText : t.text);
        p.drawText(QRect(31, 0, width() - 39, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
        if (hasFocus()) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(t.focus, 2));
            p.drawRoundedRect(bounds.adjusted(2, 2, -2, -2), 6, 6);
        }
    }
};

class CalendarPicker final : public QWidget {
public:
    explicit CalendarPicker(QWidget *parent = nullptr) : QWidget(parent)
    {
        QSizePolicy policy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        policy.setHeightForWidth(true);
        setSizePolicy(policy);
    }
    QList<QPushButton *> buttons;
    int heightForWidth(int width) const override { return arrange(width, false); }
    QSize sizeHint() const override { return QSize(550, heightForWidth(550)); }
    QSize minimumSizeHint() const override { return QSize(80, 33); }
protected:
    void resizeEvent(QResizeEvent *) override { arrange(width(), true); }
private:
    int arrange(int width, bool move) const
    {
        int x = 0, y = 0;
        for (QPushButton *b : buttons) {
            if (x && x + b->width() > width) { x = 0; y += 38; }
            if (move) b->move(x, y);
            x += b->width() + 5;
        }
        return y + 33;
    }
};

QWidget *picker(const QString &value, bool months)
{
    const QStringList labels = months
        ? QStringList{QStringLiteral("Янв"), QStringLiteral("Фев"), QStringLiteral("Мар"), QStringLiteral("Апр"),
                      QStringLiteral("Май"), QStringLiteral("Июн"), QStringLiteral("Июл"), QStringLiteral("Авг"),
                      QStringLiteral("Сен"), QStringLiteral("Окт"), QStringLiteral("Ноя"), QStringLiteral("Дек")}
        : QStringList{QStringLiteral("Пн"), QStringLiteral("Вт"), QStringLiteral("Ср"), QStringLiteral("Чт"),
                      QStringLiteral("Пт"), QStringLiteral("Сб"), QStringLiteral("Вс")};
    QString normalized;
    const bool valid = expand(value, months ? 1 : 0, months ? 12 : 6, &normalized, true, true);
    const QStringList selected = normalized.split(QLatin1Char(','));
    auto *w = new CalendarPicker;
    w->setObjectName(months ? QStringLiteral("monthPicker") : QStringLiteral("weekdayPicker"));
    w->setProperty("invalidOriginal", valid ? QString() : value);
    for (int i = 0; i < labels.size(); ++i) {
        const int number = months ? i + 1 : (i + 1) % 7;
        auto *b = new CalendarButton(labels[i], w);
        b->setObjectName((months ? QStringLiteral("month") : QStringLiteral("weekday")) + QString::number(number));
        b->setProperty("calendarValue", number);
        b->setCheckable(true);
        b->setAutoDefault(false);
        b->setChecked(normalized == QLatin1String("*") || selected.contains(QString::number(number)));
        b->setAccessibleName(months ? QLocale(QLocale::Russian).standaloneMonthName(number) : QLocale(QLocale::Russian).standaloneDayName(number ? number : 7));
        Restyle::button(b, "day");
        b->setFont(Restyle::font(11));
        b->setFixedSize(b->fontMetrics().horizontalAdvance(labels[i]) + 41, 33);
        w->buttons << b;
        QObject::connect(b, &QPushButton::clicked, w, [w] { w->setProperty("invalidOriginal", QString()); });
    }
    return w;
}

QString pickerValue(QWidget *picker)
{
    const QString invalid = picker->property("invalidOriginal").toString();
    if (!invalid.isEmpty()) return invalid;
    QList<int> checked;
    const auto buttons = picker->findChildren<QPushButton *>();
    for (QPushButton *b : buttons)
        if (b->isChecked()) checked << b->property("calendarValue").toInt();
    if (checked.size() == buttons.size()) return QStringLiteral("*");
    std::sort(checked.begin(), checked.end());
    QStringList result;
    for (int v : checked) result << QString::number(v);
    return result.join(QLatin1Char(','));
}

QTimeEdit *timeEdit(const QTime &value, const QString &name)
{
    auto *edit = new QTimeEdit(value.isValid() ? value : QTime(0, 0));
    edit->setObjectName(name);
    edit->setDisplayFormat(QStringLiteral("HH:mm"));
    edit->setFont(Restyle::font(11));
    edit->setFixedHeight(35);
    edit->setProperty("invalidOriginal", !value.isValid());
    QObject::connect(edit, &QTimeEdit::timeChanged, edit, [edit] { edit->setProperty("invalidOriginal", false); });
    return edit;
}

QDateEdit *dateEdit(const QDate &value, const QString &name)
{
    auto *edit = new QDateEdit(value.isValid() ? value : QDate::currentDate());
    edit->setObjectName(name);
    edit->setDisplayFormat(QStringLiteral("dd.MM.yyyy"));
    edit->setCalendarPopup(true);
    edit->setFont(Restyle::font(11));
    edit->setFixedHeight(35);
    edit->setProperty("invalidOriginal", !value.isValid());
    QObject::connect(edit, &QDateEdit::dateChanged, edit, [edit] { edit->setProperty("invalidOriginal", false); });
    return edit;
}

struct Form {
    QGridLayout *grid;
    QHBoxLayout *actions;
    QLabel *error;
QSlider *volume = nullptr;
    QScrollArea *scroll;
};

class FormErrorLabel final : public QLabel {
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setFont(font());
        p.setPen(Restyle::tokens().error);
        p.drawText(contentsRect(), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextWordWrap, text());
    }
};

Form buildForm(QDialog *dialog, const QString &title, const QString &saveText)
{
    dialog->setWindowTitle(title);
    dialog->setWindowFlag(Qt::WindowContextHelpButtonHint, false);
    dialog->setModal(true);
    dialog->setFont(Restyle::font());
    Restyle::surface(dialog, "dialog");
    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(25, 25, 25, 25);
    layout->setSpacing(0);
    auto *headingRow = new QHBoxLayout;
    headingRow->setContentsMargins(0, 0, 0, 0);
    auto *heading = new QLabel(title);
    heading->setFont(Restyle::font(22, QFont::DemiBold, -.5));
    heading->setFixedHeight(29);
    headingRow->addWidget(heading, 1);
    auto *close = new QPushButton;
    close->setObjectName(QStringLiteral("closeRule"));
    close->setIcon(Restyle::icon("close"));
    close->setToolTip(QStringLiteral("Закрыть без сохранения"));
    close->setAccessibleName(QStringLiteral("Закрыть без сохранения"));
    close->setAutoDefault(true);
    Restyle::button(close, "icon");
    QObject::connect(close, &QPushButton::clicked, dialog, &QDialog::reject);
    headingRow->addWidget(close);
    layout->addLayout(headingRow);
    layout->addSpacing(22);
    auto *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto *content = new QWidget;
    content->setAutoFillBackground(false);
    auto *grid = new QGridLayout(content);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setHorizontalSpacing(17);
    grid->setVerticalSpacing(17);
    grid->setColumnStretch(0, 1);
    grid->setColumnStretch(1, 1);
    grid->setAlignment(Qt::AlignTop);
    scroll->setWidget(content);
    content->setAutoFillBackground(false);
    scroll->viewport()->setAutoFillBackground(false);
    layout->addWidget(scroll, 1);
    layout->addSpacing(14);
    auto *error = new FormErrorLabel;
    error->setObjectName(QStringLiteral("formError"));
    error->setAccessibleName(QStringLiteral("Ошибка проверки формы"));
    error->setWordWrap(true);
    error->setFont(Restyle::font(11));
    error->setMinimumHeight(18);
    auto errorPalette = error->palette();
    errorPalette.setColor(QPalette::WindowText, Restyle::tokens().error);
    error->setPalette(errorPalette);
    layout->addWidget(error);
    layout->addSpacing(22);
    auto *actions = new QHBoxLayout;
    actions->setSpacing(9);
    actions->addStretch();
    auto *cancel = new QPushButton(QStringLiteral("Отмена"));
    cancel->setObjectName(QStringLiteral("cancelRule"));
    cancel->setAutoDefault(true);
    auto *save = new QPushButton(saveText);
    save->setObjectName(QStringLiteral("saveRule"));
    save->setDefault(true);
    for (QPushButton *button : {cancel, save}) button->setFixedHeight(32);
    Restyle::button(save, "primary");
    QObject::connect(cancel, &QPushButton::clicked, dialog, &QDialog::reject);
    QObject::connect(save, &QPushButton::clicked, dialog, &QDialog::accept);
    actions->addWidget(cancel);
    actions->addWidget(save);
    layout->addLayout(actions);
    return {grid, actions, error, nullptr, scroll};
}

QWidget *volumeField(Form &form, int value)
{
    auto *w = new QWidget;
    auto *layout = new QVBoxLayout(w);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto *label = new QLabel(QStringLiteral("Громкость"));
    label->setFont(Restyle::font(11));
    label->setForegroundRole(QPalette::PlaceholderText);
    label->setFixedHeight(16);
    auto *output = new QLabel(QStringLiteral("%1%").arg(value));
    output->setFont(Restyle::font(11));
    output->setForegroundRole(QPalette::PlaceholderText);
    output->setFixedHeight(16);
    auto *slider = new QSlider(Qt::Horizontal);
    slider->setObjectName(QStringLiteral("ruleVolume"));
    slider->setRange(0, 100);
    slider->setValue(value);
    slider->setProperty("invalidOriginal", value < 0 || value > 100);
    slider->setAccessibleName(QStringLiteral("Громкость"));
    slider->setFixedHeight(Restyle::tokens().relief ? 17 : 26);
    label->setBuddy(slider);
    layout->addWidget(label);
    layout->addWidget(output);
    layout->addWidget(slider);
    QObject::connect(slider, &QSlider::valueChanged, output, [output](int v) {
        output->setText(QStringLiteral("%1%").arg(v));
    });
    QObject::connect(slider, &QSlider::valueChanged, slider, [slider] { slider->setProperty("invalidOriginal", false); });
    form.volume = slider;
    return w;
}

void sizeDialog(QDialog *dialog, Form &form, bool channel)
{
    form.volume->setFixedHeight(Restyle::tokens().relief ? 17 : 26);
    const QSize available = dialog->screen()->availableGeometry().size() - QSize(24, 60);
    const int targetHeight = (channel ? 677 : 641) + (Restyle::tokens().relief ? 0 : 9);
    dialog->resize(qMin(600, available.width()), qMin(targetHeight, available.height()));
    dialog->setMinimumSize(qMin(400, available.width()), qMin(380, available.height()));
    auto p = form.error->palette();
    p.setColor(QPalette::WindowText, Restyle::tokens().error);
    form.error->setPalette(p);
}

void centerDialog(QDialog *dialog)
{
    const QRect available = dialog->screen()->availableGeometry();
    const QPoint center = dialog->parentWidget()
        ? dialog->parentWidget()->mapToGlobal(dialog->parentWidget()->rect().center())
        : available.center();
    const QSize extent = dialog->frameGeometry().size();
    dialog->move(qBound(available.left(), center.x() - extent.width() / 2,
                        qMax(available.left(), available.right() - extent.width() + 1)),
                 qBound(available.top(), center.y() - extent.height() / 2,
                        qMax(available.top(), available.bottom() - extent.height() + 1)));
}

QStringList channelNames(QAbstractItemModel *model, int except)
{
    QStringList names;
    for (int row = 0; row < model->rowCount(); ++row)
        if (row != except) names << model->index(row, 0).data(Qt::EditRole).toString();
    return names;
}

QVariant read(QAbstractItemModel *model, int row, int col)
{
    return model->index(row, col).data(Qt::EditRole);
}

QTime readTime(const QVariant &value)
{
    return value.metaType().id() == QMetaType::QTime ? value.toTime()
           : QTime::fromString(value.toString(), QStringLiteral("HH:mm"));
}

bool apply(QAbstractItemModel *model, int row, const QList<QVariant> &values, int first)
{
    if (!model || row < 0 || row >= model->rowCount() || model->columnCount() < 7) return false;
    QList<QPersistentModelIndex> indices;
    QList<QVariant> old;
    for (int i = first; i < 7; ++i) {
        const QModelIndex index = model->index(row, i);
        if (!(model->flags(index) & Qt::ItemIsEditable)) return false;
        indices << QPersistentModelIndex(index);
        const QVariant previous = index.data(Qt::EditRole);
        old << (values[i].metaType().id() == QMetaType::QTime ? QVariant(readTime(previous)) : previous);
    }
    QList<int> changed;
    for (int i = 0; i < indices.size(); ++i) {
        const QVariant &next = values[i + first];
        if (old[i] == next || (next.metaType().id() == QMetaType::QTime && readTime(old[i]) == next.toTime())) continue;
        if (!indices[i].isValid() || !model->setData(indices[i], next, Qt::EditRole)) {
            // Models persist on setData. Restore successful writes if a later
            // field refuses the edit; a channel rename is attempted first.
            for (auto it = changed.crbegin(); it != changed.crend(); ++it)
                if (indices[*it].isValid()) model->setData(indices[*it], old[*it], Qt::EditRole);
            return false;
        }
        changed << i;
    }
    return true;
}

} // namespace

struct ChannelRuleDialog::Private {
    Form form;
    QStringList names;
    QLineEdit *name;
    QTimeEdit *start;
    QTimeEdit *end;
    QWidget *weekdays;
    QLineEdit *days;
    QWidget *months;
    bool remove = false;
};

ChannelRuleDialog::ChannelRuleDialog(const ChannelRuleValues &initial, const QStringList &names,
                                     QWidget *parent, bool creating)
    : QDialog(parent), d(std::make_unique<Private>())
{
    setObjectName(QStringLiteral("channelRuleDialog"));
    d->names = names;
    d->form = buildForm(this, creating ? QStringLiteral("Новый канал") : QStringLiteral("Расписание канала"), QStringLiteral("Сохранить"));
    d->name = line(initial.name, QStringLiteral("channelName"));
    d->name->setToolTip(QStringLiteral("От 1 до 15 букв, цифр или знаков подчёркивания, без пробелов."));
    d->start = timeEdit(initial.start, QStringLiteral("channelStart"));
    d->end = timeEdit(initial.end, QStringLiteral("channelEnd"));
    d->weekdays = picker(initial.weekdays, false);
    d->days = line(compact(initial.days, 1, 31), QStringLiteral("channelDays"));
    d->days->setPlaceholderText(QStringLiteral("1–31 или 1, 5, 10–20"));
    d->months = picker(initial.months, true);
    auto *grid = d->form.grid;
    grid->addWidget(field(QStringLiteral("Название канала"), d->name), 0, 0, 1, 2);
    grid->addWidget(field(QStringLiteral("Начало"), d->start), 1, 0);
    grid->addWidget(field(QStringLiteral("Окончание"), d->end), 1, 1);
    grid->addWidget(field(QStringLiteral("Дни недели"), d->weekdays), 2, 0, 1, 2);
    grid->addWidget(field(QStringLiteral("Дни месяца"), d->days,
                          QStringLiteral("Отдельные дни и диапазоны от 1 до 31. * — все дни.")), 3, 0, 1, 2);
    grid->addWidget(field(QStringLiteral("Месяцы"), d->months), 4, 0, 1, 2);
    grid->addWidget(volumeField(d->form, initial.volume), 5, 0, 1, 2);
    sizeDialog(this, d->form, true);
    d->name->setFocus();
}

ChannelRuleDialog::~ChannelRuleDialog() = default;

ChannelRuleValues ChannelRuleDialog::values() const
{
    return {d->name->text(), d->start->property("invalidOriginal").toBool() ? QTime() : d->start->time(),
            d->end->property("invalidOriginal").toBool() ? QTime() : d->end->time(),
            pickerValue(d->weekdays), d->days->text(), pickerValue(d->months),
            d->form.volume->property("invalidOriginal").toBool() ? -1 : d->form.volume->value()};
}

void ChannelRuleDialog::accept()
{
    auto v = values();
    const QString error = validate(v, d->names);
    d->form.error->setText(error);
    if (error.isEmpty()) QDialog::accept();
}

void ChannelRuleDialog::enableDelete()
{
    auto *remove = new QPushButton(QStringLiteral("Удалить канал"));
    remove->setObjectName(QStringLiteral("deleteChannel"));
    remove->setAutoDefault(true);
    remove->setFixedHeight(32);
    Restyle::button(remove, "danger");
    remove->setIcon(Restyle::icon("trash"));
    d->form.actions->insertWidget(1, remove);
    connect(remove, &QPushButton::clicked, this, [this] { d->remove = true; reject(); });
}

bool ChannelRuleDialog::deleteRequested() const { return d->remove; }

void ChannelRuleDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::ApplicationPaletteChange && d && d->form.volume) sizeDialog(this, d->form, true);
}

void ChannelRuleDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    centerDialog(this);
}

struct AdvertRuleDialog::Private {
    Form form;
    QString fileName;
    QLineEdit *hours;
    QComboBox *mode;
    QLineEdit *minutes;
    QSpinBox *frequency;
    QWidget *minuteField;
    QWidget *frequencyField;
    QWidget *weekdays;
    QDateEdit *start;
    QDateEdit *end;
    QString invalidMinutes;
};

AdvertRuleDialog::AdvertRuleDialog(const AdvertRuleValues &initial, QWidget *parent, bool creating)
    : QDialog(parent), d(std::make_unique<Private>())
{
    setObjectName(QStringLiteral("advertRuleDialog"));
    d->fileName = initial.fileName;
    if (initial.minutes != QLatin1String("*")) {
        bool frequencyOk = false;
        const int frequency = initial.minutes.toInt(&frequencyOk);
        if (!(frequencyOk && frequency >= 1 && frequency <= 20) && !exactMinutes(initial.minutes, nullptr, true))
            d->invalidMinutes = initial.minutes.isEmpty() ? QStringLiteral("invalidm") : initial.minutes;
    }
    d->form = buildForm(this, creating ? QStringLiteral("Добавить в расписание") : QStringLiteral("Рекламный выход"), QStringLiteral("Сохранить выход"));
    auto *file = line(initial.fileName, QStringLiteral("advertFile"));
    file->setReadOnly(true);
    d->hours = line(compact(initial.hours, 0, 23), QStringLiteral("advertHours"));
    d->hours->setPlaceholderText(QStringLiteral("8–22 или 8, 12, 18"));
    d->mode = new QComboBox;
    d->mode->setObjectName(QStringLiteral("advertMode"));
    d->mode->setFixedHeight(35);
    d->mode->setFont(Restyle::font(11));
    d->mode->addItem(QStringLiteral("В точные минуты"), QStringLiteral("exact"));
    d->mode->addItem(QStringLiteral("Несколько раз в час"), QStringLiteral("frequency"));
    d->mode->addItem(QStringLiteral("Никогда"), QStringLiteral("never"));
    QString minuteValue = initial.minutes;
    minuteValue.remove(QLatin1Char('m'));
    minuteValue = minuteValue.split(QLatin1Char(',')).join(QStringLiteral(", "));
    d->minutes = line(minuteValue, QStringLiteral("advertMinutes"));
    d->minutes->setPlaceholderText(QStringLiteral("00, 15, 30, 45"));
    d->frequency = new QSpinBox;
    d->frequency->setObjectName(QStringLiteral("advertFrequency"));
    d->frequency->setRange(1, 20);
    d->frequency->setValue(initial.minutes.toInt());
    d->frequency->setFixedHeight(35);
    d->frequency->setFont(Restyle::font(11));
    d->minuteField = field(QStringLiteral("Минуты"), d->minutes);
    d->frequencyField = field(QStringLiteral("Выходов в час"), d->frequency);
    d->weekdays = picker(initial.weekdays, false);
    d->start = dateEdit(initial.start, QStringLiteral("advertStart"));
    d->end = dateEdit(initial.end, QStringLiteral("advertEnd"));
    auto *grid = d->form.grid;
    grid->addWidget(field(QStringLiteral("Ролик"), file), 0, 0, 1, 2);
    grid->addWidget(field(QStringLiteral("Часы выхода"), d->hours,
                          QStringLiteral("0–23 — круглосуточно. Можно указать отдельные часы и диапазоны.")), 1, 0, 1, 2);
    grid->addWidget(field(QStringLiteral("Режим выхода"), d->mode), 2, 0);
    grid->addWidget(d->minuteField, 2, 1);
    grid->addWidget(d->frequencyField, 2, 1);
    grid->addWidget(field(QStringLiteral("Дни недели"), d->weekdays), 3, 0, 1, 2);
    grid->addWidget(field(QStringLiteral("Дата начала"), d->start), 4, 0);
    grid->addWidget(field(QStringLiteral("Дата окончания"), d->end), 4, 1);
    grid->addWidget(volumeField(d->form, initial.volume), 5, 0, 1, 2);
    const auto updateMode = [this] {
        d->minuteField->setVisible(d->mode->currentIndex() == 0);
        d->frequencyField->setVisible(d->mode->currentIndex() == 1);
    };
    connect(d->mode, &QComboBox::currentIndexChanged, this, updateMode);
    d->mode->setCurrentIndex(initial.minutes == QLatin1String("*") ? 2 : initial.minutes.contains(QLatin1Char('m')) ? 0 : 1);
    updateMode();
    const auto clearInvalid = [this] { d->invalidMinutes.clear(); };
    connect(d->mode, &QComboBox::currentIndexChanged, this, clearInvalid);
    connect(d->minutes, &QLineEdit::textChanged, this, clearInvalid);
    connect(d->frequency, &QSpinBox::valueChanged, this, clearInvalid);
    sizeDialog(this, d->form, false);
    d->hours->setFocus();
}

AdvertRuleDialog::~AdvertRuleDialog() = default;

AdvertRuleValues AdvertRuleDialog::values() const
{
    QString minutes;
    if (!d->invalidMinutes.isEmpty()) minutes = d->invalidMinutes;
    else if (d->mode->currentIndex() == 2) minutes = QStringLiteral("*");
    else if (d->mode->currentIndex() == 1) minutes = QString::number(d->frequency->value());
    else if (!exactMinutes(d->minutes->text(), &minutes, false)) minutes = QStringLiteral("invalidm");
    return {d->fileName, d->hours->text(), minutes, pickerValue(d->weekdays),
            d->start->property("invalidOriginal").toBool() ? QDate() : d->start->date(),
            d->end->property("invalidOriginal").toBool() ? QDate() : d->end->date(),
            d->form.volume->property("invalidOriginal").toBool() ? -1 : d->form.volume->value()};
}

void AdvertRuleDialog::accept()
{
    auto v = values();
    const QString error = validate(v);
    d->form.error->setText(error);
    if (error.isEmpty()) QDialog::accept();
}

void AdvertRuleDialog::changeEvent(QEvent *event)
{
    QDialog::changeEvent(event);
    if (event->type() == QEvent::ApplicationPaletteChange && d && d->form.volume) sizeDialog(this, d->form, false);
}

void AdvertRuleDialog::showEvent(QShowEvent *event)
{
    QDialog::showEvent(event);
    centerDialog(this);
}

std::optional<ChannelRuleValues> newChannel(QWidget *parent, const QStringList &existingNames)
{
    ChannelRuleDialog dialog(ChannelRuleValues{}, existingNames, parent, true);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    auto values = dialog.values();
    if (!validate(values, existingNames).isEmpty()) return std::nullopt;
    return values;
}

std::optional<AdvertRuleValues> newAdvert(const QString &fileName, QWidget *parent)
{
    AdvertRuleValues initial;
    initial.fileName = fileName;
    AdvertRuleDialog dialog(initial, parent, true);
    if (dialog.exec() != QDialog::Accepted) return std::nullopt;
    auto values = dialog.values();
    if (!validate(values).isEmpty()) return std::nullopt;
    return values;
}

bool applyChannel(QAbstractItemModel *model, int row, const ChannelRuleValues &values)
{
    if (!model) return false;
    auto v = values;
    if (!validate(v, channelNames(model, row)).isEmpty()) return false;
    return apply(model, row, {v.name, v.start, v.end, v.weekdays, v.days, v.months, v.volume}, 0);
}

bool applyAdvert(QAbstractItemModel *model, int row, const AdvertRuleValues &values)
{
    auto v = values;
    if (!model || !validate(v).isEmpty()) return false;
    if (row < 0 || row >= model->rowCount() || read(model, row, 0).toString() != v.fileName) return false;
    return apply(model, row, {v.fileName, v.hours, v.minutes, v.weekdays, v.start, v.end, v.volume}, 1);
}

bool editChannel(QAbstractItemModel *model, int row, QWidget *parent, const std::function<void()> &deleteAction)
{
    if (!model || row < 0 || row >= model->rowCount()) return false;
    QPersistentModelIndex target(model->index(row, 0));
    ChannelRuleValues initial{read(model, row, 0).toString(), readTime(read(model, row, 1)), readTime(read(model, row, 2)),
                              read(model, row, 3).toString(), read(model, row, 4).toString(), read(model, row, 5).toString(), read(model, row, 6).toInt()};
    ChannelRuleDialog dialog(initial, channelNames(model, row), parent);
    if (deleteAction) dialog.enableDelete();
    if (dialog.exec() != QDialog::Accepted) {
        if (dialog.deleteRequested()) deleteAction();
        return false;
    }
    if (target.isValid() && applyChannel(model, target.row(), dialog.values())) return true;
    QMessageBox::warning(parent, QStringLiteral("Расписание канала"),
                          QStringLiteral("Не удалось сохранить расписание. Проверьте название канала, доступ к его каталогу и доступность станции."));
    return false;
}

bool editAdvert(QAbstractItemModel *model, int row, QWidget *parent)
{
    if (!model || row < 0 || row >= model->rowCount()) return false;
    if (!(model->flags(model->index(row, 1)) & Qt::ItemIsEditable)) {
        QMessageBox::information(parent, QStringLiteral("Рекламный выход"), QStringLiteral("Рекламой сетевой станции управляют централизованно."));
        return false;
    }
    QPersistentModelIndex target(model->index(row, 0));
    AdvertRuleValues initial{read(model, row, 0).toString(), read(model, row, 1).toString(), read(model, row, 2).toString(),
                             read(model, row, 3).toString(), read(model, row, 4).toDate(), read(model, row, 5).toDate(), read(model, row, 6).toInt()};
    AdvertRuleDialog dialog(initial, parent);
    if (dialog.exec() != QDialog::Accepted) return false;
    if (target.isValid() && applyAdvert(model, target.row(), dialog.values())) return true;
    QMessageBox::warning(parent, QStringLiteral("Рекламный выход"), QStringLiteral("Не удалось сохранить рекламный выход. Проверьте доступность станции и права редактирования."));
    return false;
}

} // namespace RuleEditors
