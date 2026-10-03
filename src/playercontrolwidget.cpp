#include "playercontrolwidget.h"

#include "restylewidgets.h"
#include "settings.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int seekSteps = 10000;
constexpr qint64 maximumSeekPosition = 9007199254740991LL;

RestyleLabel *label(const QString &text, QWidget *parent, int size = 12,
                    int weight = QFont::Normal)
{
    auto *result = new RestyleLabel(text, size, weight, parent);
    result->setTextFormat(Qt::PlainText);
    result->setWordWrap(true);
    result->setMinimumWidth(0);
    result->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    return result;
}

QPushButton *button(const QString &text, const char *name, QWidget *parent,
                    const QString &role = QStringLiteral("normal"))
{
    auto *result = new QPushButton(text, parent);
    result->setObjectName(QString::fromLatin1(name));
    result->setAccessibleName(text);
    result->setFont(Restyle::font(11, QFont::DemiBold));
    result->setMinimumHeight(32);
    result->setAutoDefault(false);
    Restyle::button(result, role);
    return result;
}

QString timeText(qint64 milliseconds)
{
    const qint64 seconds = qMax<qint64>(0, milliseconds) / 1000;
    if (seconds >= 3600)
        return QStringLiteral("%1:%2:%3").arg(seconds / 3600)
                .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
                .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(seconds / 60)
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QString commandName(const QString &command)
{
    if (command == "load") return QObject::tr("Замена очереди");
    if (command == "enqueue") return QObject::tr("Добавление в очередь");
    if (command == "play") return QObject::tr("Воспроизведение");
    if (command == "pause") return QObject::tr("Пауза");
    if (command == "stop") return QObject::tr("Остановка");
    if (command == "next") return QObject::tr("Следующая запись");
    if (command == "previous") return QObject::tr("Предыдущая запись");
    if (command == "seek") return QObject::tr("Перемотка");
    if (command == "volume") return QObject::tr("Громкость");
    if (command == "mute") return QObject::tr("Отключение звука");
    if (command == "repeat") return QObject::tr("Повтор");
    if (command == "clear") return QObject::tr("Очистка очереди");
    if (command == "status") return QObject::tr("Обновление состояния");
    return command;
}

QString stateName(const QString &state)
{
    if (state == "playing") return QObject::tr("Воспроизводится");
    if (state == "paused") return QObject::tr("Пауза");
    if (state == "loading") return QObject::tr("Загрузка");
    if (state == "error") return QObject::tr("Ошибка аудио");
    return QObject::tr("Остановлен");
}

bool absolutePlayerPath(const QString &path)
{
    // Paths belong to the remote platform: QDir::isAbsolutePath alone would
    // reject a Windows drive path when Manager runs on Linux.
    static const QRegularExpression drive(QStringLiteral("^[A-Za-z]:[/\\\\]"));
    return path.startsWith(QLatin1Char('/')) || path.startsWith(QStringLiteral("\\\\"))
            || drive.match(path).hasMatch();
}
}

