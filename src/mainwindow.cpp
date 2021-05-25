

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "settingsdialog.h"
#include "aboutlampbox.h"

// STL
#include <functional>
#include <iostream>

// Классы
#include "channelmanager.h"
#include "channelmodel.h"
#include "mediamanager.h"
#include "mediamodel.h"
#include "timedelegate.h"
#include "simplelineeditdelegate.h"
#include "mediacontroller.h"
#include "report.h"
#include "settings.h"
#include "volumedelegate.h"
#include "advertmanager.h"
#include "advertmodel.h"
#include "dateeditdelegate.h"
#include "delegatedatetimeperiod.h"
#include "trackfullinfo.h"
#include "qcrongenerator.h"
#include "delegatevolumeedit.h"
#include "delegatedaysoftheweek.h"
#include "stationmanager.h"
#include "informer.h"
#include "maininformwidget.h"
#include "delegatedaysoftheweek.h"
#include "delegatemonthedit.h"
#include "trialmessagebox.h"

// Qt
#include <QDir>
#include <QDebug>
#include <QModelIndex>
#include <QSharedMemory>
#include <QProgressDialog>
#include <QPointer>
#include <QMessageBox>
#include <QTimer>
#include <QLabel>
//#include <QThread>
#include <QSignalMapper>
#include <QTreeView>
#include <QTableView>
#include <QFileSystemModel>


