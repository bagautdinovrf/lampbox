#pragma once

#ifndef VIDEOCONTROLWIDGET_H
#define VIDEOCONTROLWIDGET_H

#include "mediaboxvplayerclient.h"

#include <QHash>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class RestyleLabel;

// Desired profiles are manager data. Only explicit operator actions send commands;
// the separately displayed confirmed state always comes from MediaBoxVPlayer.
class VideoControlWidget final : public QWidget
{
    Q_OBJECT
public:
    explicit VideoControlWidget(QWidget *parent = nullptr, MediaBoxVPlayerClient *client = nullptr);
    void reloadConnection();
    void addPlaylistPaths(const QStringList &paths);
    void setSchedulePublication(const QString &activePath, const QString &contentRoot);
    void setSelectedChannel(const QString &name, const QStringList &paths, int volume,
                            const QString &order = QStringLiteral("shuffle_cycle"));
    bool playSelectedChannel();
    bool startSelectedSchedule();
signals:
    void settingsRequested();
private:
    struct Playlist { QString id; QString name; QStringList paths; };
    struct WindowProfile {
        QString id;
        QString name;
        QString screen;
        bool fullscreen = true;
        QList<Playlist> playlists;
        QString selectedPlaylist;
    };
    struct Pending { QString description; QString removeWindow; };

    WindowProfile *selectedWindow();
    Playlist *selectedPlaylist();
    const VideoWindowStatus *confirmedWindow() const;
    void restoreProfiles();
    void saveProfiles();
    void refreshWindows(const QString &select = {});
    void refreshEditor();
    void refreshPlaylists();
    void refreshPaths();
    void refreshDisplays();
    void refreshStatus();
    void refreshPlayback();
    void commitSeek();
    void commitVolume();
    void beginSeek();
    void beginVolume();
    void updateActions();
    void receiveStatus(const VideoPlayerStatus &status);
    void addWindow();
    void removeWindow();
    void editWindow();
    void applyWindow();
    void addPlaylist();
    void renamePlaylist();
    void removePlaylist();
    void addPaths(const QStringList &paths);
    void addRemotePaths();
    void removePaths();
    void movePath(int direction);
    bool playbackTargetAvailable() const;
    void submit(const QString &id, const QString &description, const QString &removeWindow = {});
    void message(const QString &text, const QString &role = QStringLiteral("muted"));

    MediaBoxVPlayerClient *mClient = nullptr;
    QList<WindowProfile> mProfiles;
    QHash<QString, Pending> mPending;
    QString mConnectionErrorMessage;
    QString mScheduleActivePath;
    QString mScheduleContentRoot;
    QString mSelectedChannelName;
    QStringList mSelectedChannelPaths;
    int mSelectedChannelVolume = 100;
    bool mUpdating = false;
    QListWidget *mWindows = nullptr;
    QLineEdit *mName = nullptr;
    QComboBox *mDisplays = nullptr;
    QCheckBox *mFullscreen = nullptr;
    QComboBox *mPlaylists = nullptr;
    QListWidget *mPaths = nullptr;
    RestyleLabel *mConnection = nullptr;
    RestyleLabel *mConfirmed = nullptr;
    RestyleLabel *mMessage = nullptr;
    RestyleLabel *mProfileError = nullptr;
    QWidget *mEditor = nullptr;
    QPushButton *mAddWindow = nullptr;
    QPushButton *mRemoveWindow = nullptr;
    QPushButton *mApply = nullptr;
    QPushButton *mLoad = nullptr;
    QPushButton *mSchedule = nullptr;
    QPushButton *mPlayChannel = nullptr;
    RestyleLabel *mSelectedChannel = nullptr;
    QString mSelectedChannelOrder = QStringLiteral("shuffle_cycle");
    QPushButton *mToggleFullscreen = nullptr;
    QSlider *mSeek = nullptr, *mVolume = nullptr;
    QCheckBox *mMuted = nullptr;
    QComboBox *mRepeat = nullptr;
    QListWidget *mQueue = nullptr;
    RestyleLabel *mPosition = nullptr, *mVolumeLabel = nullptr, *mQueueLabel = nullptr;
    QPushButton *mEnqueue = nullptr, *mClearQueue = nullptr;
    QString mSeekWindow, mSeekTrack, mVolumeWindow;
    int mSeekIndex = -1;
    QStringList mDisplayedQueue;
    QString mDisplayedWindow;
    int mDisplayedIndex = -1;
    QList<QPushButton *> mTransportButtons;
    QList<QPushButton *> mPlaylistButtons;
};

#endif