PlayerControlWidget::PlayerControlWidget(MediaBoxPlayerClient *client, QWidget *parent)
    : QWidget(parent), mClient(client), mStatus(client->status()), mHasStatus(client->hasStatus())
{
    setObjectName(QStringLiteral("playerControlWidget"));
    setFont(Restyle::font());
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *body = new QWidget(scroll);
    auto *layout = new QVBoxLayout(body);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);
    scroll->setWidget(body);
    outer->addWidget(scroll);

    auto *heading = new QHBoxLayout;
    heading->addWidget(label(tr("MediaBoxPlayer"), body, 22, QFont::DemiBold), 1);
    auto *settings = button(tr("Подключение…"), "playerSettingsButton", body);
    settings->setToolTip(tr("Адрес, порт и токен плеера"));
    heading->addWidget(settings);
    connect(settings, &QPushButton::clicked, this, &PlayerControlWidget::settingsRequested);
    layout->addLayout(heading);

    auto *connection = new QHBoxLayout;
    connection->setSpacing(6);
    mConnectionLabel = label({}, body, 12, QFont::DemiBold);
    mConnectionLabel->setObjectName(QStringLiteral("playerConnectionState"));
    connection->addWidget(mConnectionLabel, 1);
    mConnect = button(tr("Подключить"), "playerConnectButton", body);
    mDisconnect = button(tr("Отключить"), "playerDisconnectButton", body);
    mRefresh = button(tr("Обновить"), "playerRefreshButton", body);
    connection->addWidget(mConnect);
    connection->addWidget(mDisconnect);
    connection->addWidget(mRefresh);
    layout->addLayout(connection);
    mFreshnessLabel = label({}, body, 11);
    mFreshnessLabel->setObjectName(QStringLiteral("playerStatusFreshness"));
    mFreshnessLabel->setColorRole(QStringLiteral("secondary"));
    layout->addWidget(mFreshnessLabel);
    mConnectionErrorLabel = label({}, body, 11);
    mConnectionErrorLabel->setObjectName(QStringLiteral("playerConnectionError"));
    mConnectionErrorLabel->setColorRole(QStringLiteral("error"));
    layout->addWidget(mConnectionErrorLabel);

    auto *playback = new RestylePanel(body);
    auto *playbackLayout = new QVBoxLayout(playback);
    playbackLayout->setContentsMargins(14, 12, 14, 12);
    playbackLayout->setSpacing(8);
    auto *state = new QHBoxLayout;
    mStateLabel = label({}, playback, 16, QFont::DemiBold);
    mStateLabel->setObjectName(QStringLiteral("playerPlaybackState"));
    state->addWidget(mStateLabel, 1);
    mIntentLabel = label({}, playback, 11);
    mIntentLabel->setObjectName(QStringLiteral("playerPlaybackIntent"));
    mIntentLabel->setColorRole(QStringLiteral("secondary"));
    state->addWidget(mIntentLabel, 1);
    playbackLayout->addLayout(state);
    mTrack = new QLineEdit(playback);
    mTrack->setObjectName(QStringLiteral("playerCurrentTrack"));
    mTrack->setReadOnly(true);
    mTrack->setAccessibleName(tr("Выбранный файл на машине плеера"));
    mTrack->setPlaceholderText(tr("Выбранный файл на машине плеера"));
    playbackLayout->addWidget(mTrack);
    mPositionLabel = label({}, playback, 11);
    mPositionLabel->setObjectName(QStringLiteral("playerPosition"));
    mPositionLabel->setColorRole(QStringLiteral("secondary"));
    playbackLayout->addWidget(mPositionLabel);
    mSeek = new QSlider(Qt::Horizontal, playback);
    mSeek->setObjectName(QStringLiteral("playerSeekSlider"));
    mSeek->setAccessibleName(tr("Позиция воспроизведения"));
    mSeek->setRange(0, seekSteps);
    mSeek->setPageStep(seekSteps / 20);
    playbackLayout->addWidget(mSeek);

    auto *transport = new QHBoxLayout;
    transport->setSpacing(6);
    mPrevious = button(tr("Назад"), "playerPreviousButton", playback);
    mPlay = button(tr("Играть"), "playerPlayButton", playback, QStringLiteral("primary"));
    mPause = button(tr("Пауза"), "playerPauseButton", playback);
    mStop = button(tr("Стоп"), "playerStopButton", playback);
    mNext = button(tr("Далее"), "playerNextButton", playback);
    for (auto *control : {mPrevious, mPlay, mPause, mStop, mNext})
        transport->addWidget(control, 1);
    playbackLayout->addLayout(transport);

    auto *sound = new QHBoxLayout;
    sound->setSpacing(8);
    mVolumeLabel = label({}, playback, 11);
    mVolumeLabel->setObjectName(QStringLiteral("playerVolumeValue"));
    mVolumeLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    sound->addWidget(mVolumeLabel);
    mVolume = new QSlider(Qt::Horizontal, playback);
    mVolume->setObjectName(QStringLiteral("playerVolumeSlider"));
    mVolume->setAccessibleName(tr("Громкость"));
    mVolume->setRange(0, 100);
    sound->addWidget(mVolume, 1);
    mMute = new QCheckBox(tr("Без звука"), playback);
    mMute->setObjectName(QStringLiteral("playerMuteCheckBox"));
    sound->addWidget(mMute);
    playbackLayout->addLayout(sound);

    auto *repeat = new QHBoxLayout;
    auto *repeatLabel = label(tr("Повтор"), playback, 11);
    repeatLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    repeat->addWidget(repeatLabel);
    mRepeat = new QComboBox(playback);
    mRepeat->setObjectName(QStringLiteral("playerRepeatComboBox"));
    mRepeat->setAccessibleName(tr("Режим повтора"));
    mRepeat->addItem(tr("Выключен"), QStringLiteral("off"));
    mRepeat->addItem(tr("Вся очередь"), QStringLiteral("all"));
    mRepeat->addItem(tr("Одна запись"), QStringLiteral("one"));
    repeatLabel->setBuddy(mRepeat);
    repeat->addWidget(mRepeat, 1);
    playbackLayout->addLayout(repeat);
    mAudioErrorLabel = label({}, playback, 11);
    mAudioErrorLabel->setObjectName(QStringLiteral("playerAudioError"));
    mAudioErrorLabel->setColorRole(QStringLiteral("error"));
    playbackLayout->addWidget(mAudioErrorLabel);
    layout->addWidget(playback);

    auto *queuePanel = new RestylePanel(body);
    auto *queueLayout = new QVBoxLayout(queuePanel);
    queueLayout->setContentsMargins(14, 12, 14, 12);
    queueLayout->setSpacing(8);
    mQueueLabel = label({}, queuePanel, 14, QFont::DemiBold);
    mQueueLabel->setObjectName(QStringLiteral("playerQueueSummary"));
    queueLayout->addWidget(mQueueLabel);
    auto *queueActions = new QHBoxLayout;
    queueActions->setSpacing(6);
    mLoad = button(tr("Заменить…"), "playerLoadButton", queuePanel);
    mEnqueue = button(tr("Добавить…"), "playerEnqueueButton", queuePanel);
    mClear = button(tr("Очистить"), "playerClearButton", queuePanel, QStringLiteral("danger"));
    queueActions->addWidget(mLoad);
    queueActions->addWidget(mEnqueue);
    queueActions->addStretch();
    queueActions->addWidget(mClear);
    queueLayout->addLayout(queueActions);
    mQueue = new QListWidget(queuePanel);
    mQueue->setObjectName(QStringLiteral("playerQueueList"));
    mQueue->setAccessibleName(tr("Подтверждённая очередь плеера"));
    mQueue->setSelectionMode(QAbstractItemView::NoSelection);
    mQueue->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mQueue->setUniformItemSizes(true);
    mQueue->setTextElideMode(Qt::ElideMiddle);
    mQueue->setMinimumHeight(116);
    queueLayout->addWidget(mQueue, 1);
    auto *queueHelp = label(tr("Файлы должны быть доступны на машине плеера. Текущая запись выделена жирным."),
                            queuePanel, 11);
    queueHelp->setColorRole(QStringLiteral("secondary"));
    queueLayout->addWidget(queueHelp);
    layout->addWidget(queuePanel, 1);

    mNotice = new QWidget(body);
    auto *noticeLayout = new QHBoxLayout(mNotice);
    noticeLayout->setContentsMargins(0, 0, 0, 0);
    mNoticeLabel = label({}, mNotice, 11);
    mNoticeLabel->setObjectName(QStringLiteral("playerCommandResult"));
    mNoticeLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    noticeLayout->addWidget(mNoticeLabel, 1);
    auto *dismiss = button(tr("Скрыть"), "playerDismissNoticeButton", mNotice);
    noticeLayout->addWidget(dismiss, 0, Qt::AlignTop);
    connect(dismiss, &QPushButton::clicked, this, [this] {
        mCommandMessage.clear();
        mUnknownCommands.clear();
        refreshNotice();
    });
    layout->addWidget(mNotice);

    connect(mConnect, &QPushButton::clicked, this, [this] {
        Settings settings;
        mClient->connectToPlayer(settings.playerConnection());
    });
    connect(mDisconnect, &QPushButton::clicked, mClient, &MediaBoxPlayerClient::disconnectFromPlayer);
    connect(mRefresh, &QPushButton::clicked, mClient, &MediaBoxPlayerClient::requestStatus);
    connect(mPlay, &QPushButton::clicked, mClient, &MediaBoxPlayerClient::play);
    connect(mPause, &QPushButton::clicked, mClient, &MediaBoxPlayerClient::pause);
    connect(mStop, &QPushButton::clicked, mClient, &MediaBoxPlayerClient::stop);
    connect(mPrevious, &QPushButton::clicked, mClient, &MediaBoxPlayerClient::previous);
    connect(mNext, &QPushButton::clicked, mClient, &MediaBoxPlayerClient::next);
    connect(mClear, &QPushButton::clicked, mClient, &MediaBoxPlayerClient::clear);
    connect(mLoad, &QPushButton::clicked, this, [this] { editPaths(false); });
    connect(mEnqueue, &QPushButton::clicked, this, [this] { editPaths(true); });
    connect(mMute, &QCheckBox::clicked, this, [this](bool muted) {
        mClient->setMuted(muted);
        refresh();
    });
    connect(mRepeat, &QComboBox::activated, this, [this](int index) {
        mClient->setRepeat(mRepeat->itemData(index).toString());
        refresh();
    });
    connect(mSeek, &QSlider::sliderPressed, this, [this] {
        mSeekTrack = mStatus.currentTrack;
        mSeekIndex = mStatus.currentIndex;
    });
    connect(mSeek, &QSlider::sliderReleased, this, &PlayerControlWidget::commitSeek);
    connect(mVolume, &QSlider::sliderReleased, this, &PlayerControlWidget::commitVolume);
    // actionTriggered also covers keyboard, wheel and clicks on the groove.
    // The value is updated after that signal, so commit on the next event turn.
    connect(mSeek, &QSlider::actionTriggered, this, [this] {
        if (!mSeek->isSliderDown()) {
            mSeekTrack = mStatus.currentTrack;
            mSeekIndex = mStatus.currentIndex;
            QTimer::singleShot(0, this, &PlayerControlWidget::commitSeek);
        }
    });
    connect(mVolume, &QSlider::actionTriggered, this, [this] {
        if (!mVolume->isSliderDown())
            QTimer::singleShot(0, this, &PlayerControlWidget::commitVolume);
    });
    connect(mClient, &MediaBoxPlayerClient::statusChanged, this, [this](const PlayerStatus &status) {
        mStatus = status;
        mHasStatus = true;
        mReceivedAt = QDateTime::currentDateTime();
        refresh();
    });
    connect(mClient, &MediaBoxPlayerClient::connectionStateChanged, this, [this] {
        mHasStatus = mClient->hasStatus();
        if (mClient->isReady())
            mConnectionMessage.clear();
        refresh();
    });
    connect(mClient, &MediaBoxPlayerClient::connectionError, this, [this](const QString &message) {
        mConnectionMessage = message;
        refresh();
    });
    connect(mClient, &MediaBoxPlayerClient::commandFailed, this,
            [this](const QString &, const QString &command, const QString &code, const QString &message) {
        mCommandMessage = tr("%1: %2 (%3)").arg(commandName(command), message, code);
        mCommandError = true;
        refreshNotice();
    });
    connect(mClient, &MediaBoxPlayerClient::commandOutcomeUnknown, this,
            [this](const QString &, const QString &command) {
        const QString name = commandName(command);
        if (!mUnknownCommands.contains(name))
            mUnknownCommands.append(name);
        refreshNotice();
    });
    connect(mClient, &MediaBoxPlayerClient::commandCancelled, this,
            [this](const QString &, const QString &command) {
        if (command == "status") return;
        mCommandMessage = tr("%1: команда отменена до отправки.").arg(commandName(command));
        mCommandError = false;
        refreshNotice();
    });
    connect(mClient, &MediaBoxPlayerClient::commandSucceeded, this,
            [this](const QString &, const QString &command) {
        if (command == "status") return;
        mCommandMessage = tr("%1: команда принята. Состояние показано по ответу плеера.")
                .arg(commandName(command));
        mCommandError = false;
        refreshNotice();
    });
    refresh();
    refreshNotice();
}

