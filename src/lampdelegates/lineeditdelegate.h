#ifndef LINEEDITDELEGATE_H
#define LINEEDITDELEGATE_H
#include <QStyledItemDelegate>
#include <QLineEdit>
#include <QCompleter>
#include "types.h"

namespace lampproject{
    namespace delegate {

class LineEditDelegate : public QStyledItemDelegate{
    Q_OBJECT
  public:
  LineEditDelegate(QObject  *parent = 0);
  virtual ~LineEditDelegate();
  QString displayText( const QVariant & value, const QLocale & locale ) const;

  void updateEditorGeometry(QWidget *editor,
               const QStyleOptionViewItem &option, const QModelIndex &/* index */) const;

  void setEditorData(QWidget *editor, const QModelIndex &index) const;

  void setModelData(QWidget *editor, QAbstractItemModel *model, const QModelIndex &index) const;

  QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                         const QModelIndex &index) const;
signals:
    void setDataMultiple(QString value) const;
private slots:
   void commitAndCloseEditor();
private :
};

} // namespace lamproject
} // namespace delegates

#endif // LINEEDITDELEGATE_H