using namespace lampbox;
using namespace lampproject::delegate;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    mReport(nullptr),
    mPage(PAGE_MUSIC),
    mFileInfoHeaderIsClear(false)
{
    ui->setupUi(this);

    // скрываем сообщения
    ui->InfoWidget->hide();
    // Скрываем статус бар
    ui->statusBar->hide();
//    ui->mainToolBar->setContextMenuPolicy(Qt::PreventContextMenu);
    ui->pb_addFiles->setVisible(false);

    // Drops - Копирование через драг энд дроп (Drag and Drop )
    ui->tree_music->setAcceptDrops(true);
    ui->tree_music->setDropIndicatorShown(true);
    ui->tree_music->setDragDropMode( QAbstractItemView::DropOnly );

    setWindowTitle(tr("Lampmedia LampBox %1 [%2]")
                   .arg(VERSION)
                   .arg(StationManager::Instance().typeText()));

    SPathData path;
    QDir::setCurrent(path.homePathDir);

    /// ToolBar settings
    ui->mainToolBar->setIconSize(QSize(64, 64));

    QLabel *label;

    // Пространство перед кнопками музыки и рекламы
    label = new QLabel(ui->mainToolBar);
    label->setFixedHeight(20);
    ui->mainToolBar->addWidget(label);

    mAct_music = ui->mainToolBar->addAction(QIcon(":/player/music.png"), "Музыка");
    mAct_music->setCheckable(true);
    mAct_music->setChecked(true);

    // Пространство
    label = new QLabel(ui->mainToolBar);
    label->setFixedHeight(1);
    ui->mainToolBar->addWidget(label);

    mAct_video = ui->mainToolBar->addAction(QIcon(":/player/video.png"), "Видео");
    mAct_video->setCheckable(true);
    mAct_video->setChecked(false);

    // Пространство
    label = new QLabel(ui->mainToolBar);
    label->setFixedHeight(1);
    ui->mainToolBar->addWidget(label);

    mAct_ads = ui->mainToolBar->addAction(QIcon(":/player/advert.png"), "Реклама");
    mAct_ads->setCheckable(true);
    mAct_ads->setChecked(false);

    // Пространство после рекламы
    label = new QLabel(ui->mainToolBar);
    label->setFixedHeight(20);
    ui->mainToolBar->addWidget(label);

    // Кнопка обновления
    mAct_RefreshTimetable = ui->mainToolBar->addAction(QIcon(":/player/refresh_y.png"), "Обновить");
    mAct_RefreshTimetable->setToolTip("Перезапустить плеер");
//    mAct_RefreshTimetable->setIconText("Обновить");

    // Разделение рекламы и упраляющих кнопок, сдвигает кнопки вниз
    label = new QLabel(ui->mainToolBar);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mainToolBar->addWidget(label);

    ui->mainToolBar->addSeparator();

    // Кнопки управления Плей и Стоп
    mAct_play = ui->mainToolBar->addAction(QIcon(":/player/play.png"), "");
    mAct_play->setToolTip("Запустить воспроизведение по расписанию");

    mAct_stop = ui->mainToolBar->addAction(QIcon(":/player/stop.png"), "");
    mAct_stop->setToolTip("Остановить воспроизведение по расписанию");

    // Пространство после кнопки СТОП
    label = new QLabel(ui->mainToolBar);
    label->setFixedHeight(30);
    ui->mainToolBar->addWidget(label);

    // Устанавливаем первую страницу по умолчанию
    ui->sw_->setCurrentIndex(0);

    // connects
    connect( mAct_play,                 SIGNAL( triggered() ),  SLOT( slot_play()) );
    connect( mAct_stop,                 SIGNAL( triggered() ),  SLOT( slot_stop()) );

    connect( ui->act_Exit,              SIGNAL(triggered()),    qApp,    SLOT( quit() ) );
    connect( ui->act_report,            SIGNAL(triggered()),    SLOT( slot_createReport() ) );
    connect( ui->act_settings,          SIGNAL(triggered()),    SLOT( slot_settings() ));

    //connect( ui->pb_addFiles,           SIGNAL( clicked( )),    SLOT( slot_addMediaFiles() ) );
    connect( ui->pb_removeFiles,        SIGNAL( clicked( )),    SLOT( slot_removeMediaFiles() ) );

    connect(ui->pb_addChannel,          SIGNAL(clicked() ),     SLOT( slot_addChannel()) );
    connect(ui->pb_addVideoChannel,     SIGNAL(clicked() ),     SLOT( slot_addChannel()) );
    connect(ui->pb_deleteChannel,       SIGNAL(clicked() ),     SLOT( slot_deleteChannel()));
    connect(ui->pb_deleteVideoChannel,  SIGNAL(clicked() ),     SLOT( slot_deleteChannel()) );

    connect(ui->pb_addAdvert,           SIGNAL( clicked() ),    SLOT( slot_addAdvert()) );
    connect(ui->pb_delAdvert,           SIGNAL( clicked() ),    SLOT( slot_deleteAdvert()) );
    // Вывод окна "О программе"
    connect(ui->act_about,              SIGNAL(triggered()),    SLOT( about() ) );

    // Подключение информера
    connect( &Informer::Instance(),     SIGNAL( infoEventSignal( QString ) ), ui->InfoWidget, SLOT(inform(QString)) );

    // Формируем карту сигналов
    QSignalMapper  *mapper = new QSignalMapper(this);
    mapper->setMapping( mAct_music,     (int)PAGE_MUSIC );
    mapper->setMapping( mAct_ads,       (int)PAGE_ADVERT );
    mapper->setMapping( mAct_video,     (int)PAGE_VIDEO );

    connect( mAct_music,    SIGNAL( triggered() ), mapper,  SLOT( map() ) );
    connect( mAct_ads,      SIGNAL( triggered() ), mapper,  SLOT( map() ) );
    connect( mAct_video,    SIGNAL( triggered() ), mapper,  SLOT( map() ) );
    connect( mapper,        SIGNAL( mapped(int) ), SLOT( changePage(int) ) );

    /// Установка делегатов в таблицу музыки
    ui->tableCnannelMusic->setItemDelegateForColumn(0, new SimpleLineEditDelegate(ui->tableCnannelMusic) );
    ui->tableCnannelMusic->setItemDelegateForColumn(1, new TimeDelegate(ui->tableCnannelMusic) );
    ui->tableCnannelMusic->setItemDelegateForColumn(2, new TimeDelegate(ui->tableCnannelMusic) );
    ui->tableCnannelMusic->setItemDelegateForColumn(3, new DelegateDayOfWeekEdit(ui->tableCnannelMusic) );
    ui->tableCnannelMusic->setItemDelegateForColumn(4, new DelegateDateTimePeriodEdit(day, ui->tableCnannelMusic) );
    ui->tableCnannelMusic->setItemDelegateForColumn(5, new DelegateMonthEdit(ui->tableCnannelMusic) );
    ui->tableCnannelMusic->setItemDelegateForColumn(6, new DelegateVolumeEdit(ui->tableCnannelMusic));

    /// Установка делегатов в таблицу видео
    ui->tableChannelVideo->setItemDelegateForColumn(0, new SimpleLineEditDelegate(ui->tableChannelVideo));
    ui->tableChannelVideo->setItemDelegateForColumn(1, new TimeDelegate(ui->tableChannelVideo) );
    ui->tableChannelVideo->setItemDelegateForColumn(2, new TimeDelegate(ui->tableChannelVideo) );
    ui->tableChannelVideo->setItemDelegateForColumn(3, new DelegateDayOfWeekEdit(ui->tableChannelVideo));
    ui->tableChannelVideo->setItemDelegateForColumn(4, new DelegateDateTimePeriodEdit(day, ui->tableChannelVideo) );
    ui->tableChannelVideo->setItemDelegateForColumn(5, new DelegateMonthEdit(ui->tableChannelVideo));
    ui->tableChannelVideo->setItemDelegateForColumn(6, new DelegateVolumeEdit(ui->tableChannelVideo));

    /// Установка делегатов в таблицу рекламы
    // Часы запуска
    DelegateDateTimePeriodEdit *timePeriodDelegateHours = new DelegateDateTimePeriodEdit(hour, ui->tableAdvert);
    ui->tableAdvert->setItemDelegateForColumn( 1, timePeriodDelegateHours );

    // Минуты запуска
    DelegateDateTimePeriodEdit *timePeriodDelegateMinuts = new DelegateDateTimePeriodEdit(minute, ui->tableAdvert);
    ui->tableAdvert->setItemDelegateForColumn( 2, timePeriodDelegateMinuts );

    // Дни недели
    DelegateDayOfWeekEdit *dayOfWeekDelegate = new DelegateDayOfWeekEdit(ui->tableAdvert);
    ui->tableAdvert->setItemDelegateForColumn(3, dayOfWeekDelegate);

    // Начало и конец периода
    DateEditDelegate *dateEditDelegate = new DateEditDelegate(ui->tableAdvert);
    ui->tableAdvert->setItemDelegateForColumn(4, dateEditDelegate );
    ui->tableAdvert->setItemDelegateForColumn(5, dateEditDelegate );

    // Громкость
    DelegateVolumeEdit*advertVolume = new DelegateVolumeEdit(ui->tableAdvert);
    ui->tableAdvert->setItemDelegateForColumn(6, advertVolume);

/// Музыка
    // Список каналов
    mChannelManagerMusic = new ChannelManager(MUSIC);
    mChannelManagerMusic->collectChannels();
    mChannelModelMusic = new ChannelModel( mChannelManagerMusic, this );
    connect(mChannelModelMusic, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(channelDataChanged(QModelIndex,QModelIndex)) );
    mChannelManagerMusic->setChannelModel(mChannelModelMusic);
    ui->tableCnannelMusic->setModel(mChannelModelMusic);

    mMediaModelMusic = new MediaModel(0, MUSIC, this );
    ui->tree_music->setModel(mMediaModelMusic);

//    connect( mMediaModelMusic, SIGNAL(dropFileList(QStringList)), SLOT() );

/// Видео
    //
    mChannelManagerVideo = new ChannelManager(VIDEO);
    mChannelManagerVideo->collectChannels();
    mChannelModelVideo = new ChannelModel( mChannelManagerVideo, this );
    connect(mChannelModelVideo, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT(channelDataChanged(QModelIndex,QModelIndex)) );
    mChannelManagerVideo->setChannelModel(mChannelModelVideo);
    ui->tableChannelVideo->setModel(mChannelModelVideo);

    mMediaModelVideo = new MediaModel(0, VIDEO, this );
    if( mChannelManagerVideo->channelCount() ) {
        mMediaModelVideo->setMediaManager(&mChannelManagerVideo->channel(0).mediaManager() );
        mChannelManagerVideo->setCurrentChannel(0);
    }
    ui->tree_video->setModel(mMediaModelVideo);

/// Реклама
    // Список доступных рекламных треков
    mMediaAdvertManager.reset( new MediaManager(/*SPathData().homePathDir + */SPathData().advertDir, ADVERT) );
    MediaModel *advertTrackModel = new MediaModel(mMediaAdvertManager.data(), ADVERT, this);
    mMediaAdvertManager->setMediaModel(advertTrackModel);
    ui->tree_advert->setModel(advertTrackModel);

    // Расписание рекламы
    mAdvertManager.reset( new AdvertManager() );
    AdvertModel *advertModel = new AdvertModel( mAdvertManager.data(), this );
//    connect(advertModel, SIGNAL(dataChanged(QModelIndex,QModelIndex)), SLOT( generateCron() ) );
    connect(mAdvertManager.data(), SIGNAL(beginCollect()), advertModel, SLOT(beginReset()) );
    connect(mAdvertManager.data(), SIGNAL(endCollect()), advertModel, SLOT(endReset()) );
    ui->tableAdvert->setModel(advertModel);

    // Таблица музыки
    connect(ui->tableCnannelMusic->selectionModel(), SIGNAL(currentRowChanged(const QModelIndex&, const QModelIndex&)),
            SLOT(slotChangeChannel(const QModelIndex&, const QModelIndex&)) );
    connect(ui->tableChannelVideo->selectionModel(), SIGNAL(currentRowChanged(const QModelIndex&, const QModelIndex&)),
            SLOT(slotChangeChannel(const QModelIndex&, const QModelIndex&)) );

    // Проигрывание текущего трека по двойному клику
    connect(ui->tree_music,       SIGNAL(doubleClicked(QModelIndex)), SLOT(slot_playTrack(QModelIndex) ));
    connect(ui->tree_advert,      SIGNAL(doubleClicked(QModelIndex)), SLOT(slot_playTrack(QModelIndex) ));

    // выделяем первую строку в таблице
//    if( mChannelManagerMusic->channelCount() )
        ui->tableCnannelMusic->selectRow(0);
//    if( mChannelManagerVideo->channelCount() )
        ui->tableChannelVideo->selectRow(0);

    /// Самый правый список файлов
//    QFileSystemModel - настройка папки куда монтируются флешки
//    mFileSystemModel_ = new QFileSystemModel(ui->tw_fileSystem);
//    QModelIndex fileIndex = mFileSystemModel_->setRootPath(SPathData().mediaPath);
//    mFileSystemModel_->setNameFilterDisables(false);
//    mFileSystemModel_->setNameFilters( Settings().availablelAllFileFormats() );
//    ui->tw_fileSystem->setRootIndex(fileIndex);
//    ui->tw_fileSystem->setModel(mFileSystemModel_);
//    ui->tw_fileSystem->setColumnWidth(0, 200);
//    ui->tw_fileSystem->setColumnHidden(2, true);
//    ui->tw_fileSystem->setColumnHidden(3, true);

    //
    mMediaController = new MediaController(this);
    connect( mMediaController,          SIGNAL(playingState(bool)), SLOT( setPlayingButtonsState(bool) ) );
    // Показать/Скрыть плеер
    connect(ui->action_playerHide,      SIGNAL(triggered()), mMediaController,  SLOT( slot_playerHide() ));
    connect(ui->action_playerShow,      SIGNAL(triggered()), mMediaController,  SLOT( slot_playerShow() ));
    // Обновление расписания плеера
    connect( ui->act_updateTimetable,   SIGNAL( triggered()), mMediaController, SLOT( refreshPlayer() ) );
    connect( mAct_RefreshTimetable,     SIGNAL( triggered()), mMediaController, SLOT( refreshPlayer() ) );

///
/// Установка состояний кнопок
///
    // Установка состояний управляющих кнопок
    setPlayingButtonsState( mMediaController->isPlaying() );

    // Кнопка удаления каналов
    setDeleteChannelButtonState();
    // Кнопка удаления рекламы
    setDeleteButtonAdvertState();
    // Кнопка добавления рекламы
    setAddButtonAdvertState();
    // Кнопка удалить файлы
    setRemoveFilesButtonState();
    // Кнопка обновить
    setRefreshButtonState();
    // Кнопка добавления файлов в хранилище
    setAddFilesButtonState();

    connect(ui->tree_music->selectionModel(),   SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(setRemoveFilesButtonState()) );
    connect(ui->tree_advert->selectionModel(),  SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(setRemoveFilesButtonState()) );
    connect(ui->tree_video->selectionModel(),   SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(setRemoveFilesButtonState()) );

    connect(ui->tree_advert->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            SLOT(setAddButtonAdvertState()) );

/// Обновление информации о файле
    // Музыка
    connect( ui->tree_music->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
             SLOT( setSelectFileInfoHeader(QModelIndex,QModelIndex)) );
    connect( ui->tree_music->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
             SLOT( selectionFileInfoHeaderChanged() ) );
    // Реклама
    connect( ui->tree_advert->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
             SLOT( setSelectFileInfoHeader(QModelIndex,QModelIndex)) );
    connect( ui->tree_advert->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
             SLOT( selectionFileInfoHeaderChanged() ) );
    // Видео
    connect( ui->tree_video->selectionModel(), SIGNAL(currentRowChanged(QModelIndex,QModelIndex)),
             SLOT( setSelectFileInfoHeader(QModelIndex,QModelIndex)) );
    connect( ui->tree_video->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
             SLOT( selectionFileInfoHeaderChanged() ) );

} // Конец конструктора


