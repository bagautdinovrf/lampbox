#include "comboboxdelegate.h"

#ifdef LMMODELS
    #include "modelsfabric.h"
#endif
#ifdef LMOBJECTS
    #include "lmobjectfabric.h"
using namespace lampproject;
using namespace lampproject::lmobject;
#endif
#include <QComboBox>
#include <QPainter>
#include <QLabel>

ComboBoxDelegate::ComboBoxDelegate(LMObjectType type,QObject *parent):
        QStyledItemDelegate(parent),
        objectType(type)
{

}

ComboBoxDelegate::~ComboBoxDelegate()
{

}

QString ComboBoxDelegate::displayText( const QVariant & value, const QLocale & locale ) const
{
    Q_UNUSED(locale);
#ifdef LMOBJECTS
    bool ok;
    LMObjectId id = value.toInt(&ok);
    if(ok){
        LMObject* object = (const_cast<ComboBoxDelegate*>(this))->get(id);
        if(object != nullptr){
            QString text = object->Text();
            return text;
        }
    }
#endif
    return value.toString();
}
QWidget *ComboBoxDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                      const QModelIndex &index) const
{
    Q_UNUSED(index);
    Q_UNUSED(option);
    QComboBox *editor = new QComboBox(parent);
    editor->setEditable(false);
    return editor;
}

void ComboBoxDelegate::setEditorData(QWidget *editor, const QModelIndex &index) const{
    int value = index.model()->data(index, Qt::EditRole).toInt();
    QComboBox *combobox = static_cast<QComboBox*>(editor);
#ifdef LMMODELS
    combobox->setModel(ModelFabric::createModels(objectType));
    combobox->setCurrentIndex(combobox->findData(value,Qt::EditRole));
    combobox->setModelColumn(1);
#else
    #pragma message ("LMMODELS not defined: " __FILE__)
#endif
}

void ComboBoxDelegate::setModelData(QWidget *editor, QAbstractItemModel *model,
                  const QModelIndex &index) const{
    QComboBox *combobox = static_cast<QComboBox*>(editor);
    int value = combobox->model()->data(combobox->model()->index(combobox->currentIndex(),0)).toInt();
    model->setData(index,value);
}

void ComboBoxDelegate::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(index)
    QRect rect = option.rect;
    QSize sizeHint = editor->sizeHint();

    if (rect.width()<sizeHint.width()) rect.setWidth(sizeHint.width());
    if (rect.height()<sizeHint.height()) rect.setHeight(sizeHint.height());

    editor->setGeometry(rect);
    editor->setBackgroundRole(QPalette::Window);
}
void ComboBoxDelegate::paint( QPainter * painter, const QStyleOptionViewItem & option,
            const QModelIndex & index ) const{
    QStyledItemDelegate::paint(painter,option,index);
//    painter->setBrush(QBrush(Qt::white));
//    painter->setPen(QPen(Qt::black));
//    painter->drawRoundedRect(QRect(option.rect.right()-40,2,30,30),3,3);
}
LMObject* ComboBoxDelegate::get(LMObjectId id){
    ///it is ID!
    LMObject* object = nullptr;
    auto found = cacheObject.find(id);
    if(found == cacheObject.end()){
        object = LMObjectFabric::createObject(objectType,id);
        cacheObject[id] = std::unique_ptr<LMObject>(object);
    } else {
        object = (*found).second.get();
    }
    return object;
}