void PlayerControlWidget::refresh()
{
    using State = MediaBoxPlayerClient::ConnectionState;
    const State connection = mClient->connectionState();
    const bool ready = mClient->isReady();
    QString connectionText;
    switch (connection) {
    case State::Disconnected: connectionText = tr("Нет связи"); break;
    case State::Connecting: connectionText = tr("Подключение…"); break;
    case State::Synchronizing: connectionText = tr("Получение состояния…"); break;
    case State::Ready: connectionText = tr("Подключён"); break;
    case State::Reconnecting: connectionText = tr("Восстановление связи…"); break;
    case State::AuthenticationFailed: connectionText = tr("Неверный токен"); break;
    case State::ProtocolMismatch: connectionText = tr("Несовместимая версия API"); break;
    }
    mConnectionLabel->setText(connectionText);
    mConnectionLabel->setColorRole(ready ? QStringLiteral("success") : QStringLiteral("warning"));
    mConnect->setText(ready || connection == State::Reconnecting ? tr("Переподключить") : tr("Подключить"));
    mConnect->setEnabled(connection != State::Connecting && connection != State::Synchronizing);
    mDisconnect->setEnabled(connection != State::Disconnected);
    mRefresh->setEnabled(ready);
    mConnectionErrorLabel->setText(mConnectionMessage);
    mConnectionErrorLabel->setVisible(!mConnectionMessage.isEmpty());

    if (!mHasStatus)
        mFreshnessLabel->setText(tr("Подтверждённые данные плеера ещё не получены."));
    else if (!ready)
        mFreshnessLabel->setText(tr("Показан устаревший снимок. Воспроизведение на плеере может продолжаться."));
    else if (mReceivedAt.isValid())
        mFreshnessLabel->setText(tr("Подтверждено плеером · %1").arg(mReceivedAt.toString(QStringLiteral("HH:mm:ss"))));
    else
        mFreshnessLabel->setText(tr("Последнее подтверждённое состояние плеера."));

    const bool hasTrack = mHasStatus && !mStatus.queue.isEmpty() && mStatus.currentIndex >= 0;
    mPlay->setEnabled(ready && hasTrack);
    mPause->setEnabled(ready);
    mStop->setEnabled(ready);
    mPrevious->setEnabled(ready && hasTrack);
    mNext->setEnabled(ready && hasTrack);
    mLoad->setEnabled(ready);
    mEnqueue->setEnabled(ready && mStatus.queue.size() < 1000);
    mClear->setEnabled(ready && hasTrack);
    mVolume->setEnabled(ready);
    mMute->setEnabled(ready);
    mRepeat->setEnabled(ready);
    mSeek->setEnabled(ready && hasTrack && mStatus.durationMs > 0);
    mStateLabel->setText(mHasStatus ? (ready ? stateName(mStatus.state)
                                     : tr("Последнее: %1").arg(stateName(mStatus.state)))
                                  : tr("Состояние неизвестно"));
    mStateLabel->setColorRole(mHasStatus && ready && mStatus.state == "error"
                             ? QStringLiteral("error") : QStringLiteral("text"));
    mIntentLabel->setText(mHasStatus && mStatus.playbackRequested && mStatus.state != "playing"
                         ? tr("Воспроизведение запрошено") : QString());
    mIntentLabel->setVisible(!mIntentLabel->text().isEmpty());
    const QString track = mHasStatus ? mStatus.currentTrack : QString();
    if (mTrack->text() != track) {
        mTrack->setText(track);
        mTrack->setCursorPosition(0);
    }
    mPositionLabel->setText(mHasStatus ? tr("Позиция плеера: %1 / %2")
                           .arg(timeText(mStatus.positionMs), mStatus.durationMs > 0
                                ? timeText(mStatus.durationMs) : tr("длительность неизвестна"))
                                     : tr("Позиция плеера: —"));
    if (!mSeek->isSliderDown()) {
        const QSignalBlocker blocker(mSeek);
        mSeek->setValue(mHasStatus && mStatus.durationMs > 0
                       ? int(qBound(0.0, double(mStatus.positionMs) / double(mStatus.durationMs), 1.0) * seekSteps)
                       : 0);
    }
    mSeek->setToolTip(mStatus.durationMs > 0 ? tr("Перемотка после отпускания ползунка")
                                           : tr("Перемотка доступна после получения длительности"));
    if (!mVolume->isSliderDown()) {
        const QSignalBlocker blocker(mVolume);
        mVolume->setValue(mHasStatus ? mStatus.volumePercent : 0);
    }
    mVolumeLabel->setText(mHasStatus ? tr("Громкость %1 %").arg(mStatus.volumePercent) : tr("Громкость —"));
    {
        const QSignalBlocker blocker(mMute);
        mMute->setChecked(mHasStatus && mStatus.muted);
    }
    {
        const QSignalBlocker blocker(mRepeat);
        mRepeat->setCurrentIndex(mHasStatus ? mRepeat->findData(mStatus.repeat) : -1);
    }
    mAudioErrorLabel->setText(mHasStatus ? mStatus.error : QString());
    mAudioErrorLabel->setVisible(mHasStatus && !mStatus.error.isEmpty());

    const QStringList queue = mHasStatus ? mStatus.queue : QStringList();
    const int currentIndex = mHasStatus ? mStatus.currentIndex : -1;
    mQueueLabel->setText(mHasStatus ? tr("Очередь · записей: %1%2").arg(queue.size())
                        .arg(hasTrack ? tr(" · текущая: %1").arg(currentIndex + 1) : QString())
                                  : tr("Очередь · нет данных"));
    if (mDisplayedQueue != queue || mDisplayedIndex != currentIndex) {
        const bool changedQueue = mDisplayedQueue != queue;
        if (changedQueue) {
            mQueue->clear();
            for (int i = 0; i < queue.size(); ++i) {
                auto *item = new QListWidgetItem(tr("%1. %2").arg(i + 1).arg(queue.at(i)), mQueue);
                item->setSizeHint(QSize(0, 27));
            }
        }
        for (int i = 0; i < mQueue->count(); ++i)
            mQueue->item(i)->setFont(Restyle::font(11, i == currentIndex ? QFont::DemiBold : QFont::Normal));
        if (currentIndex >= 0 && currentIndex < mQueue->count())
            mQueue->scrollToItem(mQueue->item(currentIndex));
        mDisplayedQueue = queue;
        mDisplayedIndex = currentIndex;
    }
}