MainWindow::~MainWindow()
{
    delete ui;
    delete mChannelManagerMusic;
}


void MainWindow::slotChangeChannel(const QModelIndex &index, const QModelIndex &prev)
{
    if(index == prev)
        return;
    if(!index.isValid() )
        return;
    ChannelManager *manager = currentPageChannelManager();
    if(!manager)
        return;
    manager->setCurrentChannel(index.row());
    ChannelData &channelData = manager->currentChannel();
    if( channelData.isValid() )
        ((MediaModel*)currentPageMediaManagerTree()->model())->setMediaManager( &channelData.mediaManager() );

    ///Header
    setInfoHeader();
    // Кнопка удаления медиафайлов
    setRemoveFilesButtonState();
    // Очистка информации о файле
    clearFileInfoHeader();
}

/// Копирование из QFileSystemModel
void MainWindow::slot_addMediaFiles()
{
    // Если не выбран канал и страница музыки
    if( !ui->tableCnannelMusic->currentIndex().isValid() && PAGE_MUSIC == mPage )
        return;

    // Список объектов для копирования
//    QList<QModelIndex> list = ui->tw_fileSystem->selectionModel()->selectedRows();
//    if(list.isEmpty())
//        return;

    // Формирование путей файлов для копирования
//    QStringList pathList;
//    for( int i = 0; i < list.size(); ++i )
//        pathList << mFileSystemModel_->filePath( list[i] );

//    copyFiles(pathList);
}


