#include "delegatevolumeedit.h"
#include <QMainWindow>
#include <QApplication>
#include <QSpinBox>

using namespace lampproject::delegate;
DelegateVolumeEdit::DelegateVolumeEdit(QObject *parent):
    QStyledItemDelegate(parent)
{

}
QWidget* DelegateVolumeEdit::createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                      const QModelIndex &index) const{
    Q_UNUSED(index)
    Q_UNUSED(option)
    QSpinBox *editor = new QSpinBox(parent);
    editor->setMaximum(100);
    editor->setMinimum(0);
#ifdef __APPLE__
    editor->setFocusPolicy(Qt::StrongFocus);
#endif
    editor->setAutoFillBackground(true);
    return editor;
}

void DelegateVolumeEdit::setEditorData(QWidget *_editor, const QModelIndex &index) const{
    int value = index.model()->data(index, Qt::EditRole).toInt();

    QSpinBox * editor = static_cast<QSpinBox*>(_editor);
    editor->setValue(value);

}
void DelegateVolumeEdit::setModelData(QWidget *_editor, QAbstractItemModel *model,
                  const QModelIndex &index) const{
    QSpinBox * editor = static_cast<QSpinBox*>(_editor);
    int value = editor->value();
    model->setData(index,value);
    emit setDataMultiple(value);
}

void DelegateVolumeEdit::updateEditorGeometry(QWidget *editor,
    const QStyleOptionViewItem &option, const QModelIndex &index) const{
    Q_UNUSED(index)
    QRect rect = option.rect;
    QSize sizeHint = editor->sizeHint();

    if (rect.width()<sizeHint.width()) rect.setWidth(sizeHint.width());
    if (rect.height()<sizeHint.height()) rect.setHeight(sizeHint.height());

    editor->setGeometry(rect);
    editor->setBackgroundRole(QPalette::Window);
}

void DelegateVolumeEdit::paint( QPainter * painter, const QStyleOptionViewItem & option,
            const QModelIndex & index ) const{
    QStyleOptionProgressBar progressBarStyleOption;
     progressBarStyleOption.rect = option.rect;
     int volume = index.model()->data(index, Qt::EditRole).toInt();
     progressBarStyleOption.minimum = 0;
     progressBarStyleOption.maximum = 100;
     progressBarStyleOption.textAlignment = Qt::AlignCenter;
     progressBarStyleOption.progress = volume ;
     progressBarStyleOption.text = QString( "%1%" ).arg( volume  );
     progressBarStyleOption.textVisible = true;

     qApp->style()->drawControl( QStyle::CE_ProgressBar, &progressBarStyleOption, painter );
}