void PlayerControlWidget::refreshNotice()
{
    QString message;
    if (!mUnknownCommands.isEmpty())
        message = tr("Результат неизвестен: %1. Ответ потерян; команды автоматически не повторяются. "
                     "После восстановления связи проверьте состояние перед новым действием.")
                .arg(mUnknownCommands.join(QStringLiteral(", ")));
    if (!mCommandMessage.isEmpty()) {
        if (!message.isEmpty()) message += QLatin1Char('\n');
        message += mCommandMessage;
    }
    mNoticeLabel->setText(message);
    mNoticeLabel->setColorRole(!mUnknownCommands.isEmpty() ? QStringLiteral("warning")
                              : mCommandError ? QStringLiteral("error") : QStringLiteral("secondary"));
    mNotice->setVisible(!message.isEmpty());
}

void PlayerControlWidget::commitSeek()
{
    if (mClient->isReady() && mStatus.durationMs > 0
            && mSeekTrack == mStatus.currentTrack && mSeekIndex == mStatus.currentIndex) {
        // Status positions may be larger than the protocol's maximum seek
        // argument. Bound the floating-point value before converting to qint64.
        const double target = double(mStatus.durationMs) * mSeek->value() / seekSteps;
        const qint64 position = qRound64(qBound(0.0, target, double(maximumSeekPosition)));
        mClient->seek(qBound<qint64>(0, position, mStatus.durationMs));
    }
    refresh();
}

