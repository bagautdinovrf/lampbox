#include "onairdelegate.h"
#include "switch.hpp"
#include "drawing.hpp"
#include <QPainter>
#include <QStyleOption>
#include <QLinearGradient>
#include <QStyle>
using namespace lampproject::delegate;

OnAirDelegate::OnAirDelegate(QObject *parent) :
    QStyledItemDelegate(parent)
{
}
QString OnAirDelegate::displayText( const QVariant & value, const QLocale & locale ) const{
int values = value.toInt();
return (values == 2 ? tr("В эфире") : tr("Отключен"));
}
QWidget* OnAirDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                      const QModelIndex &index) const{
    Q_UNUSED(index)
    Q_UNUSED(option)
    QColor onColor;
    QColor offColor;
    onColor.setNamedColor("#66FF00");
    offColor.setNamedColor("#999999");
    Switch *editor = new Switch(parent);
    QFont f = editor->font();
    f.setBold( true );
    editor->setFont( f );
    editor->setOnText(tr("В эфире"));
    editor->setOffText(tr("Откл."));
    editor->setOnColor(onColor);
#ifdef __APPLE__
    editor->setFocusPolicy(Qt::StrongFocus);
#endif
    //editor->setAutoFillBackground(true);
    return editor;
}

void OnAirDelegate::setEditorData(QWidget *_editor, const QModelIndex &index) const{
    int value = index.model()->data(index, Qt::EditRole).toInt();

    Switch * editor = static_cast<Switch*>(_editor);
    if(value == 2){
        editor->setState(Switch::AcceptedCheck);
    } else {
       editor->setState(Switch::AcceptedUncheck);
    }

}
void OnAirDelegate::setModelData(QWidget *_editor, QAbstractItemModel *model,
                  const QModelIndex &index) const{
    Switch * editor = static_cast<Switch*>(_editor);
    int value = 0;
    if(editor->isChecked()){
        value = 2;
    } else {
        value = 8;
    }
    model->setData(index,value);
    emit setDataMultiple(value);
}

void OnAirDelegate::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(index)
    QRect rect = option.rect;
    QSize sizeHint = editor->sizeHint();

    if (rect.width()<sizeHint.width()) rect.setWidth(sizeHint.width());
    if (rect.height()<sizeHint.height()) rect.setHeight(sizeHint.height());

    editor->setGeometry(rect);
    editor->setBackgroundRole(QPalette::Window);
}

void OnAirDelegate::paint( QPainter * p, const QStyleOptionViewItem & option,
            const QModelIndex & index ) const{
    QColor onColor;
    QColor offColor;
    onColor.setNamedColor("#66FF00");
    offColor.setNamedColor("#999999");
    int value = index.model()->data(index, Qt::EditRole).toInt();
    if(value == 2){
        p->fillRect(option.rect, QBrush(onColor));;
    } else {
        p->fillRect(option.rect, QBrush(offColor));
    }
    QStyledItemDelegate::paint( p, option, index );
}
