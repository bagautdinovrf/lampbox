#include "delegateadsrepeat.h"
#include "ui_delegateadsrepeat.h"
#include <QRegularExpression>

using namespace lampproject;
using namespace lampproject::delegate;

DelegateAdsRepeat::DelegateAdsRepeat(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DelegateAdsRepeat)
{
    ui->setupUi(this);
}

DelegateAdsRepeat::~DelegateAdsRepeat()
{
    delete ui;
}

QString DelegateAdsRepeat::getValue(){
return currentValue;
}

bool DelegateAdsRepeat::setValue(QString value){
    QRegularExpression re("\\d+m");
    QRegularExpressionMatch match = re.match(value);

        if(match.hasMatch()){
            QRegularExpression getMin("\\d+");
            QRegularExpressionMatch matchMin = getMin.match(value);
            ui->timeEdit->setTime(QTime(0,matchMin.captured().toInt()));
            return true;
        } else {
            ui->horizontalSlider->setValue(value.toInt());
            return true;
        }
    return false;
}

void DelegateAdsRepeat::on_horizontalSlider_valueChanged(int value)
{
     currentValue = QString("%1").arg(value);
     ui->label->setText(tr("%1 раз в час").arg(value));
}

void DelegateAdsRepeat::on_timeEdit_timeChanged(const QTime &time)
{
    currentValue = QString("%1m").arg(time.toString("mm"));
    ui->label->setText(tr("в %1-ую минуту").arg(time.toString("mm")));
}

DelegateAdsRepeatEdit::DelegateAdsRepeatEdit(QObject *parent):
    QStyledItemDelegate(parent)
{

}

QString DelegateAdsRepeatEdit::displayText ( const QVariant & value, const QLocale & locale ) const{
    Q_UNUSED(locale);
//    QList<DayOfWeek> days = DelegateAdsRepeatEdit::getDays(value.toString());
    QString friendlyString;
    QRegularExpression re("\\d+m");
    QRegularExpressionMatch match = re.match(value.toString());
    if(match.hasMatch()){
        QRegularExpression getMin("\\d+");
        QRegularExpressionMatch matchMin = getMin.match(value.toString());
        friendlyString = tr("В %1-ую минуту")
                        .arg(matchMin.captured());
    } else {
       friendlyString = tr("%1 раз(а) в час").arg(value.toInt());
    }
    return friendlyString;
}
QWidget* DelegateAdsRepeatEdit::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(option);
    Q_UNUSED(index);
    DelegateAdsRepeat *editor = new DelegateAdsRepeat(parent);
#ifdef __APPLE__
    editor->setFocusPolicy(Qt::StrongFocus);
#endif
    editor->setAutoFillBackground(true);
    return editor;
}

void DelegateAdsRepeatEdit::setEditorData(QWidget *editor, const QModelIndex &index) const{
    QString value = index.model()->data(index, Qt::EditRole).toString();

    DelegateAdsRepeat * adsrEditor = static_cast<DelegateAdsRepeat*>(editor);
    adsrEditor->setValue(value);
}
void DelegateAdsRepeatEdit::setModelData(QWidget *editor, QAbstractItemModel *model,
                  const QModelIndex &index) const{
    DelegateAdsRepeat * adsrEditor = static_cast<DelegateAdsRepeat*>(editor);
    QString value = adsrEditor->getValue();

    model->setData(index,value);
}

void DelegateAdsRepeatEdit::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const{
//    Q_UNUSED(option);
    Q_UNUSED(index);
    QRect rect = option.rect;
    QSize sizeHint = editor->sizeHint();

    if (rect.width()<sizeHint.width()) rect.setWidth(sizeHint.width());
    if (rect.height()<sizeHint.height()) rect.setHeight(sizeHint.height());

    editor->setGeometry(rect);
    editor->setBackgroundRole(QPalette::Window);
}

