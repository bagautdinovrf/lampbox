#include "delegatedaysoftheweek.h"
#include "ui_delegatedaysoftheweek.h"


using namespace lampproject;
using namespace lampproject::delegate;

DelegateDaysOfTheWeek::DelegateDaysOfTheWeek(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DelegateDaysOfTheWeek)
{
    ui->setupUi(this);
    //Dow = Button
    listButtons[Sunday] = ui->pbSunday;
    listButtons[Monday] = ui->pbMonday;
    listButtons[Tuesday] = ui->pbTuesday;
    listButtons[Wednesday] = ui->pbWednasday;
    listButtons[Thursday] = ui->pbThursday;
    listButtons[Friday] = ui->pbFriday;
    listButtons[Saturday] = ui->pbSaturday;
    setMinimumHeight(10);
    setMinimumWidth(10);
}

DelegateDaysOfTheWeek::~DelegateDaysOfTheWeek()
{
    delete ui;
}

bool DelegateDaysOfTheWeek::setValue(QList<DayOfWeek> dow){
    int ok = 0;
    foreach (DayOfWeek day, dow) {
        if((Sunday <= day) && (day <= Saturday)){
            listButtons[day]->setChecked(true);
            ++ok;
        }// else skip
    }
    return (bool)ok;
}

QList<DayOfWeek> DelegateDaysOfTheWeek::getValue(){
    QList<DayOfWeek> result;
    foreach (QPushButton* dayB, listButtons.values()) {
        if(dayB->isChecked()) {
            QMapIterator<DayOfWeek, QPushButton *> i(listButtons);
            while(i.findNext(dayB)) {
                 result << i.key();
            }
        }
    }
    return result;
}

DelegateDayOfWeekEdit::DelegateDayOfWeekEdit(QObject *parent):
    QStyledItemDelegate(parent)
{

}

QString DelegateDayOfWeekEdit::displayText ( const QVariant & value,
                                             const QLocale & locale ) const{
    QList<DayOfWeek> days = DelegateDayOfWeekEdit::getDays(value.toString());
    QString friendlyString;
    QMap<DayOfWeek, QString> inLanguage;
    //all week
    if(days.count() == 7 ) {
        switch (locale.language()){
            case QLocale::Russian:
                return tr("Каждый день");
            default:
            return tr("Every day");
        }
    }
    //never
    if(days.isEmpty() ) {
        switch (locale.language()){
            case QLocale::Russian:
                return tr("Никогда");
            default:
            return tr("Never");
        }
    }
    //switch by language for localize
    switch (locale.language()){
        case QLocale::Russian:
        inLanguage[Sunday] = tr("Вc");
        inLanguage[Monday] = tr("Пн");
        inLanguage[Tuesday] = tr("Вт");
        inLanguage[Wednesday] = tr("Ср");
        inLanguage[Thursday] = tr("Чт");
        inLanguage[Friday] = tr("Пт");
        inLanguage[Saturday] = tr("Сб");
        break;
        default:
            inLanguage[Sunday] = tr("Sun");
            inLanguage[Monday] = tr("Mon");
            inLanguage[Tuesday] = tr("Tue");
            inLanguage[Wednesday] = tr("Wed");
            inLanguage[Thursday] = tr("Thu");
            inLanguage[Friday] = tr("Fri");
            inLanguage[Saturday] = tr("Sat");
            break;
    }
    foreach (DayOfWeek day, days) {
        friendlyString += QString("%1, ").arg(inLanguage[day]);
    }
    if(!friendlyString.isEmpty())
        friendlyString.remove(friendlyString.count()-2,2);

    return friendlyString;
}
QWidget* DelegateDayOfWeekEdit::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(option);
    Q_UNUSED(index);
    DelegateDaysOfTheWeek *editor = new DelegateDaysOfTheWeek(parent);
#ifdef __APPLE__
    editor->setFocusPolicy(Qt::StrongFocus);
#endif
    editor->setAutoFillBackground(true);
    return editor;
}

void DelegateDayOfWeekEdit::setEditorData(QWidget *editor, const QModelIndex &index) const{
    QString value = index.model()->data(index, Qt::EditRole).toString();

    DelegateDaysOfTheWeek * dowEditor = static_cast<DelegateDaysOfTheWeek*>(editor);
//    DelegateDayOfWeekEdit* This = reinterpret_cast<DelegateDayOfWeekEdit*>(this);
    dowEditor->setValue(DelegateDayOfWeekEdit::getDays(value));
}
void DelegateDayOfWeekEdit::setModelData(QWidget *editor, QAbstractItemModel *model,
                  const QModelIndex &index) const{
    DelegateDaysOfTheWeek * dowEditor = static_cast<DelegateDaysOfTheWeek*>(editor);
    QList<DayOfWeek> days = dowEditor->getValue();
    if(days.count() == 7){
        model->setData(index,tr("*"));
    }
    QString value;
    foreach (DayOfWeek day, days) {
        value += QString("%1,").arg(day);
    }
    if(!value.isEmpty())
        value.remove(value.count()-1,1);
    model->setData(index,value);
    emit setDataMultiple(value);
}

void DelegateDayOfWeekEdit::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(index);
    QRect rect = option.rect;
    QSize sizeHint = editor->sizeHint();

    if (rect.width()<sizeHint.width()) rect.setWidth(sizeHint.width());
    if (rect.height()<sizeHint.height()) rect.setHeight(sizeHint.height());

    editor->setGeometry(rect);
    editor->setBackgroundRole(QPalette::Window);
}

QList<DayOfWeek> DelegateDayOfWeekEdit::getDays(QString value) const{
    if(value.isEmpty())
        return QList<DayOfWeek>();
    //try all day - "*"
    if(value.compare("*") == 0){
        //all week
        return QList<DayOfWeek>()
                         << Sunday   << Monday
                         << Tuesday  << Wednesday
                         << Thursday << Friday
                         << Saturday;

    }
    //result
    QList<DayOfWeek> result;
    //try get days
    QStringList dayString = value.split(",");
    if(dayString.isEmpty())
        return QList<DayOfWeek>(); // string not contain days
    //
    foreach (QString sub, dayString) {
        bool ok;
        int day = sub.toInt(&ok, 10);
        if(ok && ((Sunday <= day) && (day <= Saturday))){
            result << static_cast<DayOfWeek>(day);
        }
    }
    return result;
}