void MainWindow::copyFiles(const QStringList &fileList)
{
    // Формирование списка файлов из выбранных директорий
    QStringList files( SPathData::addFiles(fileList) );
    if( files.isEmpty() )
        return;

    MediaManager *mediaManager = currentPageMediaManager();
    if(!mediaManager)
        return;

    if(StationManager::Instance().trial() ) {
        if( mediaManager->count() > 9 ) {
            TrialMessageBox("Вы можете добавить не больше 10 файлов.");
            return;
        }
    }

    int numFiles = files.size();

    QString labelText = "Копирование файлов...";
    QPointer<QProgressDialog> progress = new QProgressDialog(labelText, "Отменить", 0, numFiles, this);
    progress->setWindowModality(Qt::WindowModal);
    progress->show();
    qApp->processEvents();
    for( int i = 0; i < numFiles; ++i ) {
        qApp->processEvents();
        progress->setValue(i);
        qApp->processEvents();
        if( progress->wasCanceled() )
            break;
        qApp->processEvents();
        QString currentFile = files.at(i).section("/", -1);
        if( currentFile.length() > 25 )
            currentFile = currentFile.left(25) + "...";
        progress->setLabelText(labelText + "\n" + currentFile);
        qApp->processEvents();
        mediaManager->addFile(files.at(i));
        qApp->processEvents();
        mediaManager->collectMediaFiles();
        qApp->processEvents();
    }
    progress->setValue(numFiles);

    // Информационный заголовок
    setInfoHeader();
    // Кнопка добавления рекламы
    setAddButtonAdvertState();
    // Кнопка удаления файлов
    setRemoveFilesButtonState();
    //
    clearFileInfoHeader();
}


