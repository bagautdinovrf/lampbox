#include "medialibrarydelegate.h"
#include "settings.h"

#include "mediamodel.h"
#include "restyletheme.h"

#include <QApplication>
#include <QAbstractItemView>
#include <QContextMenuEvent>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QItemSelectionModel>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionButton>
#include <QToolTip>

namespace {
QString fileIcon(const QModelIndex &index)
{
    const bool video = index.data(MediaModel::MediaTypeRole).toInt() == MediaBoxManager::VIDEO
        || Settings::allVideoFormats().contains(
            QStringLiteral("*.") + index.data(MediaModel::FormatRole).toString(), Qt::CaseInsensitive);
    return video ? QStringLiteral("video") : QStringLiteral("music");
}

void paintFileTile(QPainter *painter, const QRectF &rect, const QModelIndex &index)
{
    const Restyle::Tokens &theme = Restyle::tokens();
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    Restyle::paintSurface(*painter, rect, QStringLiteral("thumbnail"));
    const QRectF iconRect(rect.center().x() - 7, rect.center().y() - 7, 14, 14);
    const bool playing = index.data(MediaModel::PlayingRole).toBool();
    Restyle::paintIcon(*painter, iconRect, playing ? QStringLiteral("play") : fileIcon(index),
                       playing ? theme.success : theme.secondary);
    painter->restore();
}
}

MediaLibraryDelegate::MediaLibraryDelegate(QObject *parent) : QStyledItemDelegate(parent)
{
    if (auto *view = qobject_cast<QAbstractItemView *>(parent)) {
        view->viewport()->installEventFilter(this);
        view->installEventFilter(this);
    }
}

void MediaLibraryDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option,
                                 const QModelIndex &index) const
{
    QStyleOptionViewItem item(option);
    initStyleOption(&item, index);
    const bool playing = index.data(MediaModel::PlayingRole).toBool();
    const Restyle::Tokens &theme = Restyle::tokens();
    if (playing)
        item.backgroundBrush = theme.successBg;
    item.text.clear();
    item.icon = QIcon();
    item.features &= ~(QStyleOptionViewItem::HasDisplay | QStyleOptionViewItem::HasDecoration);
    const QStyle *style = item.widget ? item.widget->style() : QApplication::style();
    // Native painting preserves the view's hover, selection and keyboard focus.
    style->drawControl(QStyle::CE_ItemViewItem, &item, painter, item.widget);

    painter->save();
    painter->setClipRect(option.rect);
    const bool enabled = option.state.testFlag(QStyle::State_Enabled);
    const QColor textColor = enabled ? theme.text : theme.muted;
    const QColor secondaryColor = enabled ? theme.secondary : theme.muted;
    const QRect content = option.rect.adjusted(8, 4, -8, -4);

    if (index.column() == MediaModel::FileNameColumn) {
        if (playing)
            painter->fillRect(QRect(option.rect.left(), option.rect.top() + 3, 3,
                                    option.rect.height() - 6), theme.success);
        QStyleOptionButton check;
        check.rect = selectionRect(option.rect);
        check.palette = option.palette;
        check.state = enabled ? QStyle::State_Enabled : QStyle::State_None;
        check.state |= option.state.testFlag(QStyle::State_Selected) ? QStyle::State_On : QStyle::State_Off;
        style->drawPrimitive(QStyle::PE_IndicatorCheckBox, &check, painter, item.widget);
        const QRectF tile(content.left() + 37, option.rect.center().y() - 12.0, 23, 25);
        paintFileTile(painter, tile, index);
        const int textLeft = content.left() + 68;
        const int availableWidth = qMax(0, content.right() - textLeft + 1);
        const QString fileName = index.data(MediaModel::FileNameRole).toString();
        QString title = index.data(MediaModel::TitleRole).toString();
        if (title.trimmed().isEmpty())
            title = fileName;
        QString metadata = index.data(MediaModel::ArtistRole).toString();
        if (metadata.trimmed().isEmpty() && title != fileName)
            metadata = fileName;

        const QFont titleFont = Restyle::font(13, playing ? QFont::DemiBold : QFont::Normal);
        const QFont metadataFont = Restyle::font(11);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics metadataMetrics(metadataFont);
        // Reserve metadata only when it is present. The title retains priority
        // and both strings elide independently inside the existing column.
        const int metadataReserve = metadata.isEmpty() ? 0
            : qMin(metadataMetrics.horizontalAdvance(metadata) + 9, availableWidth / 3);
        const QString visibleTitle = titleMetrics.elidedText(title, Qt::ElideRight,
                                                            availableWidth - metadataReserve);
        const int baseline = option.rect.top() + (option.rect.height() - titleMetrics.height()) / 2
            + titleMetrics.ascent();
        painter->setFont(titleFont);
        painter->setPen(playing && enabled ? theme.success : textColor);
        painter->drawText(textLeft, baseline, visibleTitle);
        const int metadataLeft = textLeft + titleMetrics.horizontalAdvance(visibleTitle) + 9;
        const int metadataWidth = qMax(0, content.right() - metadataLeft + 1);
        if (!metadata.isEmpty() && metadataWidth > 0) {
            painter->setFont(metadataFont);
            painter->setPen(secondaryColor);
            painter->drawText(metadataLeft, baseline,
                              metadataMetrics.elidedText(metadata, Qt::ElideRight, metadataWidth));
        }
    } else {
        const bool format = index.column() == MediaModel::FormatColumn;
        const bool actions = index.column() == MediaModel::DurationColumn;
        const QRect textRect = actions ? content.adjusted(0, 0, -45, 0) : content;
        painter->setFont(Restyle::font(format ? 9 : 13, QFont::Normal, format ? 0.2 : 0));
        painter->setPen(format ? secondaryColor : textColor);
        const QString text = painter->fontMetrics().elidedText(index.data().toString(),
                                                               Qt::ElideRight, qMax(0, textRect.width()));
        painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, text);
        if (actions) {
            const QRect control = actionsRect(option.rect);
            Restyle::paintIcon(*painter, QRectF(control.center().x() - 7,
                                               control.center().y() - 7, 14, 14),
                              QStringLiteral("more"), secondaryColor);
        }
    }
    painter->setPen(theme.tableLine);
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());
    painter->restore();
}

