#include "delegatedatetimeperiod.h"
#include "ui_delegatedatetimeperiod.h"
#include <QDebug>
#include <stdlib.h>

using namespace lampproject::delegate;


///************************************************************************************************************************
//! ДЕЛЕГАТ
///************************************************************************************************************************
DelegateDateTimePeriodEdit::DelegateDateTimePeriodEdit(DataTimePeriodDelegateType delegateType, QObject *parent):
    QStyledItemDelegate(parent),
    mDelegateType(delegateType),
    drawParent(static_cast<QWidget*>(parent))
{
    //
    foreverText[day] = tr("Каждый день");
    foreverText[minute] = tr("Никогда");
    foreverText[hour] = tr("Круглосуточно");
}

QString DelegateDateTimePeriodEdit::displayText( const QVariant & value, const QLocale & locale ) const
{
    Q_UNUSED(locale)
    return userFriendly(value.toString().split(','));
}

QString DelegateDateTimePeriodEdit::userFriendly(QStringList item) const
{
    if( item.isEmpty() )
        return tr("Никогда");
    if(item[0] == "*")
        return foreverText[mDelegateType];

    if( mDelegateType != minute ) {
        return DelegateDateTimePeriodEdit::groupItem(item, GroupUserFriendly).join(", ");
    } else if( minute == mDelegateType ) {
        if( !item.first().contains("m") )
            return item.join(", ") + QString(" в час");
        else
            return item.replaceInStrings( "m", tr("") ).join(", ");
//            return item.replaceInStrings("m",tr(" мин.")).join(", ");
    }
    return QString();
}

QStringList DelegateDateTimePeriodEdit::groupItem(QStringList value, GroupFormat grouping) const
{
    QStringList groupList;
    if(value.count() <= 1){
        return value;
    }
    QMap<int,int> mapPeriod;
    foreach (QString item, value) {
        mapPeriod[item.toInt()] = item.toInt();
    }

    QMapIterator<int,int> mapIter(mapPeriod);

    while(mapIter.hasNext()){
        mapIter.next();
        QMapIterator<int, int> mapFound(mapPeriod);
        while (mapFound.hasNext()) {
            mapFound.next();
            if(mapFound.key() == mapIter.value()+1){
                mapPeriod[mapIter.key()] = mapFound.value();
                mapPeriod.remove(mapFound.key());
                mapIter.toFront();
                break;
            }
        }
    }

    QMapIterator<int,int> group(mapPeriod);
    while(group.hasNext()){
        group.next();
        if(group.key() == group.value()){
            if( grouping == GroupUserFriendly){
                groupList += tr("в %1").arg(group.value());
            } else {
                groupList += QString("%1").arg(group.value());
            }
        } else {
            if( grouping == GroupUserFriendly){
                groupList += tr("с %1 по %2").arg(group.key()).arg(group.value());
            } else {
                groupList += QString("%1-%2").arg(group.key()).arg(group.value());
            }
        }
    }

    return groupList;
}

QWidget *DelegateDateTimePeriodEdit::createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(option);
    //Q_UNUSED(index);
    DelegateDateTimePeriod *editor = new DelegateDateTimePeriod(parent);
    //#ifdef __APPLE__
    editor->setFocusPolicy(Qt::StrongFocus);
    //#endif
    editor->setType(mDelegateType);
    editor->setAutoFillBackground(true);
    return editor;
}

void DelegateDateTimePeriodEdit::setEditorData(QWidget *editor, const QModelIndex &index) const
{
    QString value = index.model()->data(index, Qt::EditRole).toString();
    DelegateDateTimePeriod * deditor = static_cast<DelegateDateTimePeriod*>(editor);
    deditor->setValue((mDelegateType != minute ? DelegateDateTimePeriodEdit::groupItem(value.split(',')).join(", ") :
                                               value));
    connect(deditor,&DelegateDateTimePeriod::accept,this,
                    &DelegateDateTimePeriodEdit::commitAndCloseEditor);
    connect(deditor,&DelegateDateTimePeriod::reject,this,
                    [=](){
                        this->destroyEditor(deditor,index);
                    });

}

void DelegateDateTimePeriodEdit::commitAndCloseEditor()
{
    DelegateDateTimePeriod *editor = qobject_cast<DelegateDateTimePeriod*>(sender());
    emit commitData(editor);
    emit closeEditor(editor);
}

void DelegateDateTimePeriodEdit::setModelData(QWidget *editor, QAbstractItemModel *model,
                                              const QModelIndex &index) const{
    DelegateDateTimePeriod * deditor = static_cast<DelegateDateTimePeriod*>(editor);
    QString value = deditor->getValue();
    qDebug() << "INDEX: " << index.row();
    model->setData(index,value);
    emit setDataMultiple(value);
}

