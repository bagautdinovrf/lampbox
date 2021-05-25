#include <QMouseEvent>
#include <QKeyEvent>
#include <QApplication>

#include <QDebug>

#include "radiobuttondelegate.h"


RadioButtonItemDelegate::RadioButtonItemDelegate(QObject *p)
        : QStyledItemDelegate(p)
{
    // nothing
}

void RadioButtonItemDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (index.data(IsRadioRole).canConvert<bool>() && index.data(IsRadioRole).value<bool>()) {
        /// determine size and spacing of radio buttons in current style
        int radioButtonWidth = QApplication::style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, &option);
        int spacing = QApplication::style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing, &option);

        /// draw default appearance (text, highlighting) shifted to the left
        QStyleOptionViewItem myOption = option;
        int left = myOption.rect.left();
        myOption.rect.setLeft(left + spacing * 3 / 2 + radioButtonWidth);
        QStyledItemDelegate::paint(painter, myOption, index);

        /// draw radio button in the open space
        myOption.rect.setLeft(left + spacing / 2);
        myOption.rect.setWidth(radioButtonWidth);
        if (index.data(RadioSelectedRole).canConvert<bool>()) {
            /// change radio button's visual appearance if selected or not
            bool radioButtonSelected = index.data(RadioSelectedRole).value<bool>();
            myOption.state |= radioButtonSelected ? QStyle::State_On : QStyle::State_Off;
        }
        QApplication::style()->drawPrimitive(QStyle::PE_IndicatorRadioButton, &myOption, painter);
    } else
        QStyledItemDelegate::paint(painter, option, index);
}

QSize RadioButtonItemDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QSize s = QStyledItemDelegate::sizeHint(option, index);
    if (index.data(IsRadioRole).value<bool>()) {
        /// determine size of radio buttons in current style
        int radioButtonHeight = QApplication::style()->pixelMetric(QStyle::PM_ExclusiveIndicatorHeight, &option);
        /// ensure that line is tall enough to draw radio button
        s.setHeight(qMax(s.height(), radioButtonHeight));
    }
    return s;
}