void MainWindow::slot_removeMediaFiles()
{
    MediaManager *mediaManager   = currentPageMediaManager();
    QTreeView    *treeView       = currentPageMediaManagerTree();

    if( !mediaManager || !treeView )
        return;

    QList<QModelIndex> list = treeView->selectionModel()->selectedRows();
    if(list.isEmpty())
        return;

    // Проверка на использование файлов в расписании
    if( PAGE_ADVERT == mPage) {
        QStringList  advertList;
        for( int row = 0; row < ui->tableAdvert->model()->rowCount(); ++row )
            advertList << ui->tableAdvert->model()->index(row, 0).data().toString();

        QStringList   advertFiles;
        for( int i = 0; i < list.size(); ++i ) {
            QString fileName = list.at(i).data().toString();
            if( advertList.contains( fileName ) )
                advertFiles << fileName;
        }

        if( !advertFiles.isEmpty() ) {
            // Ограничеие: не более 15 файлов выводится в сообщении
            QMessageBox::warning(this, "Реклама", "Эти файлы используются в расписании!\n\n"
                                        + ((QStringList)(advertFiles.mid(0, 15))).join('\n'));
            return;
        }
    }

    foreach (QModelIndex index, list) {
        if( !index.isValid() )
            continue;
            mediaManager->delFile(index.data().toString());
    }

    // Обновление медиафайлов
    mediaManager->collectMediaFiles();
    // Информационный заголовок
    setInfoHeader();
    // Кнопка добавления рекламы
    setAddButtonAdvertState();
    // Кнопка удаления файлов
    setRemoveFilesButtonState();
    //
    clearFileInfoHeader();
}


void MainWindow::slot_addChannel()
{
    ChannelManager *manager = currentPageChannelManager();
    if(!manager)
        return;

    manager->createChannel();
    // Выбор текущим каналом последний добавленный
    QTableView *table = currentTableView();
    if(table)
        currentTableView()->selectRow(manager->channelCount() - 1);
    // Установка состояния кнопоки удалить
    setDeleteChannelButtonState();
    // Кнопка копирования файлов
    setAddFilesButtonState();
    // Кнопка обновить
    setRefreshButtonState();
}


void MainWindow::slot_deleteChannel()
{
    QTableView *table = currentTableView();
    if(!table)
        return;

    if( !table->currentIndex().isValid() )
        return;

    ChannelManager *manager = currentPageChannelManager();
    if(!manager)
        return;

    if( manager->currentChannel().mediaManager().mediaCount() ) {
        QPointer<QMessageBox> mbox = new QMessageBox;
        mbox->setWindowTitle(windowTitle());
        mbox->addButton("Да", QMessageBox::YesRole);
        QAbstractButton *no = mbox->addButton("Нет", QMessageBox::NoRole);
        mbox->setDefaultButton(qobject_cast<QPushButton*>(no));
        mbox->setText("Вы действительно хотите удалить канал <b>"+ manager->currentChannel().channelName()+"</b><br /> \n со всем его содержимым?");
        mbox->exec();
        if( mbox->clickedButton() == no )
            return;
    }
    // Удаление канала
    if( !manager->deleteCurrentChannel() ) {
        Informer::Instance().infoEvent("Невозможно удалить плейлист! Остановите медиастанцию и попробуйте снова!");
        manager->currentChannel().mediaManager().collectMediaFiles();
        return;
    }

    if( !manager->channelCount() )
        setInfoHeader();

    // Выбор текущего канала после удаления
    int currentRow;
    if( manager->currentChannelNum() >= manager->channelCount() )
        currentRow = manager->channelCount() - 1;
    else
        currentRow = manager->currentChannelNum();
    table->selectRow( currentRow );

    // Установка состояния кнопоки удалить
    setDeleteChannelButtonState();
    // Кнопка удаления файлов
    setRemoveFilesButtonState();
    // Кнопка копирования файлов
    setAddFilesButtonState();
    // Кнопка обновить
    setRefreshButtonState();
    // Очистка информационного заголовка
    clearFileInfoHeader();
}