void DelegateDateTimePeriodEdit::updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    Q_UNUSED(index);

    QRect pWindow = option.widget->geometry();
    QRect rect = option.rect;
    QSize gEditor = editor->sizeHint();
    if( (rect.y() + gEditor.height()) > pWindow.height() ) {
        rect.moveTop( rect.y() + rect.height() - gEditor.height());
    }

    if ( rect.width() < gEditor.width() )
        rect.setWidth(gEditor.width() );

    if ( rect.height() < gEditor.height() )
        rect.setHeight(gEditor.height());
    if(rect.y() < 0){
        rect.moveTop(0);
    }
    editor->setGeometry(rect);
    editor->setBackgroundRole(QPalette::Window);
}

void DelegateDateTimePeriodEdit::setType(DataTimePeriodDelegateType stype)
{
    mDelegateType = stype;
}

bool DelegateDateTimePeriodEdit::editorEvent(QEvent * event, QAbstractItemModel * model,
                 const QStyleOptionViewItem & option, const QModelIndex & index){
    Q_UNUSED(model);
    Q_UNUSED(option);
    Q_UNUSED(index);
    if(event->type() == QEvent::MouseButtonPress){
            event->ignore();
//            return true;
    }
return false;
}

///************************************************************************************************************************
//! ВИДЖЕТ
///************************************************************************************************************************
DelegateDateTimePeriod::DelegateDateTimePeriod(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DelegateDateTimePeriod),type(hour)
{
    format[hour] = "H";
    format[minute] = "mm";
    format[day] = "d";

    maximum[hour] = 23;
    maximum[minute] = 59;
    maximum[day] = 31;

    minimum[hour] = 0;
    minimum[minute] = 0;
    minimum[day] = 1;

    label[hour] = tr("Час");
    label[minute] = tr("Минута");
    label[day] = tr("День");

    ui->setupUi(this);
    ui->spinBox->setDisabled(true);

//    if( minute == type )
        connect(ui->checkBox, SIGNAL( stateChanged(int) ), SLOT(setElementsState(int) ) );
}

DelegateDateTimePeriod::~DelegateDateTimePeriod()
{
    delete ui;
}


void DelegateDateTimePeriod::setElementsState(int cb)
{
    if( type != minute )
        return;

    bool state;
    if( cb == 0 ) {
        state = false;
        setValue("*");
    }

    if( cb == 2 )
        state = true;

    ui->spinBox->setEnabled(state);
    ui->pushButtonAddOne->setDisabled(state);
    ui->timeEditOne->setDisabled(state);
    ui->timeEditStart->setDisabled(state);
    ui->pushButtonAddStartEnd->setDisabled(state);
    ui->listWidget->setDisabled(state);
}


QString DelegateDateTimePeriod::getValue()
{
    QStringList listItem;
    for(int i = 0; i < ui->listWidget->count(); ++i)
    {
        QListWidgetItem* item = ui->listWidget->item(i);
        listItem += convertItem(item->text());
    }
    QStringList clearList = removeDuplicate(listItem);
    //add letter 'm'
    if(type == minute && !ui->checkBox->isChecked() ){
        for(int j = 0; j < clearList.count(); ++j){
            if(clearList[j] != "*"){
                clearList[j].append('m');
            }
        }
    } else if(type == minute) {
        return ui->spinBox->text();
    }
    return clearList.join(",");
}


bool DelegateDateTimePeriod::setValue(QString value)
{
    if( value.right(1) != "m" && value.right(1) != "*" && type == minute ) {
        ui->checkBox->setChecked(true);
        ui->spinBox->setValue(value.toInt());
        //WARNING: костыль
        return true;
    }

    QStringList listItem;
    value.remove(QChar('m'), Qt::CaseInsensitive);
    listItem = value.split(",");
    for(int i = 0; i < listItem.count(); ++i){
        if(listItem.at(i) != "*"){
            ui->listWidget->addItem(new TimePeriodListWidgetItem(listItem.at(i)));
        }
    }
    checkList();
    //WARNING: костыль
    return true;
}