QSize MediaLibraryDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const
{
    return QSize(index.column() == MediaModel::FileNameColumn ? 357 : 145, 38);
}

QRect MediaLibraryDelegate::selectionRect(const QRect &row)
{
    return QRect(row.left() + 8, row.center().y() - 6, 13, 13);
}

QRect MediaLibraryDelegate::actionsRect(const QRect &row)
{
    return QRect(row.right() - 36, row.center().y() - 14, 29, 29);
}

MediaLibraryDelegate::Control MediaLibraryDelegate::controlAt(
    const QModelIndex &index, const QRect &row, const QPoint &position)
{
    if (index.column() == MediaModel::FileNameColumn
        && selectionRect(row).adjusted(-5, -5, 5, 5).contains(position))
        return Control::Selection;
    if (index.column() == MediaModel::DurationColumn && actionsRect(row).contains(position))
        return Control::Actions;
    return Control::None;
}

bool MediaLibraryDelegate::editorEvent(QEvent *event, QAbstractItemModel *,
                                      const QStyleOptionViewItem &option, const QModelIndex &index)
{
    auto *view = qobject_cast<QAbstractItemView *>(parent());
    if (!view || !index.isValid() || !index.flags().testFlag(Qt::ItemIsEnabled))
        return false;
    auto toggle = [&] {
        const QModelIndex row = index.siblingAtColumn(MediaModel::FileNameColumn);
        view->selectionModel()->setCurrentIndex(row, QItemSelectionModel::NoUpdate);
        view->selectionModel()->select(row, QItemSelectionModel::Toggle | QItemSelectionModel::Rows);
    };
    if (event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Space && key->modifiers() == Qt::NoModifier) {
            if (!key->isAutoRepeat())
                toggle();
            return true;
        }
    }
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseButtonRelease)
        return false;
    const auto *mouse = static_cast<QMouseEvent *>(event);
    if (mouse->button() != Qt::LeftButton)
        return false;
    const Control control = controlAt(index, option.rect, mouse->position().toPoint());
    if (event->type() == QEvent::MouseButtonPress) {
        mSuppressRelease = false;
        mPressedIndex = QModelIndex();
        mPressedControl = control;
        if (control == Control::None)
            return false;
        mPressedIndex = index;
        view->setFocus(Qt::MouseFocusReason);
        // QAbstractItemView dispatches editorEvent before its ordinary selection
        // command; consuming the press retains other selected rows.
        return true;
    }
    const bool activate = mPressedIndex == index && mPressedControl == control;
    mPressedIndex = QModelIndex();
    mPressedControl = Control::None;
    if (activate && control == Control::Selection)
        toggle();
    else if (activate && control == Control::Actions)
        emit fileActionsRequested(index.siblingAtColumn(MediaModel::FileNameColumn),
                                  mouse->globalPosition().toPoint());
    return control != Control::None;
}