void MainWindow::slot_addAdvert()
{
    QModelIndexList list = ui->tree_advert->selectionModel()->selectedRows();
    if(list.isEmpty())
        return;

    if( StationManager::Instance().trial() ) {
        if( mAdvertManager->count() > 1 ) {
            TrialMessageBox();
            return;
        }
    }

    foreach (QModelIndex index, list) {
        if(index.isValid()) {
            mAdvertManager->addAdvert(mMediaAdvertManager->mediaData( index.row() ).fileName());
//            qDebug() << mMediaAdvertManager->mediaData(index.row()).fileName();
        }
    }

    // Выбор последней добавленную рекламу
    ui->tableAdvert->selectRow(mAdvertManager->count()-1);
    // Установка состояния кнопоки удалить
    setDeleteButtonAdvertState();
    // Кнопка обновить
    setRefreshButtonState();
}


void MainWindow::slot_deleteAdvert()
{
    QModelIndexList list =  ui->tableAdvert->selectionModel()->selectedRows();
    if(list.isEmpty())
        return;
//qDebug() << list.size();
    foreach (QModelIndex index, list) {
        if(index.isValid())
            mAdvertManager->delAdvert( index.row() );
    }
    // Выбор последней добавленную рекламу
    ui->tableAdvert->selectRow(mAdvertManager->count()-1);
    // Установка состояния кнопоки удалить
    setDeleteButtonAdvertState();
    // Кнопка обновить
    setRefreshButtonState();
}


void MainWindow::slot_stop()
{
    setPlayingButtonsState(false);
    mMediaController->stop();
}


void MainWindow::slot_play()
{
    setPlayingButtonsState(true);
    mMediaController->play();
}


void MainWindow::setPlayingButtonsState(bool state)
{
    mAct_play->setDisabled(state);
    mAct_stop->setEnabled(state);
}


void MainWindow::slot_createReport()
{
    if( !mReport )
        mReport = new Report(this, Qt::Dialog);

    mReport->show();
}


void MainWindow::changeFileFormats()
{
//    mFileSystemModel_->setNameFilters(Settings().availablelAllFileFormats());
}


void MainWindow::changePage( const int page )
{
    bool act_music,
         act_advert,
         act_video;

    switch (page) {
        case PAGE_MUSIC:
            act_music = true;
            act_advert = false;
            act_video = false;
        break;

        case PAGE_ADVERT:
            act_music = false;
            act_advert = true;
            act_video = false;
        break;

        case PAGE_VIDEO:
            act_music = false;
            act_advert = false;
            act_video = true;
        break;
    }

    mAct_music->setChecked(act_music);
    mAct_ads->setChecked(act_advert);
    mAct_video->setChecked(act_video);
    mPage = (MainWindowPage)page;

    if(page == ui->sw_->currentIndex() )
        return;

    // Информационный заголовок
    setInfoHeader();
    // Кнопка удаления файлов
    setRemoveFilesButtonState();
    // Кнопка копирования файлов
    setAddFilesButtonState();
    // Кнопка удаления канала
    setDeleteChannelButtonState();
    //
    QTreeView *view = currentPageMediaManagerTree();
    if(view)
        setSelectFileInfoHeader( view->currentIndex() );
    // Установка текущей страницы
    ui->sw_->setCurrentIndex(page);
}

/**
 * @brief slot_playTrack                    - Воспроизведение текущей композиции
 */
void MainWindow::slot_playTrack(QModelIndex index)
{
    QString track = index.data().toString();
    QString path;
    if( PAGE_MUSIC == mPage ) {
        path = SPathData().channelDirMusic + mChannelManagerMusic->currentChannel().channelName();
    } else if( PAGE_ADVERT == mPage ) {
        path = SPathData().advertDir;
    }
    // Провеврка стоит ли в конце строки разделитель директории или нет
    // Если нет, то добавляем
    if(QChar('\\') != path.right(1) || QChar('/') != path.right(1))
        path.append( QDir::separator() );
    mMediaController->playTrack(path + track);
}

/**
 * @brief channelDataChanged                - Измененние данных канала
 * @param index             - новое значение
 * @param cindex            - новое значение
 */
