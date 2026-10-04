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
    void submit(const QString &id, const QString &description, const QString &removeWindow = {});
    void message(const QString &text, const QString &role = QStringLiteral("muted"));

    MediaBoxVPlayerClient *mClient = nullptr;
    QList<WindowProfile> mProfiles;
    QHash<QString, Pending> mPending;
    QString mConnectionErrorMessage;
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
    QPushButton *mToggleFullscreen = nullptr;
    QList<QPushButton *> mTransportButtons;
    QList<QPushButton *> mPlaylistButtons;
};

#endif
