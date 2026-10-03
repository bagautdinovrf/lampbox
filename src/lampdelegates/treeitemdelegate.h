#ifndef TREEITEMDELEGATE_H
#define TREEITEMDELEGATE_H

#include <QObject>
#include <QStyledItemDelegate>
#include <QIcon>

#include <unordered_map>

#include "lmtypedef.h"
#include "journallisten.h"
#include "lmobjects.h"
#include "playlist.h"
using namespace lampproject::lmobject;
using namespace lampproject;

class TreeItemDelegate: public QStyledItemDelegate
{
    Q_OBJECT
public:
    explicit TreeItemDelegate(QObject *parent = nullptr);
    ~TreeItemDelegate() override;
    QString displayText( const QVariant & value, const QLocale & locale ) const override;

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
    void updateEditorGeometry(QWidget *editor,
        const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint( QPainter * painter, const QStyleOptionViewItem & option,
                const QModelIndex & index ) const override;
    QSize sizeHint(const QStyleOptionViewItem &option,
                   const QModelIndex &index) const override;
private:
    ///
    /// \brief statusBoxs - список статусов станций
    ///
    std::unordered_map<MusicboxId,JournalListen::PlayerStatus> statusBoxs;
    std::unordered_map<MusicboxId, QString> nowPlayBoxs;
    ///
    /// \brief icons - иконки для дерева по типам объектов
    ///
    std::unordered_map<LMObjectType,QImage> icons;
    std::unordered_map<JournalListen::PlayerStatus,QImage> iconsBoxs;
    ///
    /// \brief iconsBoxs
    ///
    QMap<TypePlaylist,QString> plType;
    void makeIconList();
    QImage StatusItem(const LMObject* object);
    JournalListen::PlayerStatus status(const MusicboxId id) ;
    QString nowPlay(const MusicboxId id) ;
signals:
    void hasChanges();
public slots:
};

#endif // TREEITEMDELEGATE_H
