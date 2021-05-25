#ifndef COMBOBOXGENREDELEGATE_H
#define COMBOBOXGENREDELEGATE_H

#include <QStyledItemDelegate>
#include <unordered_map>
#include <memory>

#ifdef LMOBJECT
    #include "lmtypedef.h"
    #include "lmobjects.h"
    using namespace lampproject;
    using namespace lampproject::lmobject;
#else
    typedef int LMObjectType;
#endif

class ComboBoxDelegate : public QStyledItemDelegate
{
Q_OBJECT
public:
    ComboBoxDelegate(LMObjectType type, QObject* parent =0);
    ~ComboBoxDelegate();
    QString displayText( const QVariant & value, const QLocale & locale ) const;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const;

    void setEditorData(QWidget *editor, const QModelIndex &index) const;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const;

    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const;
    void paint( QPainter * painter, const QStyleOptionViewItem & option,
                const QModelIndex & index ) const;
private:
    LMObjectType objectType;
    std::unordered_map<LMObjectId,std::unique_ptr<LMObject>> cacheObject;
    LMObject* get(LMObjectId id);
};

#endif // COMBOBOXGENREDELEGATE_H