void DelegateDateTimePeriod::setType(DataTimePeriodDelegateType stype)
{
    type=stype;
    ui->label->setText(label[type]);
    // Максимальные и минимальные значения
    ui->timeEditStart-> setMaximum(maximum[type]);
    ui->timeEditStart-> setMinimum(minimum[type]);

    ui->timeEditEnd->   setMaximum(maximum[type]);
    ui->timeEditEnd->   setMinimum(minimum[type]);

    ui->timeEditOne->setMaximum(maximum[type]);
    ui->timeEditOne->setMinimum(minimum[type]);
//    ui->timeEditOne->setDisplayFormat(format[type]);
//    ui->timeEditStart->setDisplayFormat(format[type]);
//    ui->timeEditEnd->setDisplayFormat(format[type]);
    ui->spinBox->hide();
    ui->checkBox->hide();
    //для минут нет интервалов: только точное значение
    if(type == minute) {
        ui->spinBox->show();
        ui->checkBox->show();
        ui->label_2->setVisible(false);
        ui->label_3->setVisible(false);
        ui->timeEditStart->setVisible(false);
        ui->timeEditEnd->setVisible(false);
        ui->pushButtonAddStartEnd->setVisible(false);
    }
}

void lampproject::delegate::DelegateDateTimePeriod::on_pushButtonAddOne_clicked()
{
    if(!allreadyInList(ui->timeEditOne->text())){
        ui->listWidget->addItem(new TimePeriodListWidgetItem( ui->timeEditOne->text()));
    }
    checkList();
}

void lampproject::delegate::DelegateDateTimePeriod::on_pushButtonAddStartEnd_clicked()
{
    QString text = QString("%1-%2")
            .arg(ui->timeEditStart->text())
            .arg(ui->timeEditEnd->text());
    if(!allreadyInList(text)){
        ui->listWidget->addItem(new TimePeriodListWidgetItem( text));
    }
    checkList();
}

void lampproject::delegate::DelegateDateTimePeriod::on_listWidget_clicked(const QModelIndex &index)
{
    if(index.row() < ui->listWidget->count()){
        ui->listWidget->takeItem(index.row());
    }
    checkList();
}
bool lampproject::delegate::DelegateDateTimePeriod::allreadyInList(QString element){
    if(ui->listWidget->findItems(element,Qt::MatchExactly).count() != 0){
        return true;
    }
    return false;
}

void lampproject::delegate::DelegateDateTimePeriod::checkList(){
    if(ui->listWidget->count() == 0){
        ui->listWidget->addItem(new TimePeriodListWidgetItem("*"));
        return;
    } else if(ui->listWidget->count() > 1) {
        foreach(QListWidgetItem* item, ui->listWidget->findItems("*",Qt::MatchExactly)){
            delete item;
        }
    }
    ui->listWidget->sortItems();
}

QStringList lampproject::delegate::DelegateDateTimePeriod::convertItem(QString item){
    QRegExp re;
    re.setPattern("(\\d+)-(\\d+)");
    if(re.indexIn(item) != -1){
        int start = re.cap(1).toInt();
        int end = re.cap(2).toInt();
        QStringList fatLine;
        if(start < end){
            for(int i = start; i <= end ; ++i ){
                fatLine << QString("%1").arg(i);
            }
            return fatLine;
        }
        if(start > end){
            int endPeriod = maximum[type];
            for(int i = start; i <= endPeriod ; ++i ){
                fatLine << QString("%1").arg(i);
            }
            for(int i = 0; i <= end ; ++i ){
                fatLine << QString("%1").arg(i);
            }
            return fatLine;
        }
        if(start == end){
            return QStringList(QString("%1").arg(start));
        }
    }
    return QStringList(item);
}

QStringList DelegateDateTimePeriod::removeDuplicate(QStringList value){
    std::list<QString> clearList = value.toStdList();
    clearList.sort();
    clearList.unique([=](QString first, QString second){
        return ( first == second);
    });
    return QStringList::fromStdList(clearList);
}


///************************************************************************************************************************
//! Запись в листе
///************************************************************************************************************************
TimePeriodListWidgetItem::TimePeriodListWidgetItem(QString text, QListWidget *view,int type): QListWidgetItem(view,type){
    setText(text);
    QRegExp re;
    re.setPattern("(\\d+)-(\\d+)");
    if(re.indexIn(text) != -1){
        setIcon(QIcon(":/ico/res/period.png"));
    } else {
        setIcon(QIcon(":/ico/res/onepoint.png"));
    }
}

bool TimePeriodListWidgetItem::operator< ( const QListWidgetItem & other ) const{
    int this_value = this->data(Qt::EditRole).toInt();
    int other_value = other.data(Qt::EditRole).toInt();
    return this_value < other_value;
}

void lampproject::delegate::DelegateDateTimePeriod::on_pushButtonAccept_clicked()
{
    emit accept();
}

void lampproject::delegate::DelegateDateTimePeriod::on_pushButtonCancel_clicked()
{
    emit reject();
}
