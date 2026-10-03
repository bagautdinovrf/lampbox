#ifndef MEDIALIBRARYDELEGATE_H
#define MEDIALIBRARYDELEGATE_H

#include <QStyledItemDelegate>
#include <QPersistentModelIndex>

// A compact, native model/view row. Identity, selection and keyboard operations
// stay with MediaModel/QItemSelectionModel; only the presentation is customized.
class MediaLibraryDelegate final : public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit MediaLibraryDelegate(QObject *parent = nullptr);

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view,
                   const QStyleOptionViewItem &option, const QModelIndex &index) override;

signals:
    void fileActionsRequested(const QModelIndex &index, const QPoint &globalPosition);

protected:
    bool eventFilter(QObject *object, QEvent *event) override;

private:
    enum class Control { None, Selection, Actions };
    static QRect selectionRect(const QRect &row);
    static QRect actionsRect(const QRect &row);
    static Control controlAt(const QModelIndex &index, const QRect &row, const QPoint &position);
    QPersistentModelIndex mPressedIndex;
    Control mPressedControl = Control::None;
    bool mSuppressRelease = false;
};

#endif
