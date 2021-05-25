#include "delegatemonthedit.h"

#include <QDateTime>
using namespace lampproject::delegate;

DelegateMonthEdit::DelegateMonthEdit(QObject *parent):
    QStyledItemDelegate(parent)
{

}

QString DelegateMonthEdit::displayText ( const QVariant & value,
                                             const QLocale & locale ) const{
    QList<Month> months = DelegateMonthEdit::getMonth(value.toString());
    QString friendlyString;
    //QMap<DayOfWeek, QString> inLanguage;
    //all week
    if(months.count() == 12 ) {
        switch (locale.language()){
            case QLocale::Russian:
            return tr("Весь год");
            default:
            return tr("All year");
        }
    }
    //never
    if(months.isEmpty() ) {
        switch (locale.language()){
            case QLocale::Russian:
                return tr("Никогда");
            default:
            return tr("Never");
        }
    }
    foreach (Month month, months) {
        friendlyString += QString("%1, ").arg(QDate::shortMonthName(month));
    }
    if(!friendlyString.isEmpty())
        friendlyString.remove(friendlyString.count()-2,2);

    return friendlyString;
}
QWidget* DelegateMonthEdit::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                      const QModelIndex &index) const{
    Q_UNUSED(option)
    Q_UNUSED(index)
    DelegateMonthWidget *editor = new DelegateMonthWidget(parent);
#ifdef __APPLE__
    editor->setFocusPolicy(Qt::StrongFocus);
#endif
    editor->setAutoFillBackground(true);
    return editor;
}

void DelegateMonthEdit::setEditorData(QWidget *_editor, const QModelIndex &index) const{
    QString value = index.model()->data(index, Qt::EditRole).toString();
    DelegateMonthWidget * editor = static_cast<DelegateMonthWidget*>(_editor);
    editor->setValue(DelegateMonthEdit::getMonth(value));
}
void DelegateMonthEdit::setModelData(QWidget *_editor, QAbstractItemModel *model,
                  const QModelIndex &index) const{
    DelegateMonthWidget * editor = static_cast<DelegateMonthWidget*>(_editor);
    QList<Month> months = editor->getValue();
    if(months.count() == 12){
        model->setData(index,tr("*"));
    }
    QString value;
    foreach (Month month, months) {
        value += QString("%1,").arg(month);
    }
    if(!value.isEmpty())
        value.remove(value.count()-1,1);
    model->setData(index,value);
}

void DelegateMonthEdit::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(index)
    QRect rect = option.rect;
    QSize sizeHint = editor->sizeHint();

    if (rect.width()<sizeHint.width()) rect.setWidth(sizeHint.width());
    if (rect.height()<sizeHint.height()) rect.setHeight(sizeHint.height());

    editor->setGeometry(rect);
    editor->setBackgroundRole(QPalette::Window);
}
QList<Month> DelegateMonthEdit::getMonth(QString value) const{
    if(value.isEmpty())
        return QList<Month>();
    //try all day - "*"
    if(value.compare("*") == 0){
        //all week
        return QList<Month>()
                         << January   << February
                         << March     << April
                         << May       << June
                         << July      << August
                         << September << October
                         << November  << December;

    }
    //result
    QList<Month> result;
    //try get days
    QStringList monthString = value.split(",");
    if(monthString.isEmpty())
        return QList<Month>(); // string not contain days
    //
    foreach (QString sub, monthString) {
        bool ok;
        int month = sub.toInt(&ok, 10);
        if(ok && ((January <= month) && (month <= December))){
            result << static_cast<Month>(month);
        }
    }
    return result;
}
