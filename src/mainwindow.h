

#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QMainWindow>
#include <QModelIndex>
#include <QMap>
#include <QPointer>


class QModelIndex;
class QFileSystemModel;
class QAction;
class QTableView;
class QTreeView;

class ChannelManager;
class ChannelModel;
class MediaModel;
class MediaManager;
class MediaController;
class Report;
class AdvertManager;


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();


    enum MainWindowPage { PAGE_MUSIC, PAGE_VIDEO, PAGE_ADVERT };

public slots:
    /**
     * @brief slotChangeChannel                 - Слот изменение текущего канала
     * @param index     - текущий индекс
     * @param prev      - предыдущий индекс
     */
    void slotChangeChannel(const QModelIndex &index, const QModelIndex &prev );

private:
    /**
     * @brief MainWindow::setInfoHeader         - Формирование информационного заголовка
     */
    void setInfoHeader();

    /**
     * @brief clearInfoHeader                   - Очистка информационного заголовка
     */
    void clearInfoHeader();

    /**
     * @brief clearFileInfoHeader
     */
    void clearFileInfoHeader();

    /**
     * @brief currentMediaManager
     * @return
     */
    MediaManager *currentPageMediaManager();

    /**
     * @brief currenChannelManager
     * @return
     */
    ChannelManager *currentPageChannelManager();

    /**
     * @brief currentTableView
     * @return
     */
    QTableView *currentTableView();

    /**
     * @brief currentMediaManagerTree
     * @return
     */
    QTreeView * currentPageMediaManagerTree();

private slots:
    /**
     * @brief channelDataChanged                - Измененние данных канала
     * @param index             - новое значение
     * @param cindex            - новое значение
     */
    void channelDataChanged(QModelIndex index, QModelIndex cindex);

    /**
     * @brief slot_addMediaFiles
     */
    void slot_addMediaFiles();
    void slot_removeMediaFiles();

    /**
     * @brief slot_addChannel
     */
    void slot_addChannel();
    void slot_deleteChannel();

    /**
     * @brief slot_addAdvert
     */
    void slot_addAdvert();
    void slot_deleteAdvert();

    /// Остановка воспроизведения
    void slot_stop();

    /// Запуск воспроизведения
    void slot_play();

    /// Установка состояния кнопок
    void setPlayingButtonsState(bool state);

    ///
    void slot_createReport();

    /**
     * @brief slot_settings             - Вызов окна отображения настроек
     */
    void slot_settings();

    /**
     * @brief changePage                - Изменение текущей страницы
     */
    void changePage(const int page);

    /**
     * @brief setDeleteButtonsState     - Установка состояний кнопок
     */
    /// Кнопка удаления каналов
    void setDeleteChannelButtonState();
    /// Кнопка удаления рекламы из таблицы
    void setDeleteButtonAdvertState();
    /// Кнопка добавление рекламы в таблицу
    void setAddButtonAdvertState();
    /// Кнопка удаления файлов из хранилища
    void setRemoveFilesButtonState();
    /// Кнопка добавления файлов в хранилище
    void setAddFilesButtonState();
    /// Кнопка "обновить"
    void setRefreshButtonState();

    /**
     * @brief slot_playTrack            - Воспроизведение текущей композиции
     */
    void slot_playTrack(QModelIndex index);

    /**
     * @brief changeFileFormat          - Изменение доступных форматов файлов
     */
    void changeFileFormats();

    /**
     * @brief about -                   - О программе
     */
    void about();

    /**
     * @brief setSelectFileInfoHeader   - Информация о фалах
     * @param index
     * @param i
     */
    void setSelectFileInfoHeader(QModelIndex index, QModelIndex i = QModelIndex() );

    /**
     * @brief selectionFileInfoHeaderChanged    - Информация о файлах
     */
    void selectionFileInfoHeaderChanged();

    /**
     * @brief copyFiles                 - Копирование файлов
     * @param fileList
     */
    void copyFiles(const QStringList &fileList);

private:
    Ui::MainWindow *ui;

    ChannelManager          *mChannelManagerMusic;
    ChannelManager          *mChannelManagerVideo;
    ChannelModel            *mChannelModelMusic;
    ChannelModel            *mChannelModelVideo;
    MediaModel              *mMediaModelMusic;
    MediaModel              *mMediaModelVideo;

    ///
    QScopedPointer<MediaManager>        mMediaAdvertManager;
    QScopedPointer<AdvertManager>       mAdvertManager;

    QFileSystemModel        *mFileSystemModel_;

//    QModelIndex             mCurrentIndex;

    QAction                 *mAct_stop;
    QAction                 *mAct_play;
    QAction                 *mAct_music;
    QAction                 *mAct_ads;
    QAction                 *mAct_video;
    QAction                 *mAct_RefreshTimetable;

    //
    QPointer<QTimer>        mStopPlayTimer;

    ///
    MediaController         *mMediaController;

    /// Форма отчетов
    Report                            *mReport;

    /// Текущее окно
    MainWindowPage                     mPage;

    ///
    bool                                mFileInfoHeaderIsClear;

};

#endif // MAINWINDOW_H
