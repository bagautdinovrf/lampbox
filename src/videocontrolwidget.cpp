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
#include <QUuid>
#include <QVBoxLayout>

namespace {
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
    auto *transport = new QHBoxLayout;
    auto *previous = button(tr("Пред."), "videoPrevious", playback);
    auto *play = button(tr("Пуск"), "videoPlay", playback, true);
    auto *pause = button(tr("Пауза"), "videoPause", playback);
    auto *stop = button(tr("Стоп"), "videoStop", playback);
    auto *next = button(tr("След."), "videoNext", playback);
    mTransportButtons = {previous, play, pause, stop, next};
    for (auto *action : mTransportButtons) transport->addWidget(action);
    playbackLayout->addLayout(transport);
    mToggleFullscreen = button(tr("Переключить полный экран"), "videoToggleFullscreen", playback);
    playbackLayout->addWidget(mToggleFullscreen, 0, Qt::AlignLeft);
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
                + (playback.error.isEmpty() ? QString() : tr("\nОшибка: %1").arg(playback.error))
                + (confirmed->restoreError.isEmpty() ? QString() : tr("\nВосстановление: %1").arg(confirmed->restoreError)));
    }
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