void PlayerControlWidget::commitVolume()
{
    if (mClient->isReady())
        mClient->setVolume(mVolume->value());
    refresh();
}

void PlayerControlWidget::editPaths(bool append)
{
    QDialog dialog(this);
    dialog.setObjectName(QStringLiteral("playerPathsDialog"));
    dialog.setWindowTitle(append ? tr("Добавить в очередь MediaBoxPlayer") : tr("Заменить очередь MediaBoxPlayer"));
    dialog.setFont(Restyle::font());
    dialog.resize(640, 380);
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(10);
    auto *explanation = label(append
        ? tr("Добавить файлы в конец очереди. Текущий трек продолжит воспроизведение.")
        : tr("Вся очередь будет заменена, воспроизведение остановится. Для запуска нажмите «Играть»."), &dialog);
    layout->addWidget(explanation);
    auto *instructions = label(tr("Абсолютные пути на машине плеера — по одному на строке. "
                                   "Файлы должны быть уже размещены там; Manager их не пересылает."), &dialog, 11);
    instructions->setColorRole(QStringLiteral("secondary"));
    layout->addWidget(instructions);
    auto *paths = new QPlainTextEdit(&dialog);
    paths->setObjectName(QStringLiteral("playerRemotePathsEdit"));
    paths->setAccessibleName(tr("Абсолютные пути на машине плеера"));
    paths->setPlaceholderText(QStringLiteral("/srv/music/Песня.mp3\nC:/Media/music/Песня.mp3"));
    paths->setLineWrapMode(QPlainTextEdit::NoWrap);
    layout->addWidget(paths, 1);
    auto *error = label({}, &dialog, 11);
    error->setObjectName(QStringLiteral("playerPathsError"));
    error->setColorRole(QStringLiteral("error"));
    error->hide();
    layout->addWidget(error);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Отмена"));
    auto *accept = buttons->addButton(append ? tr("Добавить") : tr("Заменить и остановить"),
                                       QDialogButtonBox::AcceptRole);
    accept->setObjectName(QStringLiteral("playerSubmitPathsButton"));
    Restyle::button(accept, append ? QStringLiteral("primary") : QStringLiteral("danger"));
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(mClient, &MediaBoxPlayerClient::connectionStateChanged, &dialog, [this, accept] {
        accept->setEnabled(mClient->isReady());
    });
    connect(accept, &QPushButton::clicked, &dialog, [this, append, paths, error, &dialog] {
        QStringList entries;
        QString validation;
        const QStringList lines = paths->toPlainText().split(QLatin1Char('\n'));
        for (int i = 0; i < lines.size(); ++i) {
            const QString path = lines.at(i).trimmed();
            if (path.isEmpty()) continue;
            if (path.size() > 4096 || path.contains(QChar::Null) || !absolutePlayerPath(path)) {
                validation = tr("Строка %1: нужен абсолютный путь к файлу на машине плеера "
                                "(до 4096 символов, без URL и NUL).").arg(i + 1);
                break;
            }
            entries.append(path);
        }
        if (validation.isEmpty() && entries.isEmpty())
            validation = tr("Укажите хотя бы один путь.");
        if (validation.isEmpty() && entries.size() + (append ? mStatus.queue.size() : 0) > 1000)
            validation = tr("В очереди может быть не более 1000 записей.");
        if (validation.isEmpty() && !mClient->isReady())
            validation = tr("Нет связи с плеером. Дождитесь получения состояния.");
        if (!validation.isEmpty()) {
            error->setText(validation);
            error->show();
            return;
        }
        const QString id = append ? mClient->enqueue(entries) : mClient->load(entries, 0, false);
        if (id.isEmpty()) {
            error->setText(tr("Команда не отправлена. Проверьте подключение и размер списка."));
            error->show();
            return;
        }
        dialog.accept();
    });
    layout->addWidget(buttons);
    paths->setFocus();
    dialog.exec();
}
