#ifndef DELEGATEDATETIMEPERIOD_H
#define DELEGATEDATETIMEPERIOD_H

#include <QWidget>
#include <QStyledItemDelegate>
#include <QListWidgetItem>

namespace Ui {
class DelegateDateTimePeriod;
}

namespace lampproject{
    namespace delegate {
    enum DataTimePeriodDelegateType{
        hour,
        minute,
        day
    };

    enum GroupFormat{
        Group,
        GroupUserFriendly
    };
    class TimePeriodListWidgetItem: public QListWidgetItem {
    public:
        TimePeriodListWidgetItem(QString text, QListWidget *view =0,int type = Type);
        bool operator< ( const QListWidgetItem & other ) const;
    };

    /// Класс делегата
    class DelegateDateTimePeriodEdit : public QStyledItemDelegate
    {
        Q_OBJECT
        DataTimePeriodDelegateType mDelegateType;
        bool autoGroup;
        QString userFriendly(QStringList item) const;
        QStringList groupItem(QStringList value, GroupFormat fromat = Group) const;
        QMap<DataTimePeriodDelegateType, QString> foreverText;
    public:
        DelegateDateTimePeriodEdit(DataTimePeriodDelegateType delegateType = hour, QObject *parent = 0);
        QString displayText( const QVariant & value, const QLocale & locale ) const;
        QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                              const QModelIndex &index) const;

        void setEditorData(QWidget *editor, const QModelIndex &index) const;
        void setModelData(QWidget *editor, QAbstractItemModel *model,
                          const QModelIndex &index) const;
        bool editorEvent(QEvent * event, QAbstractItemModel * model,
                         const QStyleOptionViewItem & option, const QModelIndex & index);

        void updateEditorGeometry(QWidget *editor, const QStyleOptionViewItem &option, const QModelIndex &index) const;

        void setType(DataTimePeriodDelegateType type);
    private:
        QWidget* drawParent;
    public slots:
        void commitAndCloseEditor();
    signals:
        void setDataMultiple(QString value) const;
    };

    /// Виджет
        class DelegateDateTimePeriod : public QWidget
        {
            Q_OBJECT
            DataTimePeriodDelegateType type;
            QMap<DataTimePeriodDelegateType, QString> format;
            QMap<DataTimePeriodDelegateType, int> maximum;
            QMap<DataTimePeriodDelegateType, int> minimum;
            QMap<DataTimePeriodDelegateType, QString> label;
        public:
            explicit DelegateDateTimePeriod(QWidget *parent = 0);
            ~DelegateDateTimePeriod();
            QString getValue();
            bool setValue(QString value);
            void setType(DataTimePeriodDelegateType type);
        private slots:

            void on_pushButtonAddOne_clicked();

            void on_pushButtonAddStartEnd_clicked();

            void on_listWidget_clicked(const QModelIndex &index);

            void on_pushButtonAccept_clicked();
            void on_pushButtonCancel_clicked();
            void setElementsState(int);

        signals:
            void accept();
            void reject();
        private:
            Ui::DelegateDateTimePeriod *ui;
            bool allreadyInList(QString element);
            void checkList();
            QStringList convertItem(QString item);
            QStringList removeDuplicate(QStringList value);
        };
    }
}
#endif // DELEGATEDATETIMEPERIOD_H
