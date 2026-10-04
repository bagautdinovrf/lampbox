#pragma once
#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QModelIndex>
#include <QJsonObject>
#include <array>
#include <memory>

class ChannelManager;
class ChannelModel;
class AdvertManager;
class AdvertModel;
class MediaManager;
class MediaModel;
class MediaController;
class MediaImportService;
class VideoController;
class QStackedWidget;
class QComboBox;
class QPushButton;
class QLabel;
class QTreeView;
class QTableView;
class QSortFilterProxyModel;
class QLineEdit;
class QTimer;
class SchedulePreviewWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
  public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    enum MainWindowPage {
        PAGE_MUSIC,
        PAGE_VIDEO,
        PAGE_ADVERT
    };
  public slots:
    void changePage(int page);
    void openChannelEditor();
    void openAdvertEditor();
    void slotChangeChannel(const QModelIndex &index, const QModelIndex &previous = {});

  protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
  private slots:
    void slot_addMediaFiles();
    void slot_removeMediaFiles();
    void slot_addChannel();
    void slot_deleteChannel();
    void slot_addAdvert();
    void slot_deleteAdvert();
    void copyFiles(const QStringList &files);
    void slot_playTrack(QModelIndex index);
    void updatePlayerState();

  private:
    friend void seedRestyleWindow(MainWindow &window);
    friend class PlaybackUiTests;
    struct MediaPage {
        QWidget *root = nullptr;
        QTableView *channels = nullptr;
        QTableView *adverts = nullptr;
        QTreeView *files = nullptr;
        QSortFilterProxyModel *proxy = nullptr;
        MediaModel *source = nullptr;
        SchedulePreviewWidget *schedule = nullptr;
        QLineEdit *search = nullptr;
        QLabel *subtitle = nullptr;
        QLabel *channelTitle = nullptr;
        QLabel *channelDetail = nullptr;
        QLabel *channelConditions = nullptr;
        QLabel *fileTitle = nullptr;
        QLabel *fileArtist = nullptr;
        std::array<QLabel *, 4> fileFields{};
        QLabel *fileCount = nullptr;
        QLabel *libraryEmpty = nullptr;
        QLabel *planNow = nullptr;
        QLabel *planNext = nullptr;
        QLabel *planAd = nullptr;
        QPushButton *addFiles = nullptr;
        QPushButton *deleteFiles = nullptr;
        QPushButton *editChannel = nullptr;
        QPushButton *deleteChannel = nullptr;
        QPushButton *addAdvert = nullptr;
        QPushButton *editAdvert = nullptr;
        QPushButton *deleteAdvert = nullptr;
        QPushButton *preview = nullptr;
        QPushButton *fileInfo = nullptr;
        QPushButton *playChannel = nullptr;
        QPushButton *schedulePlayback = nullptr;
        QPushButton *openFolder = nullptr;
    };
    QWidget *buildMediaPage(int page);
    void buildShell();
    void adaptLayout(int width);
    void updatePage(int page);
    void updateFileInfo(int page);
    void updateSummary(int page);
    void showFileInfo();
    void showStationInfo();
    void showPlayerControls();
    void showVideoControls();
    void addSelectedVideosToPlaylist();
    void playSelectedChannel(int page);
    void startScheduledPlayback(int page);
    void editScheduleProject(int page = PAGE_MUSIC);
    bool publishMusicSchedule(bool autoplay, QString *error);
    void updateScheduleDocumentPreview();
    QJsonObject playbackSchedule(int page, QString *error) const;
    void refreshPlaybackSchedules();
    void updatePlaybackActions();
    void updatePlayingMedia();
    bool updateVideoPlaybackContext(QString *error = nullptr);
    void showAllSchedules();
    void showError(const QString &message);
    void selectChannel(int page, int row);
    MediaManager *mediaManager(int page) const;
    bool advertWritable() const;
    bool playerAvailable() const;
    std::array<MediaPage, 3> mPages;
    ChannelManager *mChannelManagers[2] = {nullptr, nullptr};
    ChannelModel *mChannelModels[2] = {nullptr, nullptr};
    std::unique_ptr<MediaManager> mMediaAdvertManager;
    std::unique_ptr<AdvertManager> mAdvertManager;
    AdvertModel *mAdvertModel = nullptr;
    MediaController *mMediaController = nullptr;
    VideoController *mVideoController = nullptr;
    MediaImportService *mMediaImport = nullptr;
    QString mImportTargetDirectory;
    bool mCloseAfterImport = false;
    QStackedWidget *mStack = nullptr;
    QComboBox *mAppearance = nullptr;
    QComboBox *mTheme = nullptr;
    std::array<QPushButton *, 6> mNavigation{};
    QPushButton *mPlay = nullptr;
    QPushButton *mStop = nullptr;
    QLabel *mPlayerState = nullptr;
    QLabel *mPlayerDetail = nullptr;
    QLabel *mOperationState = nullptr;
    QString mUnknownPlayerCommand;
    QString mAudioConnectionMessage;
    QString mVideoConnectionMessage;
    int mPage = PAGE_MUSIC;
    QTimer *mScheduleUpdateTimer = nullptr;
    std::array<QJsonObject, 2> mScheduleDocuments;
    QString mVideoScheduleActivePath;
    QString mVideoScheduleContentRoot;
};
#endif