void MainWindow:: channelDataChanged(QModelIndex index, QModelIndex cindex)
{
Q_UNUSED(cindex)
    if( 0 != index.column() )
        return;

    setInfoHeader();
}

ChannelManager *MainWindow::currentPageChannelManager()
{
    ChannelManager *manager = nullptr;
    if( PAGE_MUSIC == mPage ) {
        manager = mChannelManagerMusic;
    } else if( PAGE_VIDEO == mPage ) {
        manager = mChannelManagerVideo;
    }
    return manager;
}

MediaManager *MainWindow::currentPageMediaManager()
{
    MediaManager *manager = nullptr;
    if( PAGE_MUSIC == mPage ) {
        manager = &mChannelManagerMusic->currentChannel().mediaManager();
    } else if( PAGE_ADVERT == mPage ) {
        manager = mMediaAdvertManager.data();
    } else if(PAGE_VIDEO == mPage ) {
        manager = &mChannelManagerVideo->currentChannel().mediaManager();
    }
    return manager;
}

QTreeView * MainWindow::currentPageMediaManagerTree()
{
    QTreeView *treeView = nullptr;
    if( PAGE_MUSIC == mPage ) {
        treeView = ui->tree_music;
    } else if( PAGE_ADVERT == mPage ) {
        treeView = ui->tree_advert;
    } else if(PAGE_VIDEO == mPage ) {
        treeView = ui->tree_video;
    }

    return treeView;
}

QTableView * MainWindow::currentTableView()
{
    QTableView *tableView = nullptr;
    if( PAGE_MUSIC == mPage ) {
        tableView = ui->tableCnannelMusic;
    } else if( PAGE_ADVERT == mPage ) {
        tableView = ui->tableAdvert;
    } else if(PAGE_VIDEO == mPage ) {
        tableView = ui->tableChannelVideo;
    }
    return tableView;
}

///
/// Информационный заголовок
///

void MainWindow::setSelectFileInfoHeader(QModelIndex index, QModelIndex)
{

    if(!index.isValid()) {
        clearFileInfoHeader();
        return;
    }

    QTreeView *treeView = currentPageMediaManagerTree();
    if(!treeView)
        return;
    MediaManager *manager = currentPageMediaManager();
    if( !manager )
        return;

    QString lb_trackName    = "Название: <b>%1</b>";
    QString lb_trackGenre   = "Жанр: <b>%1</b>";
    QString lb_trackAlbum   = "Альбом: <b>%1</b>";
    QString lb_trackYear    = "Год: <b>%1</b>";
    QString lb_trackLength  = "Длительность: <b>%1</b>";
    QString lb_trackAutor   = "Исполнитель: <b>%1</b>";

    const MediaData &data = manager->mediaData( index.row() );
    ui->lb_trackName->      setText( lb_trackName.arg( data.title() ) );
    ui->lb_trackGenre->     setText( lb_trackGenre.arg( data.genre() ) );
    ui->lb_trackAlbum->     setText( lb_trackAlbum.arg( data.album() ) );
    ui->lb_trackYear->      setText( lb_trackYear.arg( data.year() ) );
    ui->lb_trackLength->    setText( lb_trackLength.arg( manager->calculateLength( data.length() ) ) );
    ui->lb_trackAutor->     setText( lb_trackAutor.arg( data.artist() ) );

    mFileInfoHeaderIsClear = false;
}



/**
 * @brief MainWindow::setInfoHeader         - Формирование информационного заголовка
 */
void MainWindow::setInfoHeader()
{
    QString totalFiles = "Всего файлов: <b>%1</b>";
    QString totalTime = "Общее время: <b>%1</b>";
    QString lb_about;
    MediaManager *mediaManager = nullptr;

    /// Музыка
    if( PAGE_MUSIC == mPage || PAGE_VIDEO == mPage ) {
        ChannelManager *manager = currentPageChannelManager();
        if( !manager )
            return;

        if( !manager->channelCount() ) {
            clearInfoHeader();
            if( PAGE_MUSIC == mPage )
                lb_about = "Музыка";
            else if( PAGE_VIDEO == mPage )
                lb_about = "Видео";

            ui->lb_nameAbout->setText(lb_about);
            return;
        }
        mediaManager = &manager->currentChannel().mediaManager();
        lb_about = manager->currentChannel().channelName();
    } else if( PAGE_ADVERT == mPage )
    /// Реклама
    {
        if(!mMediaAdvertManager->mediaCount()) {
            clearInfoHeader();
            return;
        }
        mediaManager = mMediaAdvertManager.data();
        lb_about = "Реклама";
    } else {
        clearInfoHeader();
    }

    if(!mediaManager)
        return;

    ui->lb_count->setText( totalFiles.arg( mediaManager->mediaCount() ) );
    ui->lb_totalTime->setText( totalTime.arg( mediaManager->totalLength() ) );
    ui->lb_nameAbout->setText(lb_about);
}