bool MediaLibraryDelegate::eventFilter(QObject *object, QEvent *event)
{
    auto *view = qobject_cast<QAbstractItemView *>(parent());
    if (!view || !view->isEnabled())
        return QStyledItemDelegate::eventFilter(object, event);
    if (object == view->viewport() && event->type() == QEvent::MouseButtonRelease && mSuppressRelease) {
        mSuppressRelease = false;
        return true;
    }
    if (object == view->viewport() && mPressedControl != Control::None) {
        if (event->type() == QEvent::MouseMove)
            return true;
        if (event->type() == QEvent::MouseButtonRelease) {
            const auto *mouse = static_cast<QMouseEvent *>(event);
            const QModelIndex index = view->indexAt(mouse->position().toPoint());
            if (index.isValid() && index == mPressedIndex) {
                QStyleOptionViewItem option;
                option.initFrom(view);
                option.rect = view->visualRect(index);
                editorEvent(event, view->model(), option, index);
            } else {
                mPressedIndex = QModelIndex();
                mPressedControl = Control::None;
            }
            // Qt otherwise applies its deferred ClearAndSelect even after a
            // handled editorEvent. Inline controls must retain native row state.
            return true;
        }
    }
    if (object == view->viewport() && event->type() == QEvent::MouseButtonDblClick) {
        const auto *mouse = static_cast<QMouseEvent *>(event);
        const QModelIndex index = view->indexAt(mouse->position().toPoint());
        if (index.isValid()
            && controlAt(index, view->visualRect(index), mouse->position().toPoint()) != Control::None) {
            // Qt emits doubleClicked before calling editorEvent. A double click
            // on a selection checkbox must never start preview playback.
            mPressedIndex = QModelIndex();
            mPressedControl = Control::None;
            mSuppressRelease = true;
            return true;
        }
    }
    if ((object == view || object == view->viewport()) && event->type() == QEvent::ContextMenu) {
        const auto *context = static_cast<QContextMenuEvent *>(event);
        const QModelIndex index = context->reason() == QContextMenuEvent::Keyboard
            ? view->currentIndex()
            : view->indexAt(view->viewport()->mapFromGlobal(context->globalPos()));
        if (index.isValid()) {
            const QPoint position = context->reason() == QContextMenuEvent::Keyboard
                ? view->viewport()->mapToGlobal(view->visualRect(index).bottomLeft())
                : context->globalPos();
            emit fileActionsRequested(index.siblingAtColumn(MediaModel::FileNameColumn), position);
            return true;
        }
    }
    return QStyledItemDelegate::eventFilter(object, event);
}

bool MediaLibraryDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view,
                                    const QStyleOptionViewItem &option, const QModelIndex &index)
{
    const Control control = controlAt(index, option.rect, event->pos());
    if (control != Control::None) {
        const QString text = control == Control::Actions
            ? tr("Действия с файлом (Shift+F10)")
            : view->selectionModel()->isSelected(index) ? tr("Снять выделение файла (Пробел)")
                                                       : tr("Выбрать файл (Пробел)");
        QToolTip::showText(event->globalPos(), text, view);
        return true;
    }
    return QStyledItemDelegate::helpEvent(event, view, option, index);
}
