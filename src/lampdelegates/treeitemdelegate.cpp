#include "treeitemdelegate.h"
#include "globallistener.h"
#include "globalairconnect.h"

#include <QLineEdit>
#include <QPainter>
#include <QApplication>
TreeItemDelegate::TreeItemDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
    makeIconList();
    plType[Audio] = tr("Музыкальный");
    plType[Video] = tr("Видео");
    plType[Slide] = tr("Слайды");

}

TreeItemDelegate::~TreeItemDelegate()
{

}


QString TreeItemDelegate::displayText( const QVariant & value, const QLocale & locale ) const{
    return value.toString();
}

QWidget* TreeItemDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                                        const QModelIndex &index) const{
    QLineEdit* line = new QLineEdit(parent);
    return line;
}

void TreeItemDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const{
    QString name = index.model()->data(index, Qt::EditRole).toString();
    QLineEdit * line = static_cast<QLineEdit*>(editor);
    line->setText(name);
}
void TreeItemDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                                    const QModelIndex &index) const{
    QLineEdit * line = static_cast<QLineEdit*>(editor);
    model->setData(index,line->text());
    //    emit setDataMultiple(value);
}
void TreeItemDelegate::updateEditorGeometry(QWidget *editor,
                                            const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(index)
    QRect rect = option.rect;
    QSize sizeHint = editor->sizeHint();

    if (rect.width()<sizeHint.width()) rect.setWidth(sizeHint.width());
    if (rect.height()<sizeHint.height()) rect.setHeight(sizeHint.height());

    editor->setGeometry(rect);
    editor->setBackgroundRole(QPalette::Window);
}
void TreeItemDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option,
                              const QModelIndex & index ) const{
    LMObject* object = reinterpret_cast<LMObject*>(index.internalPointer());
    TreeItemDelegate* This = (TreeItemDelegate*)this;
    QStyleOptionViewItemV4 opt = option;
    initStyleOption(&opt, index);

    QString line0 = index.model()->data(index).toString();
    QString line1 = "";
    if(object){
        switch(object->Type()){
        case OTOrgs:
             line1 = tr("Мест: %1").arg(object->ChildCount());
             break;
        case OTDeps:
            line1 = tr("Станций: %1").arg(object->ChildCount());
            break;
        case OTPlaylist:
            {
            Playlist* pl = reinterpret_cast<Playlist*>(object);
            TypePlaylist type = TypePlaylist( pl->data(Playlist::video).toInt());
            line1 = plType[type];
            }
            break;
        case OTGroup:
        case OTTrash:
            line1 = tr("Объектов: %1").arg(object->ChildCount());
            break;
        case OTMediastation:
            line1 = This->nowPlay(object->getId());
            break;
        }
    }
    // draw correct background
    opt.text = "";
    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QRect rect = opt.rect;
    QPalette::ColorGroup cg = opt.state & QStyle::State_Enabled ? QPalette::Normal : QPalette::Disabled;
    if (cg == QPalette::Normal && !(opt.state & QStyle::State_Active))
        cg = QPalette::Inactive;

    // set pen color
    if (opt.state & QStyle::State_Selected)
        painter->setPen(opt.palette.color(cg, QPalette::HighlightedText));
    else
        painter->setPen(opt.palette.color(cg, QPalette::Text));

    // draw 2 lines of text
    painter->drawText(QRect(rect.left()+17, rect.top(), rect.width(), rect.height()/2),
                      opt.displayAlignment, line0);
    // set pen color
    painter->setPen(QPen(Qt::gray));

    painter->drawText(QRect(rect.left()+17, rect.top()+rect.height()/2, rect.width(), rect.height()/2),
                      opt.displayAlignment, line1);
    ////    option.icon = StatusItem(object);
    //     painter->save();
        painter->drawImage(QRect(option.rect.left(),(rect.top()+rect.height()/2)-8,16,16),This->StatusItem(object));
    //    painter->restore();
    //    QStyledItemDelegate::paint(painter,option,index);
}
QSize TreeItemDelegate::sizeHint(const QStyleOptionViewItem &option,
               const QModelIndex &index) const{
    QSize result = QStyledItemDelegate::sizeHint(option, index);
           result.setHeight(result.height()*2);
           return result;
}
void TreeItemDelegate::makeIconList(){
    icons.clear();
    icons[OTOrgs] = QImage(":/ico/toolbar/main/resource/lamp_ico_new_company.png");
    icons[OTDeps] = QImage(":/ico/toolbar/main/resource/lamp_ico_new_deps.png");
    icons[OTMediastation] = QImage(":/ico/toolbar/main/resource/lamp_ico_new_box.png");
    icons[OTPlaylist] = QImage(":/ico/toolbar/main/resource/lamp_ico_playlist.png");
    icons[OTAdvertising] = QImage(":/ico/toolbar/main/resource/lamp_ico_ads.png");
    icons[OTGroup] = QImage(":/ico/toolbar/main/resource/group.png");
    icons[OTTrash] = QImage(":/ico/toolbar/main/resource/trash.png");
    iconsBoxs[JournalListen::Just] = QImage(":/ico/toolbar/main/resource/lamp_ico_new_box_just.png");
    iconsBoxs[JournalListen::Late] = QImage(":/ico/toolbar/main/resource/lamp_ico_new_box_late.png");
    iconsBoxs[JournalListen::Long] = QImage(":/ico/toolbar/main/resource/lamp_ico_new_box_long.png");

}

QImage& TreeItemDelegate::StatusItem(const LMObject* object){
    if(!object)
        return QImage();
    if(object->Type() == OTMediastation){
        LMObject* box = const_cast<LMObject*>(object);
        MusicboxId id = box->getId();
        JournalListen::PlayerStatus current = status(id);
        return iconsBoxs[current];
    }
    return icons[object->Type()];
}

JournalListen::PlayerStatus TreeItemDelegate::status(const MusicboxId id){
    if(statusBoxs.count(id) == 0){
        JournalListenController *l = GlobalListener::Instance().get(id);
        connect(l,&JournalListenController::playerStatus, this,
                [=](JournalListen::PlayerStatus status)
        {
            JournalListenController *listner = static_cast<JournalListenController*>(sender());
            if(statusBoxs[listner->getId()] != status){
                statusBoxs[listner->getId()] = status;
                emit hasChanges();
            }
        });
        statusBoxs[id] = JournalListen::Long;
    }
    return statusBoxs[id];
}

QString TreeItemDelegate::nowPlay(const MusicboxId id) {
    if(nowPlayBoxs.count(id) == 0){
        AirConnect* air = GlobalAirConnect::Instance().get(id);
        connect(air,&AirConnect::nowPlayChange, this,
                [=](QDateTime start, QTime time, int /*delta*/, TrackId /*id*/, QString label, QString title)
                {
                    AirConnect *_air = static_cast<AirConnect*>(sender());
                    if(nowPlayBoxs[_air->getId()] != title){
                        nowPlayBoxs[_air->getId()] = title;
                      //  emit hasChanges();
                    }
                }, Qt::QueuedConnection);
         nowPlayBoxs[id] = tr("Нет данных");
    }
    return nowPlayBoxs[id];
}
