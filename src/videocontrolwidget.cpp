#include "videocontrolwidget.h"

#include "restylewidgets.h"
#include "settings.h"
#include "videocontroller.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFileDialog>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QSlider>
#include <QTimer>
#include <QFileInfo>
#include <QUuid>
#include <QVBoxLayout>

namespace {
constexpr int seekSteps = 10000;
constexpr qint64 maximumSeekPosition = 9007199254740991LL;
QString timeText(qint64 milliseconds)
{
    const qint64 seconds = qMax(qint64(0), milliseconds) / 1000;
    return QStringLiteral("%1:%2:%3").arg(seconds / 3600, 2, 10, QLatin1Char('0'))
            .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0')).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}
QString newId() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

QPushButton *button(const QString &text, const char *name, QWidget *parent, bool primary = false)
{
    auto *result = new QPushButton(text, parent);
    result->setObjectName(QString::fromLatin1(name));
    result->setAutoDefault(false);
    result->setMinimumHeight(32);
    Restyle::button(result, primary ? QStringLiteral("primary") : QStringLiteral("normal"));
    return result;
}

RestyleLabel *label(const QString &text, QWidget *parent, const char *name = nullptr)
{
    auto *result = new RestyleLabel(text, 12, QFont::Normal, parent);
    result->setTextFormat(Qt::PlainText);
    result->setWordWrap(true);
    if (name) result->setObjectName(QString::fromLatin1(name));
    return result;
}

bool absoluteMediaPath(const QString &path)
{
    // Paths belong to the player machine, whose OS may differ from the manager.
    static const QRegularExpression drive(QStringLiteral("\\A[A-Za-z]:[/\\\\]"));
    return path.size() <= 4096 && !path.contains(QChar::Null) && !path.contains('\n') && !path.contains('\r')
            && (path.startsWith('/') || path.startsWith(QStringLiteral("\\\\"))
                || drive.match(path).hasMatch());
}
}

VideoControlWidget::VideoControlWidget(QWidget *parent, MediaBoxVPlayerClient *client) : QWidget(parent)
{
    setObjectName(QStringLiteral("videoControlWidget"));
    setFont(Restyle::font());
    setMinimumSize(760, 540);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(18, 16, 18, 16);
    outer->setSpacing(10);
    auto *heading = new QHBoxLayout;
    heading->addWidget(new RestyleLabel(tr("Видеоэкраны"), 24, QFont::DemiBold, this));
    heading->addStretch();
    auto *settings = button(tr("Подключение…"), "videoConnectionSettings", this);
    heading->addWidget(settings);
    outer->addLayout(heading);
    mConnection = label({}, this, "videoConnectionStatus");
    mConnection->setColorRole(QStringLiteral("muted"));
    outer->addWidget(mConnection);
    auto *splitter = new QSplitter(this);
    auto *sidebar = new RestylePanel(splitter);
    auto *side = new QVBoxLayout(sidebar);
    side->setContentsMargins(12, 12, 12, 12);
    side->addWidget(new RestyleLabel(tr("Экраны"), 16, QFont::DemiBold, sidebar));
    mWindows = new QListWidget(sidebar);
    mWindows->setObjectName(QStringLiteral("videoWindows"));
    mWindows->setAccessibleName(tr("Логические видеоэкраны"));
    side->addWidget(mWindows, 1);
    mAddWindow = button(tr("Создать экран"), "videoAddWindow", sidebar);
    mRemoveWindow = button(tr("Удалить экран"), "videoRemoveWindow", sidebar);
    side->addWidget(mAddWindow);
    side->addWidget(mRemoveWindow);
    sidebar->setMinimumWidth(185);

    mEditor = new QWidget(splitter);
    auto *rightLayout = new QVBoxLayout(mEditor);
    rightLayout->setContentsMargins(6, 0, 0, 0);
    rightLayout->setSpacing(10);
    auto *scroll = new QScrollArea(mEditor);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *scrollBody = new QWidget(scroll);
    auto *editor = new QVBoxLayout(scrollBody);
    editor->setContentsMargins(0, 0, 0, 0);
    editor->setSpacing(10);
    auto *window = new RestylePanel(scrollBody);
    auto *windowLayout = new QGridLayout(window);
    windowLayout->setContentsMargins(14, 12, 14, 12);
    windowLayout->setHorizontalSpacing(10);
    windowLayout->setVerticalSpacing(8);
    mName = new QLineEdit(window);
    mName->setObjectName(QStringLiteral("videoWindowName"));
    mName->setAccessibleName(tr("Название видеоэкрана"));
    mName->setMaxLength(128);
    mName->setMinimumHeight(32);
    mDisplays = new QComboBox(window);
    mDisplays->setObjectName(QStringLiteral("videoDisplay"));
    mDisplays->setAccessibleName(tr("Монитор на машине видеоплеера"));
    mDisplays->setMinimumHeight(32);
    mDisplays->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    mDisplays->setMinimumContentsLength(18);
    mFullscreen = new QCheckBox(tr("Запускать на весь экран"), window);
    mFullscreen->setObjectName(QStringLiteral("videoFullscreen"));
    mApply = button(tr("Применить окно"), "videoApplyWindow", window, true);
    windowLayout->addWidget(label(tr("Название"), window), 0, 0);
    windowLayout->addWidget(label(tr("Монитор видеоплеера"), window), 0, 1);
    windowLayout->addWidget(mName, 1, 0);
    windowLayout->addWidget(mDisplays, 1, 1);
    windowLayout->addWidget(mFullscreen, 2, 0);
    windowLayout->addWidget(mApply, 2, 1);
    windowLayout->setColumnStretch(0, 1);
    windowLayout->setColumnStretch(1, 1);
    windowLayout->addWidget(label(tr("Настройки и плейлисты сохраняются в менеджере. «Применить окно» создаёт или обновляет окно плеера."), window), 3, 0, 1, 2);
    editor->addWidget(window);

    auto *playlists = new RestylePanel(scrollBody);
    auto *playlistLayout = new QVBoxLayout(playlists);
    playlistLayout->setContentsMargins(14, 12, 14, 12);
    playlistLayout->setSpacing(8);
    playlistLayout->addWidget(new RestyleLabel(tr("Плейлисты этого экрана"), 16, QFont::DemiBold, playlists));
    auto *playlistRow = new QHBoxLayout;
    mPlaylists = new QComboBox(playlists);
    mPlaylists->setObjectName(QStringLiteral("videoPlaylists"));
    mPlaylists->setAccessibleName(tr("Плейлист выбранного видеоэкрана"));
    mPlaylists->setMinimumHeight(32);
    mPlaylists->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    playlistRow->addWidget(mPlaylists, 1);
    auto *addPlaylistButton = button(tr("Создать"), "videoAddPlaylist", playlists);
    auto *renamePlaylistButton = button(tr("Название…"), "videoRenamePlaylist", playlists);
    auto *removePlaylistButton = button(tr("Удалить"), "videoRemovePlaylist", playlists);
    playlistRow->addWidget(addPlaylistButton);
    playlistRow->addWidget(renamePlaylistButton);
    playlistRow->addWidget(removePlaylistButton);
    playlistLayout->addLayout(playlistRow);
    mPaths = new QListWidget(playlists);
    mPaths->setObjectName(QStringLiteral("videoPlaylistPaths"));
    mPaths->setAccessibleName(tr("Файлы плейлиста в порядке воспроизведения"));
    mPaths->setSelectionMode(QAbstractItemView::ExtendedSelection);
    mPaths->setMinimumHeight(80);
    mPaths->setMaximumHeight(100);
    playlistLayout->addWidget(mPaths, 1);
    auto *pathRow = new QHBoxLayout;
    auto *local = button(tr("Файлы…"), "videoAddFiles", playlists);
    auto *remote = button(tr("Пути…"), "videoAddPaths", playlists);
    auto *remove = button(tr("Убрать"), "videoRemovePaths", playlists);
    auto *up = button(tr("Выше"), "videoPathUp", playlists);
    auto *down = button(tr("Ниже"), "videoPathDown", playlists);
    for (auto *action : {local, remote, remove, up, down}) pathRow->addWidget(action);
    mPlaylistButtons = {renamePlaylistButton, removePlaylistButton, local, remote, remove, up, down};
    playlistLayout->addLayout(pathRow);
    playlistLayout->addWidget(label(tr("Указывайте абсолютные пути на машине MediaBoxVPlayer; файлы по TCP не передаются."), playlists));
    mLoad = button(tr("Загрузить плейлист в окно"), "videoLoadPlaylist", playlists, true);
    playlistLayout->addWidget(mLoad, 0, Qt::AlignLeft);
    editor->addWidget(playlists, 1);
    scroll->setWidget(scrollBody);
    rightLayout->addWidget(scroll, 1);

    auto *playback = new RestylePanel(mEditor);
    auto *playbackLayout = new QVBoxLayout(playback);
    playbackLayout->setContentsMargins(14, 12, 14, 12);
    mConfirmed = label({}, playback, "videoConfirmedStatus");
    mConfirmed->setMinimumHeight(45);
    playbackLayout->addWidget(mConfirmed);
    auto *seekRow = new QHBoxLayout;
    mPosition = label(tr("Позиция: —"), playback, "videoPosition");
    mPosition->setWordWrap(false);
    seekRow->addWidget(mPosition);
    mSeek = new QSlider(Qt::Horizontal, playback);
    mSeek->setObjectName(QStringLiteral("videoSeek"));
    mSeek->setAccessibleName(tr("Позиция видео выбранного окна"));
    mSeek->setRange(0, seekSteps); mSeek->setPageStep(seekSteps / 20);
    seekRow->addWidget(mSeek, 1);
    playbackLayout->addLayout(seekRow);
    auto *soundRow = new QHBoxLayout;
    mVolumeLabel = label(tr("Громкость: —"), playback, "videoVolumeLabel");
    mVolumeLabel->setWordWrap(false);
    soundRow->addWidget(mVolumeLabel);
    mVolume = new QSlider(Qt::Horizontal, playback);
    mVolume->setObjectName(QStringLiteral("videoVolume"));
    mVolume->setAccessibleName(tr("Громкость выбранного видеоокна"));
    mVolume->setRange(0, 100); mVolume->setMaximumWidth(170);
    soundRow->addWidget(mVolume, 1);
    mMuted = new QCheckBox(tr("Без звука"), playback);
    mMuted->setObjectName(QStringLiteral("videoMuted"));
    soundRow->addWidget(mMuted);
    mRepeat = new QComboBox(playback);
    mRepeat->setObjectName(QStringLiteral("videoRepeat"));
    mRepeat->setAccessibleName(tr("Повтор видео выбранного окна"));
    mRepeat->addItem(tr("Без повтора"), QStringLiteral("off"));
    mRepeat->addItem(tr("Повтор очереди"), QStringLiteral("all"));
    mRepeat->addItem(tr("Повтор файла"), QStringLiteral("one"));
    soundRow->addWidget(mRepeat);
    playbackLayout->addLayout(soundRow);
    mSelectedChannel = label({}, playback, "videoSelectedChannel");
    mSelectedChannel->setColorRole(QStringLiteral("muted"));
    playbackLayout->addWidget(mSelectedChannel);
    auto *modeActions = new QHBoxLayout;
    mSchedule = button(tr("По расписанию"), "videoSchedule", playback, true);
    mPlayChannel = button(tr("Играть канал"), "videoPlayChannel", playback);
    mSchedule->setToolTip(tr("Запустить расписание видеоканалов в выбранном видеоэкране."));
    mPlayChannel->setToolTip(tr("Воспроизводить выбранный видеоканал с повтором всех файлов."));
    modeActions->addWidget(mSchedule);
    modeActions->addWidget(mPlayChannel);
    modeActions->addStretch();
    playbackLayout->addLayout(modeActions);
    auto *transport = new QHBoxLayout;
    auto *previous = button(tr("Пред."), "videoPrevious", playback);
    auto *play = button(tr("Пуск"), "videoPlay", playback, true);
    auto *pause = button(tr("Пауза"), "videoPause", playback);
    auto *stop = button(tr("Стоп"), "videoStop", playback);
    auto *next = button(tr("След."), "videoNext", playback);
    mTransportButtons = {previous, play, pause, stop, next};
    for (auto *action : mTransportButtons) transport->addWidget(action);
    playbackLayout->addLayout(transport);
    mToggleFullscreen = button(tr("Полный экран"), "videoToggleFullscreen", playback);
    mToggleFullscreen->setToolTip(tr("Переключить полный экран выбранного видеоокна"));
    modeActions->insertWidget(2, mToggleFullscreen);
    auto *queueActions = new QHBoxLayout;
    mQueueLabel = label({}, playback, "videoQueueLabel");
    queueActions->addWidget(mQueueLabel, 1);
    mEnqueue = button(tr("Плейлист в очередь"), "videoEnqueue", playback);
    mEnqueue->setToolTip(tr("Добавить все файлы выбранного плейлиста в конец фактической очереди окна."));
    mClearQueue = button(tr("Очистить очередь"), "videoClearQueue", playback);
    queueActions->addWidget(mEnqueue); queueActions->addWidget(mClearQueue);
    playbackLayout->addLayout(queueActions);
    mQueue = new QListWidget(playback);
    mQueue->setObjectName(QStringLiteral("videoConfirmedQueue"));
    mQueue->setAccessibleName(tr("Фактическая очередь выбранного видеоокна"));
    mQueue->setSelectionMode(QAbstractItemView::NoSelection);
    mQueue->setMinimumHeight(65); mQueue->setMaximumHeight(95);
    playbackLayout->addWidget(mQueue);
    rightLayout->addWidget(playback);
    splitter->addWidget(sidebar);
    splitter->addWidget(mEditor);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({210, 810});
    outer->addWidget(splitter, 1);
    mMessage = label({}, this, "videoCommandMessage");
    mMessage->setMinimumHeight(32);
    outer->addWidget(mMessage);
    mProfileError = label({}, this, "videoProfileSaveError");
    mProfileError->setColorRole(QStringLiteral("error"));
    mProfileError->hide();
    outer->addWidget(mProfileError);

    mClient = client ? client : new VideoController(this);
    if (!client)
        mClient->setObjectName(QStringLiteral("videoPlayerClient"));
    connect(settings, &QPushButton::clicked, this, &VideoControlWidget::settingsRequested);
    connect(mAddWindow, &QPushButton::clicked, this, &VideoControlWidget::addWindow);
    connect(mRemoveWindow, &QPushButton::clicked, this, &VideoControlWidget::removeWindow);
    connect(mWindows, &QListWidget::currentRowChanged, this, [this] { refreshEditor(); });
    connect(mName, &QLineEdit::editingFinished, this, &VideoControlWidget::editWindow);
    connect(mDisplays, &QComboBox::activated, this, [this] { editWindow(); });
    connect(mFullscreen, &QCheckBox::clicked, this, [this] { editWindow(); });
    connect(mApply, &QPushButton::clicked, this, &VideoControlWidget::applyWindow);
    connect(addPlaylistButton, &QPushButton::clicked, this, &VideoControlWidget::addPlaylist);
    connect(renamePlaylistButton, &QPushButton::clicked, this, &VideoControlWidget::renamePlaylist);
    connect(removePlaylistButton, &QPushButton::clicked, this, &VideoControlWidget::removePlaylist);
    connect(mPlaylists, &QComboBox::currentIndexChanged, this, [this] {
        if (mUpdating) return;
        if (auto *profile = selectedWindow()) {
            profile->selectedPlaylist = mPlaylists->currentData().toString();
            saveProfiles();
        }
        refreshPaths();
    });
    connect(local, &QPushButton::clicked, this, [this] {
        addPaths(QFileDialog::getOpenFileNames(this, tr("Видео — путь должен быть доступен плееру"), {},
                                             tr("Видео (*.mp4 *.mkv *.avi *.mov *.webm *.wmv);;Все файлы (*)")));
    });
    connect(remote, &QPushButton::clicked, this, &VideoControlWidget::addRemotePaths);
    connect(remove, &QPushButton::clicked, this, &VideoControlWidget::removePaths);
    connect(up, &QPushButton::clicked, this, [this] { movePath(-1); });
    connect(down, &QPushButton::clicked, this, [this] { movePath(1); });
    connect(mLoad, &QPushButton::clicked, this, [this] {
        const auto *profile = selectedWindow();
        const auto *playlist = selectedPlaylist();
        if (profile && playlist)
            submit(mClient->load(profile->id, playlist->paths, 0, false), tr("Загрузка плейлиста"));
    });
    connect(mSeek, &QSlider::sliderPressed, this, &VideoControlWidget::beginSeek);
    connect(mSeek, &QSlider::sliderReleased, this, &VideoControlWidget::commitSeek);
    connect(mVolume, &QSlider::sliderPressed, this, &VideoControlWidget::beginVolume);
    connect(mVolume, &QSlider::sliderReleased, this, &VideoControlWidget::commitVolume);
    connect(mSeek, &QSlider::actionTriggered, this, [this] {
        if (!mSeek->isSliderDown()) { beginSeek(); QTimer::singleShot(0, this, &VideoControlWidget::commitSeek); }
    });
    connect(mVolume, &QSlider::actionTriggered, this, [this] {
        if (!mVolume->isSliderDown()) { beginVolume(); QTimer::singleShot(0, this, &VideoControlWidget::commitVolume); }
    });
    connect(mMuted, &QCheckBox::clicked, this, [this](bool muted) {
        if (const auto *window = confirmedWindow(); window && mPending.isEmpty())
            submit(mClient->setMuted(window->id, muted), tr("Звук видеоокна"));
        refreshPlayback();
    });
    connect(mRepeat, &QComboBox::activated, this, [this](int index) {
        if (const auto *window = confirmedWindow(); window && mPending.isEmpty())
            submit(mClient->setRepeat(window->id, mRepeat->itemData(index).toString()), tr("Повтор видео"));
        refreshPlayback();
    });
    connect(mEnqueue, &QPushButton::clicked, this, [this] {
        const auto *window = confirmedWindow();
        const auto *playlist = selectedPlaylist();
        if (window && playlist && !playlist->paths.isEmpty() && mPending.isEmpty())
            submit(mClient->enqueue(window->id, playlist->paths), tr("Добавление плейлиста в очередь"));
    });
    connect(mClearQueue, &QPushButton::clicked, this, [this] {
        if (const auto *window = confirmedWindow(); window && mPending.isEmpty())
            submit(mClient->clear(window->id), tr("Очистка очереди"));
    });
    connect(mSchedule, &QPushButton::clicked, this, [this] { startSelectedSchedule(); });
    connect(mPlayChannel, &QPushButton::clicked, this, [this] { playSelectedChannel(); });
    connect(play, &QPushButton::clicked, this, [this] {
        if (auto *profile = selectedWindow()) submit(mClient->play(profile->id), tr("Пуск"));
    });
    connect(pause, &QPushButton::clicked, this, [this] {
        if (auto *profile = selectedWindow()) submit(mClient->pause(profile->id), tr("Пауза"));
    });
    connect(stop, &QPushButton::clicked, this, [this] {
        if (auto *profile = selectedWindow()) submit(mClient->stop(profile->id), tr("Стоп"));
    });
    connect(previous, &QPushButton::clicked, this, [this] {
        if (auto *profile = selectedWindow()) submit(mClient->previous(profile->id), tr("Предыдущее видео"));
    });
    connect(next, &QPushButton::clicked, this, [this] {
        if (auto *profile = selectedWindow()) submit(mClient->next(profile->id), tr("Следующее видео"));
    });
    connect(mToggleFullscreen, &QPushButton::clicked, this, [this] {
        if (const auto *confirmed = confirmedWindow())
            submit(mClient->setFullscreen(confirmed->id, !confirmed->fullscreen), tr("Полный экран"));
    });
    connect(mClient, &MediaBoxVPlayerClient::videoStatusChanged, this, &VideoControlWidget::receiveStatus);
    connect(mClient, &MediaBoxPlayerClient::connectionStateChanged, this, [this] {
        if (mClient->isReady()) {
            if (!mConnectionErrorMessage.isEmpty() && mMessage->text() == mConnectionErrorMessage)
                message(tr("Подключение к MediaBoxVPlayer установлено."), QStringLiteral("success"));
            mConnectionErrorMessage.clear();
        }
        refreshDisplays();
        refreshStatus();
        updateActions();
    });
    connect(mClient, &MediaBoxPlayerClient::connectionError, this, [this](const QString &text) {
        mConnectionErrorMessage = text;
        message(text, QStringLiteral("error"));
    });
    connect(mClient, &MediaBoxPlayerClient::commandSucceeded, this, [this](const QString &id) {
        if (!mPending.contains(id)) return;
        const auto pending = mPending.take(id);
        if (!pending.removeWindow.isEmpty()) {
            for (int i = 0; i < mProfiles.size(); ++i) {
                if (mProfiles.at(i).id == pending.removeWindow) {
                    mProfiles.removeAt(i);
                    break;
                }
            }
            saveProfiles();
            refreshWindows();
        }
        message(tr("%1: подтверждено плеером.").arg(pending.description), QStringLiteral("success"));
        updateActions();
    });
    connect(mClient, &MediaBoxPlayerClient::commandFailed, this,
            [this](const QString &id, const QString &, const QString &, const QString &text) {
        mPending.remove(id);
        message(text, QStringLiteral("error"));
        updateActions();
    });
    connect(mClient, &MediaBoxPlayerClient::commandOutcomeUnknown, this, [this](const QString &id) {
        mPending.remove(id);
        message(tr("Связь прервана: результат команды неизвестен. После подключения проверьте состояние окна."), QStringLiteral("error"));
        updateActions();
    });
    connect(mClient, &MediaBoxPlayerClient::commandCancelled, this, [this](const QString &id) {
        if (mPending.remove(id)) message(tr("Команда отменена при изменении подключения."));
        updateActions();
    });
    setSelectedChannel({}, {}, 100);
    restoreProfiles();
    refreshWindows();
    if (!client)
        reloadConnection();
    else if (mClient->isReady())
        receiveStatus(mClient->videoStatus());
    else {
        refreshStatus();
        updateActions();
    }
}

void VideoControlWidget::reloadConnection()
{
    mClient->disconnectFromPlayer();
    mPending.clear();
    const auto connection = Settings().videoPlayerConnection();
    if (connection.token.isEmpty()
        && (!VideoController::supportsLocalStart() || !VideoController::isLocalHost(connection.host))) {
        message(tr("Укажите адрес и токен MediaBoxVPlayer в настройках подключения."));
        refreshStatus();
        updateActions();
        return;
    }
    mClient->connectToPlayer(connection);
}

void VideoControlWidget::addPlaylistPaths(const QStringList &paths)
{
    if (!selectedPlaylist()) {
        message(tr("Сначала создайте экран и выберите его плейлист."), QStringLiteral("error"));
        return;
    }
    addPaths(paths);
}

void VideoControlWidget::setScheduleSnapshot(const QJsonObject &schedule)
{
    mScheduleSnapshot = schedule;
    updateActions();
}

void VideoControlWidget::setSelectedChannel(const QString &name, const QStringList &paths, int volume,
                                            const QString &order)
{
    mSelectedChannelName = name;
    mSelectedChannelPaths = paths;
    mSelectedChannelVolume = volume;
    mSelectedChannelOrder = order;
    mSelectedChannel->setText(name.isEmpty()
        ? tr("Выберите видеоканал в основном окне менеджера.")
        : tr("Выбран канал: %1 · файлов: %2").arg(name).arg(paths.size()));
    updateActions();
}

bool VideoControlWidget::playbackTargetAvailable() const
{
    const auto *confirmed = confirmedWindow();
    if (!confirmed || !mPending.isEmpty()) return false;
    const auto &displays = mClient->videoStatus().displays;
    if (confirmed->screen.isEmpty()) return !displays.isEmpty();
    for (const auto &display : displays)
        if (display.id == confirmed->screen) return true;
    return false;
}

bool VideoControlWidget::playSelectedChannel()
{
    if (!playbackTargetAvailable()) {
        message(tr("Выберите видеоэкран, назначьте доступный монитор и нажмите «Применить окно»."));
        return false;
    }
    if (mSelectedChannelName.isEmpty() || mSelectedChannelPaths.isEmpty()) {
        message(tr("Выберите видеоканал с файлами в основном окне менеджера."));
        return false;
    }
    const QString id = mClient->playChannel(confirmedWindow()->id, mSelectedChannelName,
                                           mSelectedChannelPaths, mSelectedChannelVolume, mSelectedChannelOrder);
    submit(id, tr("Запуск канала «%1»").arg(mSelectedChannelName));
    return !id.isEmpty();
}

bool VideoControlWidget::startSelectedSchedule()
{
    if (!playbackTargetAvailable()) {
        message(tr("Выберите видеоэкран, назначьте доступный монитор и нажмите «Применить окно»."));
        return false;
    }
    if (!mScheduleSnapshot.value(QStringLiteral("channels")).isArray()
        || !mScheduleSnapshot.value(QStringLiteral("adverts")).isArray()) {
        message(tr("Расписание видеоканалов ещё не подготовлено."));
        return false;
    }
    const QString windowId = confirmedWindow()->id;
    const QString id = mClient->startSchedule(windowId, mScheduleSnapshot);
    submit(id, tr("Запуск по расписанию"));
    return !id.isEmpty();
}

VideoControlWidget::WindowProfile *VideoControlWidget::selectedWindow()
{
    const int row = mWindows->currentRow();
    return row >= 0 && row < mProfiles.size() ? &mProfiles[row] : nullptr;
}

VideoControlWidget::Playlist *VideoControlWidget::selectedPlaylist()
{
    auto *profile = selectedWindow();
    if (!profile) return nullptr;
    for (auto &playlist : profile->playlists)
        if (playlist.id == profile->selectedPlaylist) return &playlist;
    return nullptr;
}

const VideoWindowStatus *VideoControlWidget::confirmedWindow() const
{
    const auto *item = mWindows->currentItem();
    if (!item || !mClient->isReady()) return nullptr;
    for (const auto &window : mClient->videoStatus().windows)
        if (window.id == item->data(Qt::UserRole).toString()) return &window;
    return nullptr;
}

void VideoControlWidget::restoreProfiles()
{
    for (const auto &value : Settings().videoWindowProfiles()) {
        const auto object = value.toObject();
        WindowProfile profile;
        profile.id = object.value("id").toString();
        profile.name = object.value("name").toString().trimmed().left(128);
        if (profile.id.isEmpty() || profile.name.isEmpty()) continue;
        bool duplicate = false;
        for (const auto &existing : mProfiles) duplicate |= existing.id == profile.id;
        if (duplicate) continue;
        profile.screen = object.value("screen").toString();
        profile.fullscreen = object.value("fullscreen").toBool(true);
        profile.selectedPlaylist = object.value("selectedPlaylist").toString();
        for (const auto &entry : object.value("playlists").toArray()) {
            const auto data = entry.toObject();
            Playlist playlist{data.value("id").toString(), data.value("name").toString(), {}};
            if (playlist.id.isEmpty() || playlist.name.isEmpty()) continue;
            for (const auto &path : data.value("paths").toArray()) {
                if (absoluteMediaPath(path.toString())) playlist.paths.append(path.toString());
                if (playlist.paths.size() >= 1000) break;
            }
            profile.playlists.append(playlist);
        }
        mProfiles.append(profile);
        if (mProfiles.size() >= 16) break;
    }
}

void VideoControlWidget::saveProfiles()
{
    QJsonArray result;
    for (const auto &profile : mProfiles) {
        QJsonArray playlists;
        for (const auto &playlist : profile.playlists)
            playlists.append(QJsonObject{{"id", playlist.id}, {"name", playlist.name},
                                         {"paths", QJsonArray::fromStringList(playlist.paths)}});
        result.append(QJsonObject{{"id", profile.id}, {"name", profile.name},
                                   {"screen", profile.screen}, {"fullscreen", profile.fullscreen},
                                   {"selectedPlaylist", profile.selectedPlaylist}, {"playlists", playlists}});
    }
    const bool saved = Settings().setVideoWindowProfiles(result);
    mProfileError->setVisible(!saved);
    if (!saved)
        mProfileError->setText(tr("Не удалось сохранить профили видеоэкранов. Проверьте доступ к настройкам менеджера."));
}

void VideoControlWidget::refreshWindows(const QString &select)
{
    QString id = select;
    if (id.isEmpty() && mWindows->currentItem()) id = mWindows->currentItem()->data(Qt::UserRole).toString();
    const QSignalBlocker block(mWindows);
    mWindows->clear();
    int selected = mProfiles.isEmpty() ? -1 : 0;
    for (int i = 0; i < mProfiles.size(); ++i) {
        auto *item = new QListWidgetItem(mProfiles.at(i).name, mWindows);
        item->setData(Qt::UserRole, mProfiles.at(i).id);
        if (mProfiles.at(i).id == id) selected = i;
    }
    mWindows->setCurrentRow(selected);
    refreshEditor();
}

void VideoControlWidget::refreshEditor()
{
    mUpdating = true;
    mSeekWindow.clear(); mVolumeWindow.clear();
    {
        const QSignalBlocker seekBlocker(mSeek), volumeBlocker(mVolume);
        mSeek->setSliderDown(false); mVolume->setSliderDown(false);
    }
    const auto *profile = selectedWindow();
    mEditor->setEnabled(profile);
    mName->setText(profile ? profile->name : QString());
    mFullscreen->setChecked(profile ? profile->fullscreen : true);
    refreshDisplays();
    mUpdating = false;
    refreshPlaylists();
    refreshStatus();
    updateActions();
}

void VideoControlWidget::refreshDisplays()
{
    const QSignalBlocker block(mDisplays);
    const auto *profile = selectedWindow();
    const QString screen = profile ? profile->screen : QString();
    mDisplays->clear();
    mDisplays->addItem(tr("Основной монитор"), QString());
    if (mClient->isReady()) {
        for (const auto &display : mClient->videoStatus().displays)
            mDisplays->addItem(tr("%1 · %2").arg(display.index + 1).arg(display.name), display.id);
    }
    int index = mDisplays->findData(screen);
    if (index < 0) {
        mDisplays->addItem(tr("Недоступен: %1").arg(screen), screen);
        index = mDisplays->count() - 1;
    }
    mDisplays->setCurrentIndex(index);
}

void VideoControlWidget::refreshPlaylists()
{
    const QSignalBlocker block(mPlaylists);
    mPlaylists->clear();
    auto *profile = selectedWindow();
    if (profile) {
        for (const auto &playlist : profile->playlists) mPlaylists->addItem(playlist.name, playlist.id);
        int index = mPlaylists->findData(profile->selectedPlaylist);
        if (index < 0 && mPlaylists->count()) index = 0;
        mPlaylists->setCurrentIndex(index);
        profile->selectedPlaylist = mPlaylists->currentData().toString();
    }
    refreshPaths();
}

void VideoControlWidget::refreshPaths()
{
    mPaths->clear();
    if (const auto *playlist = selectedPlaylist()) {
        mPaths->addItems(playlist->paths);
        for (int i = 0; i < mPaths->count(); ++i) mPaths->item(i)->setToolTip(mPaths->item(i)->text());
    }
    updateActions();
}

void VideoControlWidget::refreshStatus()
{
    const auto connection = Settings().videoPlayerConnection();
    QString state;
    using State = MediaBoxPlayerClient::ConnectionState;
    switch (mClient->connectionState()) {
    case State::Ready: state = tr("Подключён"); break;
    case State::Connecting: state = tr("Подключение…"); break;
    case State::Synchronizing: state = tr("Получение состояния…"); break;
    case State::Reconnecting: state = tr("Восстановление подключения…"); break;
    case State::AuthenticationFailed: state = tr("Ошибка токена доступа"); break;
    case State::ProtocolMismatch: state = tr("Несовместимый протокол"); break;
    case State::Disconnected: state = tr("Не подключён"); break;
    }
    mConnection->setText(tr("MediaBoxVPlayer · %1:%2 · %3").arg(connection.host).arg(connection.port).arg(state));
    if (mClient->isReady() && !mClient->videoStatus().persistenceError.isEmpty())
        mConnection->setText(mConnection->text() + tr("\nНе сохранено на плеере: %1")
                            .arg(mClient->videoStatus().persistenceError));
    const auto *confirmed = confirmedWindow();
    if (!mClient->isReady()) {
        mConfirmed->setText(tr("Подтверждённое состояние недоступно. После подключения воспроизведение автоматически не запускается."));
    } else if (!confirmed) {
        mConfirmed->setText(tr("Окно ещё не создано на плеере. Выберите монитор и нажмите «Применить окно»."));
    } else {
        const auto &playback = confirmed->playback;
        QString playbackState = playback.state;
        if (playbackState == "playing") playbackState = tr("воспроизведение");
        else if (playbackState == "paused") playbackState = tr("пауза");
        else if (playbackState == "stopped") playbackState = tr("остановлено");
        else if (playbackState == "loading") playbackState = tr("загрузка");
        else if (playbackState == "error") playbackState = tr("ошибка");
        QString monitor = confirmed->actualScreen.isEmpty() ? tr("недоступен") : confirmed->actualScreen;
        for (const auto &display : mClient->videoStatus().displays)
            if (display.id == confirmed->actualScreen) monitor = display.name;
        mConfirmed->setText(tr("Плеер: %1 · монитор %2 · %3\n%4")
                .arg(playbackState, monitor, confirmed->fullscreen ? tr("полный экран") : tr("в окне"),
                     playback.currentTrack.isEmpty() ? tr("Видео не выбрано") : playback.currentTrack)
                + tr("\nРежим: %1%2").arg(playback.playbackMode == QStringLiteral("schedule")
                    ? tr("по расписанию") : tr("ручной"),
                    playback.channelName.isEmpty() ? QString() : tr(" · канал «%1»").arg(playback.channelName))
                + (playback.error.isEmpty() ? QString() : tr("\nОшибка: %1").arg(playback.error))
                + (playback.scheduleError.isEmpty() ? QString() : tr("\nРасписание: %1").arg(playback.scheduleError))
                + (confirmed->restoreError.isEmpty() ? QString() : tr("\nВосстановление: %1").arg(confirmed->restoreError)));
    }
    refreshPlayback();
}

void VideoControlWidget::refreshPlayback()
{
    const auto *window = confirmedWindow();
    const PlayerStatus status = window ? window->playback : PlayerStatus{};
    mPosition->setText(window ? tr("%1 / %2").arg(timeText(status.positionMs),
                         status.durationMs > 0 ? timeText(status.durationMs) : tr("—")) : tr("Позиция: —"));
    if (!mSeek->isSliderDown()) {
        const QSignalBlocker blocker(mSeek);
        mSeek->setValue(window && status.durationMs > 0
            ? int(qBound(0.0, double(status.positionMs) / double(status.durationMs), 1.0) * seekSteps) : 0);
    }
    mSeek->setToolTip(window && status.durationMs > 0 ? tr("Перемотка после отпускания ползунка")
                          : tr("Перемотка доступна после получения длительности"));
    if (!mVolume->isSliderDown()) {
        const QSignalBlocker blocker(mVolume);
        mVolume->setValue(window ? status.volumePercent : 0);
    }
    mVolumeLabel->setText(window ? tr("Громкость: %1%").arg(status.volumePercent) : tr("Громкость: —"));
    const QSignalBlocker muteBlocker(mMuted), repeatBlocker(mRepeat);
    mMuted->setChecked(window && status.muted);
    mRepeat->setCurrentIndex(window ? mRepeat->findData(status.repeat) : -1);
    mQueueLabel->setText(window ? tr("Очередь плеера · %1").arg(status.queue.size()) : tr("Очередь · нет данных"));
    const QString windowId = window ? window->id : QString();
    if (mDisplayedWindow != windowId || mDisplayedQueue != status.queue || mDisplayedIndex != status.currentIndex) {
        mQueue->clear();
        for (int i = 0; i < status.queue.size(); ++i) {
            const QString path = status.queue.at(i);
            auto *item = new QListWidgetItem(tr("%1. %2").arg(i + 1).arg(QFileInfo(path).fileName()), mQueue);
            item->setData(Qt::UserRole, path);
            item->setToolTip(path);
            item->setFont(Restyle::font(11, i == status.currentIndex ? QFont::DemiBold : QFont::Normal));
            if (i == status.currentIndex) {
                item->setIcon(Restyle::icon(QStringLiteral("play")));
                item->setBackground(Restyle::tokens().accentSoft);
                item->setForeground(Restyle::tokens().accentText);
            }
        }
        if (status.currentIndex >= 0 && status.currentIndex < mQueue->count())
            mQueue->scrollToItem(mQueue->item(status.currentIndex));
        mDisplayedWindow = windowId; mDisplayedQueue = status.queue; mDisplayedIndex = status.currentIndex;
    }
}

void VideoControlWidget::beginSeek()
{
    const auto *window = confirmedWindow();
    mSeekWindow = window ? window->id : QString();
    mSeekTrack = window ? window->playback.currentTrack : QString();
    mSeekIndex = window ? window->playback.currentIndex : -1;
}

void VideoControlWidget::beginVolume()
{
    const auto *window = confirmedWindow();
    mVolumeWindow = window ? window->id : QString();
}

void VideoControlWidget::commitSeek()
{
    const auto *window = confirmedWindow();
    if (window && mPending.isEmpty() && window->id == mSeekWindow && window->playback.durationMs > 0
            && window->playback.currentTrack == mSeekTrack && window->playback.currentIndex == mSeekIndex) {
        const double target = double(window->playback.durationMs) * mSeek->value() / seekSteps;
        const qint64 position = qRound64(qBound(0.0, target, double(maximumSeekPosition)));
        submit(mClient->seek(window->id, position), tr("Перемотка видео"));
    }
    mSeekWindow.clear();
    refreshPlayback();
}

void VideoControlWidget::commitVolume()
{
    const auto *window = confirmedWindow();
    if (window && mPending.isEmpty() && window->id == mVolumeWindow)
        submit(mClient->setVolume(window->id, mVolume->value()), tr("Громкость видео"));
    mVolumeWindow.clear();
    refreshPlayback();
}

void VideoControlWidget::updateActions()
{
    const bool profile = selectedWindow();
    const bool playlist = selectedPlaylist();
    const bool ready = mClient->isReady();
    const bool confirmed = confirmedWindow();
    const bool idle = mPending.isEmpty();
    mAddWindow->setEnabled(mProfiles.size() < 16);
    mRemoveWindow->setEnabled(profile && idle && (ready || !mClient->hasStatus()));
    mApply->setEnabled(profile && ready && idle);
    mLoad->setEnabled(playlist && confirmed && idle && !selectedPlaylist()->paths.isEmpty());
    mToggleFullscreen->setEnabled(confirmed && idle);
    const auto *window = confirmedWindow();
    mSeek->setEnabled(confirmed && idle && !window->playback.currentTrack.isEmpty() && window->playback.durationMs > 0);
    mVolume->setEnabled(confirmed && idle); mMuted->setEnabled(confirmed && idle); mRepeat->setEnabled(confirmed && idle);
    mClearQueue->setEnabled(confirmed && idle && !window->playback.queue.isEmpty());
    mEnqueue->setEnabled(confirmed && idle && playlist && !selectedPlaylist()->paths.isEmpty()
                          && window->playback.queue.size() + selectedPlaylist()->paths.size() <= 1000);
    const bool playbackAvailable = playbackTargetAvailable();
    mSchedule->setEnabled(playbackAvailable
        && mScheduleSnapshot.value(QStringLiteral("channels")).isArray()
        && mScheduleSnapshot.value(QStringLiteral("adverts")).isArray());
    mPlayChannel->setEnabled(playbackAvailable && !mSelectedChannelName.isEmpty()
                            && !mSelectedChannelPaths.isEmpty());
    for (auto *action : mTransportButtons) action->setEnabled(confirmed && idle);
    for (auto *action : mPlaylistButtons) action->setEnabled(playlist);
}

void VideoControlWidget::receiveStatus(const VideoPlayerStatus &status)
{
    bool imported = false;
    for (const auto &window : status.windows) {
        bool known = false;
        for (const auto &profile : mProfiles) known |= profile.id == window.id;
        if (known || mProfiles.size() >= 16) continue;
        WindowProfile profile;
        profile.id = window.id;
        profile.name = window.name;
        profile.screen = window.screen;
        profile.fullscreen = window.fullscreen;
        Playlist playlist{newId(), tr("Плейлист плеера"), window.playback.queue};
        profile.selectedPlaylist = playlist.id;
        profile.playlists.append(playlist);
        mProfiles.append(profile);
        imported = true;
    }
    if (imported) {
        saveProfiles();
        const QString currentId = selectedWindow() ? selectedWindow()->id : QString();
        const QString draftName = mName->text();
        const int cursor = mName->cursorPosition();
        refreshWindows();
        if (selectedWindow() && selectedWindow()->id == currentId) {
            mName->setText(draftName);
            mName->setCursorPosition(cursor);
        }
    }
    refreshDisplays();
    refreshStatus();
    updateActions();
}

void VideoControlWidget::addWindow()
{
    if (mProfiles.size() >= 16) return;
    WindowProfile profile;
    profile.id = newId();
    profile.name = tr("Экран %1").arg(mProfiles.size() + 1);
    Playlist playlist{newId(), tr("Основной"), {}};
    profile.playlists.append(playlist);
    profile.selectedPlaylist = playlist.id;
    mProfiles.append(profile);
    saveProfiles();
    refreshWindows(profile.id);
    mName->setFocus();
    mName->selectAll();
}

void VideoControlWidget::removeWindow()
{
    const auto *profile = selectedWindow();
    if (!profile) return;
    if (confirmedWindow()) {
        submit(mClient->removeWindow(profile->id), tr("Удаление экрана"), profile->id);
        return;
    }
    // Offline deletion is a local profile operation; it cannot claim the player
    // closed a window. A still-configured remote window will be imported again.
    mProfiles.removeAt(mWindows->currentRow());
    saveProfiles();
    refreshWindows();
    message(tr("Локальный профиль удалён."));
}

void VideoControlWidget::editWindow()
{
    auto *profile = selectedWindow();
    if (mUpdating || !profile) return;
    if (mName->text().trimmed().isEmpty() || mName->text().contains(QChar::Null)) {
        mName->setText(profile->name);
        message(tr("Укажите непустое название экрана без нулевых символов."), QStringLiteral("error"));
        return;
    }
    profile->name = mName->text().trimmed();
    profile->screen = mDisplays->currentData().toString();
    profile->fullscreen = mFullscreen->isChecked();
    mWindows->currentItem()->setText(profile->name);
    saveProfiles();
}

void VideoControlWidget::applyWindow()
{
    editWindow();
    if (const auto *profile = selectedWindow())
        submit(mClient->configureWindow(profile->id, profile->name, profile->screen, profile->fullscreen),
               tr("Применение окна"));
}

void VideoControlWidget::addPlaylist()
{
    auto *profile = selectedWindow();
    if (!profile) return;
    Playlist playlist{newId(), tr("Плейлист %1").arg(profile->playlists.size() + 1), {}};
    profile->playlists.append(playlist);
    profile->selectedPlaylist = playlist.id;
    saveProfiles();
    refreshPlaylists();
}

void VideoControlWidget::renamePlaylist()
{
    const auto *playlist = selectedPlaylist();
    if (!playlist) return;
    const QString windowId = selectedWindow()->id;
    const QString playlistId = playlist->id;
    bool ok = false;
    const QString name = QInputDialog::getText(this, tr("Название плейлиста"), tr("Название"),
                                              QLineEdit::Normal, playlist->name, &ok).trimmed().left(128);
    if (!ok || name.isEmpty() || name.contains(QChar::Null)) return;
    for (auto &profile : mProfiles) {
        if (profile.id != windowId) continue;
        for (auto &entry : profile.playlists)
            if (entry.id == playlistId) entry.name = name;
    }
    saveProfiles();
    refreshPlaylists();
}

void VideoControlWidget::removePlaylist()
{
    auto *profile = selectedWindow();
    if (!profile) return;
    for (int i = 0; i < profile->playlists.size(); ++i) {
        if (profile->playlists.at(i).id == profile->selectedPlaylist) {
            profile->playlists.removeAt(i);
            break;
        }
    }
    profile->selectedPlaylist = profile->playlists.isEmpty() ? QString() : profile->playlists.first().id;
    saveProfiles();
    refreshPlaylists();
}

void VideoControlWidget::addPaths(const QStringList &paths)
{
    auto *playlist = selectedPlaylist();
    if (!playlist || paths.isEmpty()) return;
    if (playlist->paths.size() + paths.size() > 1000) {
        message(tr("В плейлисте может быть не более 1000 файлов."), QStringLiteral("error"));
        return;
    }
    for (const auto &path : paths) {
        if (!absoluteMediaPath(path)) {
            message(tr("Укажите абсолютные пути на машине плеера, например /media/video.mp4 или C:/Video/clip.mp4."), QStringLiteral("error"));
            return;
        }
    }
    QStringList combined = playlist->paths;
    combined.append(paths);
    // Leave room for authentication and command metadata in the 1 MiB frame.
    if (QJsonDocument(QJsonArray::fromStringList(combined)).toJson(QJsonDocument::Compact).size() > 1000000) {
        message(tr("Список путей слишком велик для одной команды TCP."), QStringLiteral("error"));
        return;
    }
    playlist->paths = combined;
    saveProfiles();
    refreshPaths();
}

void VideoControlWidget::addRemotePaths()
{
    bool ok = false;
    const QString text = QInputDialog::getMultiLineText(this, tr("Добавить пути на машине плеера"),
            tr("Один абсолютный путь к видео на строку. Файлы должны быть доступны MediaBoxVPlayer."), {}, &ok);
    if (!ok) return;
    QStringList paths;
    for (const auto &line : text.split('\n')) {
        const QString path = line.trimmed();
        if (!path.isEmpty()) paths.append(path);
    }
    addPaths(paths);
}

void VideoControlWidget::removePaths()
{
    auto *playlist = selectedPlaylist();
    if (!playlist) return;
    for (int row = mPaths->count() - 1; row >= 0; --row)
        if (mPaths->item(row)->isSelected()) playlist->paths.removeAt(row);
    saveProfiles();
    refreshPaths();
}

void VideoControlWidget::movePath(int direction)
{
    auto *playlist = selectedPlaylist();
    const int row = mPaths->currentRow();
    if (!playlist || row < 0 || row + direction < 0 || row + direction >= playlist->paths.size()) return;
    playlist->paths.move(row, row + direction);
    saveProfiles();
    refreshPaths();
    mPaths->setCurrentRow(row + direction);
}

void VideoControlWidget::submit(const QString &id, const QString &description, const QString &removeWindow)
{
    if (id.isEmpty()) return;
    mPending.insert(id, Pending{description, removeWindow});
    message(tr("%1: ожидаем ответ плеера…").arg(description));
    updateActions();
}

void VideoControlWidget::message(const QString &text, const QString &role)
{
    mMessage->setColorRole(role);
    mMessage->setText(text);
}
