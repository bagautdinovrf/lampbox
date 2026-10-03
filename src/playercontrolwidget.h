#ifndef PLAYERCONTROLWIDGET_H
#define PLAYERCONTROLWIDGET_H

#include "mediaboxplayerclient.h"

#include <QDateTime>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class RestyleLabel;

// Presents the shared client's confirmed snapshot. Opening this view never
// replaces the player's queue or changes playback.
class PlayerControlWidget : public QWidget
{
    Q_OBJECT
public:
    explicit PlayerControlWidget(MediaBoxPlayerClient *client, QWidget *parent = nullptr);

signals:
    void settingsRequested();

private:
    void refresh();
    void refreshNotice();
    void editPaths(bool append);
    void commitSeek();
    void commitVolume();

    MediaBoxPlayerClient *mClient;
    PlayerStatus mStatus;
    bool mHasStatus = false;
    QDateTime mReceivedAt;
    QStringList mDisplayedQueue;
    int mDisplayedIndex = -2;
    QString mConnectionMessage;
    QString mCommandMessage;
    QStringList mUnknownCommands;
    bool mCommandError = false;
    QString mSeekTrack;
    int mSeekIndex = -1;

    RestyleLabel *mConnectionLabel;
    RestyleLabel *mFreshnessLabel;
    RestyleLabel *mStateLabel;
    RestyleLabel *mIntentLabel;
    RestyleLabel *mPositionLabel;
    RestyleLabel *mVolumeLabel;
    RestyleLabel *mAudioErrorLabel;
    RestyleLabel *mConnectionErrorLabel;
    RestyleLabel *mQueueLabel;
    RestyleLabel *mNoticeLabel;
    QWidget *mNotice;
    QLineEdit *mTrack;
    QListWidget *mQueue;
    QSlider *mSeek;
    QSlider *mVolume;
    QCheckBox *mMute;
    QComboBox *mRepeat;
    QPushButton *mConnect;
    QPushButton *mDisconnect;
    QPushButton *mRefresh;
    QPushButton *mPlay;
    QPushButton *mPause;
    QPushButton *mStop;
    QPushButton *mPrevious;
    QPushButton *mNext;
    QPushButton *mLoad;
    QPushButton *mEnqueue;
    QPushButton *mClear;
};

#endif // PLAYERCONTROLWIDGET_H