void MainWindow::clearInfoHeader()
{
    ui->lb_nameAbout->setText("");
    ui->lb_count->setText("Всего файлов:");
    ui->lb_totalTime->setText("Общее время:");
    ui->lb_trackAutor->setText("Исполнитель:");
    ui->lb_trackLength->setText("Длительность:");
    ui->lb_trackAlbum->setText("Альбом:");
    ui->lb_trackName->setText("Название:");
    ui->lb_trackGenre->setText("Жанр:");
    ui->lb_trackYear->setText("Год:");
}

void MainWindow::clearFileInfoHeader()
{
    ui->lb_trackName->setText("Название:");
    ui->lb_trackGenre->setText("Жанр:");
    ui->lb_trackAutor->setText("Исполнитель:");
    ui->lb_trackLength->setText("Длительность:");
    ui->lb_trackAlbum->setText("Альбом:");
    ui->lb_trackYear->setText("Год:");
    mFileInfoHeaderIsClear = true;
}

void MainWindow::selectionFileInfoHeaderChanged()
{
    QTreeView *treeView = currentPageMediaManagerTree();
    if(!treeView)
        return;
    QModelIndexList indexList = treeView->selectionModel()->selectedRows();
    if( !indexList.size() ) {
        clearFileInfoHeader();
        return;
    }

    if(mFileInfoHeaderIsClear)
        setSelectFileInfoHeader( treeView->currentIndex() );
}

///
/// Состояния кнопок
///

/// Кнопка удаления каналов
void MainWindow::setDeleteChannelButtonState()
{
    ChannelManager *manager = currentPageChannelManager();
    if(!manager)
        return;
    bool state;
    if( manager->channelCount() )
        state = true;
    else
        state = false;

    QPushButton *channelDeleteButton = nullptr;
    if(PAGE_MUSIC == mPage)
        channelDeleteButton = ui->pb_deleteChannel;
    else if(PAGE_VIDEO == mPage)
        channelDeleteButton = ui->pb_deleteVideoChannel;

    if(channelDeleteButton)
        channelDeleteButton->setEnabled(state);
}

/// Кнопка удаления рекламы из таблицы
void MainWindow::setDeleteButtonAdvertState()
{
    bool state;
    if( mAdvertManager->count() )
        state = true;
    else
        state = false;

    ui->pb_delAdvert->setEnabled(state);
}

/// Кнопка добавление рекламы в таблицу
void MainWindow::setAddButtonAdvertState()
{
    QModelIndexList indexList = ui->tree_advert->selectionModel()->selectedRows();
    bool state;
    if( indexList.size() )
        state = true;
    else
        state = false;

    ui->pb_addAdvert->setEnabled(state);
}

/// Кнопка удаления файлов из хранилища
void MainWindow::setRemoveFilesButtonState()
{
    bool state;

    QTreeView *treeView = currentPageMediaManagerTree();
    if(!treeView)
        state = false;
    else {
        QModelIndexList indexList = treeView->selectionModel()->selectedRows();
        if( indexList.size() )
            state = true;
        else
            state = false;
    }

    ui->pb_removeFiles->setEnabled(state);
}

/// Кнопка добавления файлов в хранилище
void MainWindow::setAddFilesButtonState()
{
    bool state;
    QTreeView *treeView = nullptr;
    if( PAGE_MUSIC == mPage || PAGE_VIDEO == mPage ) {
        treeView = currentPageMediaManagerTree();
        if( currentPageChannelManager()->channelCount() )
            state = true;
        else
            state = false;
    } else if( PAGE_ADVERT == mPage ) {
        state = true;
    } else {
        state = false;
    }

    if( treeView )
        treeView->setEnabled(state);
//    ui->pb_addFiles->setEnabled(state);
}

/// Кнопка "обновить"
void MainWindow::setRefreshButtonState()
{
    // NOTE: Пока что не требуется изменять состояние кнопки "обновить"
    return;
    bool state;
    if( mChannelManagerMusic->channelCount() || mAdvertManager->count() )
        state = true;
    else
        state = false;

    mAct_RefreshTimetable->setEnabled(state);
    ui->act_updateTimetable->setEnabled(state);
}

///
/// Окна
///

/**
 * @brief slot_settings             - Вызов окна отображения настроек
 */
void MainWindow::slot_settings()
{
    SettingsDialog *dialog = new SettingsDialog(this, Qt::Dialog);
    // NOTE: Было нужно для фильтра в QFileSystemModel
//    connect( dialog, SIGNAL( fileFormats()), SLOT( changeFileFormats() ) );
    dialog->exec();
    dialog->deleteLater();
}

/**
 * @brief about -                   - О программе
 */
void MainWindow::about()
{
    AboutLampbox * _about = new  AboutLampbox(this, Qt::Popup | Qt::Dialog );
    _about->exec();
    _about->deleteLater();
}
