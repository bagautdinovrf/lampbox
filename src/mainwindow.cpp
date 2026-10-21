#include "mainwindow.h"
#include "aboutmediaboxmanager.h"
#include "advertmanager.h"
#include "advertmodel.h"
#include "channelmanager.h"
#include "channelmodel.h"
#include "informer.h"
#include "mediacontroller.h"
#include "videocontroller.h"
#include "playercontrolwidget.h"
#include "videocontrolwidget.h"
#include "medialibrarydelegate.h"
#include "mediamanager.h"
#include "mediaimportservice.h"
#include "mediamodel.h"
#include "report.h"
#include "restyletheme.h"
#include "restylewidgets.h"
#include "ruleeditors.h"
#include "schedulepreview.h"
#include "schedulepublication.h"
#include "scheduledocumentdialog.h"
#include "settings.h"
#include "settingsdialog.h"
#include "stationmanager.h"
#include "trialmessagebox.h"
#include <QApplication>
#include <QBoxLayout>
#include <QComboBox>
#include <QCloseEvent>
#include <QDateTimeEdit>
#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QButtonGroup>
#include <QGridLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QProgressDialog>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QTimeEdit>
#include <QTreeView>
#include <QTimer>
#include <QToolTip>
#include <QUrl>
#include <algorithm>
#include <functional>

namespace {
QLabel *label(const QString &text, int size = 11, int weight = QFont::Normal, QWidget *parent = nullptr) {
    auto *w = new QLabel(text, parent);
    w->setFont(Restyle::font(size, weight));
    w->setTextFormat(Qt::PlainText);
    if (size <= 11 && weight == QFont::Normal)
        w->setForegroundRole(QPalette::PlaceholderText);
    return w;
}
QPushButton *button(const QString &text, const QString &icon, const QString &role = "normal") {
    auto *w = new QPushButton(text);
    w->setIcon(Restyle::icon(icon));
    w->setProperty("restyleIcon", icon);
    w->setIconSize(QSize(14, 14));
    w->setFont(Restyle::font(11, QFont::DemiBold));
    w->setFixedHeight(32);
    Restyle::button(w, role);
    w->setAccessibleName(text);
    return w;
}
QPushButton *iconButton(const QString &icon, const QString &tip) {
    auto *w = button({}, icon, "icon");
    w->setFixedSize(29, 29);
    w->setToolTip(tip);
    w->setAccessibleName(tip);
    return w;
}
QVBoxLayout *column(QWidget *w, int margin = 14, int spacing = 10) {
    auto *l = new QVBoxLayout(w);
    l->setContentsMargins(margin, margin, margin, margin);
    l->setSpacing(spacing);
    return l;
}
void configureTable(QTableView *view) {
    view->setFrameShape(QFrame::NoFrame);
    view->setShowGrid(false);
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->verticalHeader()->hide();
    view->verticalHeader()->setDefaultSectionSize(32);
    view->horizontalHeader()->setFont(Restyle::font(10));
    view->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    view->horizontalHeader()->setStretchLastSection(true);
}
bool confirmDelete(QWidget *parent, const QString &title, const QString &text) {
    QMessageBox dialog(QMessageBox::NoIcon, title, text, QMessageBox::Yes | QMessageBox::Cancel, parent);
    dialog.setTextFormat(Qt::PlainText);
    dialog.button(QMessageBox::Yes)->setText("Удалить");
    dialog.button(QMessageBox::Cancel)->setText("Отмена");
    Restyle::button(dialog.button(QMessageBox::Yes), "danger");
    dialog.setDefaultButton(QMessageBox::Cancel);
    dialog.setEscapeButton(QMessageBox::Cancel);
    return dialog.exec() == QMessageBox::Yes;
}
class ChannelDelegate final : public QStyledItemDelegate {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override {
        return {180, 65};
    }
    void paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const override {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRectF r = o.rect.adjusted(0, 2, -1, -2);
        if (o.state & QStyle::State_Selected)
            Restyle::paintSurface(*p, r, "selected", true);
        else if (o.state & QStyle::State_MouseOver) {
            p->setPen(Qt::NoPen);
            p->setBrush(Restyle::tokens().surface2);
            p->drawRoundedRect(r, 7, 7);
        }
        p->setFont(Restyle::font(11, QFont::DemiBold));
        p->setPen(o.state & QStyle::State_Selected ? Restyle::tokens().accentText : Restyle::tokens().text);
        p->drawText(o.rect.adjusted(7, 5, -7, -38), Qt::AlignVCenter,
                    p->fontMetrics().elidedText(i.data().toString(), Qt::ElideRight, o.rect.width() - 14));
        p->setFont(Restyle::font(9));
        p->setPen(Restyle::tokens().secondary);
        const auto counts = parent()->property("channelFileCounts").toStringList();
        const auto statuses = parent()->property("channelStatuses").toStringList();
        const auto active = parent()->property("channelActive").toList();
        // Second line: time window and calendar conditions (the former channel card).
        const QString window = i.siblingAtColumn(1).data().toString() + "–" + i.siblingAtColumn(2).data().toString();
        p->drawText(o.rect.adjusted(7, 23, -7, -23), Qt::AlignVCenter,
                    p->fontMetrics().elidedText(window + " · " + calendar(i), Qt::ElideRight, o.rect.width() - 14));
        const QString status = statuses.value(i.row(), "План не рассчитан");
        const bool now = active.value(i.row()).toBool();
        const QString files = counts.value(i.row(), "0") + " файлов";
        const int filesWidth = p->fontMetrics().horizontalAdvance(files);
        QRect badge(o.rect.left() + 7, o.rect.top() + 42,
                    qMax(0, qMin(o.rect.width() - 24 - filesWidth, p->fontMetrics().horizontalAdvance(status) + 10)), 14);
        p->setPen(now ? Restyle::tokens().success : Restyle::tokens().line);
        p->setBrush(now ? Restyle::tokens().successBg : Restyle::tokens().surface);
        p->drawRoundedRect(badge, 2, 2);
        p->setPen(now ? Restyle::tokens().success : Restyle::tokens().secondary);
        p->drawText(badge.adjusted(4, 0, -4, 0), Qt::AlignVCenter,
                    p->fontMetrics().elidedText(status, Qt::ElideRight, badge.width() - 8));
        p->setPen(Restyle::tokens().secondary);
        p->drawText(QRect(badge.right() + 8, badge.top(), filesWidth + 2, badge.height()), Qt::AlignVCenter, files);
        if (o.state & QStyle::State_HasFocus) {
            p->setPen(QPen(Restyle::tokens().focus, 2));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(r.adjusted(2, 2, -2, -2), 7, 7);
        }
        p->restore();
    }
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option,
                   const QModelIndex &i) override {
        if (event->type() != QEvent::ToolTip || !i.isValid())
            return QStyledItemDelegate::helpEvent(event, view, option, i);
        QToolTip::showText(event->globalPos(),
                           QStringLiteral("%1\n%2–%3 · %4\nГромкость %5%")
                               .arg(i.data().toString(), i.siblingAtColumn(1).data().toString(),
                                    i.siblingAtColumn(2).data().toString(), calendar(i))
                               .arg(i.siblingAtColumn(6).data(Qt::EditRole).toInt()),
                           view);
        return true;
    }

  private:
    static QString calendar(const QModelIndex &i) {
        return RuleEditors::describeCalendar(i.siblingAtColumn(3).data(Qt::EditRole).toString(),
                                             i.siblingAtColumn(4).data(Qt::EditRole).toString(),
                                             i.siblingAtColumn(5).data(Qt::EditRole).toString());
    }
};
QString known(const QString &s) {
    return s.trimmed().isEmpty() ? QStringLiteral("Не указано") : s;
}
QStringList channelFiles(MediaManager &media) {
    QStringList paths;
    const QDir directory = media.getDirMediaFiles();
    for (const auto &file : directory.entryInfoList(media.libraryFormats(), QDir::Files, QDir::Name))
        paths.append(file.absoluteFilePath());
    return paths;
}
} // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setWindowTitle(QStringLiteral("MediaBoxManager %1").arg(VERSION));
    mScheduleUpdateTimer = new QTimer(this);
    mScheduleUpdateTimer->setSingleShot(true);
    mScheduleUpdateTimer->setInterval(250);
    connect(mScheduleUpdateTimer, &QTimer::timeout, this, &MainWindow::refreshPlaybackSchedules);
    auto *renewal = new QTimer(this);
    renewal->setInterval(60 * 60 * 1000);
    connect(renewal, &QTimer::timeout, this, &MainWindow::refreshPlaybackSchedules);
    renewal->start();
    for (int i = 0; i < 2; ++i) {
        mChannelManagers[i] = new ChannelManager(i == 0 ? MUSIC : VIDEO);
        mChannelManagers[i]->collectChannels();
        mChannelModels[i] = new ChannelModel(mChannelManagers[i], this);
        mChannelManagers[i]->setChannelModel(mChannelModels[i]);
    }
    mMediaAdvertManager = std::make_unique<MediaManager>(SPathData().advertDir, ADVERT);
    mAdvertManager = std::make_unique<AdvertManager>();
    mAdvertModel = new AdvertModel(mAdvertManager.get(), this);
    connect(mAdvertManager.get(), SIGNAL(beginCollect()), mAdvertModel, SLOT(beginReset()));
    connect(mAdvertManager.get(), SIGNAL(endCollect()), mAdvertModel, SLOT(endReset()));
    buildShell();
    for (int i = 0; i < 3; ++i)
        mStack->addWidget(buildMediaPage(i));
    mStack->addWidget(new Report(mStack, Qt::Widget));
    auto *settings = new SettingsDialog(mStack, Qt::Widget);
    settings->setWindowFlags(Qt::Widget);
    connect(settings, &SettingsDialog::doneRequested, this, [this] { changePage(mPage); });
    connect(settings, &SettingsDialog::videoScreensRequested, this, &MainWindow::showVideoControls);
    mStack->addWidget(settings);
    auto *about = new AboutMediaBoxManager(mStack, Qt::Widget);
    about->setWindowFlags(Qt::Widget);
    mStack->addWidget(about);
    connect(about, &AboutMediaBoxManager::doneRequested, this, [this] { changePage(mPage); });
    for (int i = 0; i < 2; ++i) {
        connect(mChannelModels[i], &QAbstractItemModel::dataChanged, this, [this, i] {
            updatePage(i); mScheduleUpdateTimer->start();
        });
        connect(mChannelModels[i], &QAbstractItemModel::modelReset, this, [this, i] {
            updatePage(i); mScheduleUpdateTimer->start();
        });
        if (mChannelModels[i]->rowCount())
            mPages[i].channels->selectRow(0);
        updatePage(i);
    }
    connect(mAdvertModel, &QAbstractItemModel::dataChanged, this, [this] {
        updatePage(2); mScheduleUpdateTimer->start();
    });
    connect(mAdvertModel, &QAbstractItemModel::modelReset, this, [this] {
        updatePage(2); mScheduleUpdateTimer->start();
    });
    updatePage(2);
    if (qApp->property("restylePreviewStation").toString().isEmpty()) {
        mMediaController = new MediaController(this);
        mVideoController = new VideoController(this);
        connect(mVideoController, &MediaBoxVPlayerClient::videoStatusChanged,
                this, [this] { updatePlaybackActions(); });
        connect(mVideoController, &MediaBoxPlayerClient::connectionStateChanged,
                this, [this] { updatePlaybackActions(); });
        connect(mVideoController, &MediaBoxPlayerClient::commandFailed, this,
                [](const QString &, const QString &command, const QString &code, const QString &message) {
            Informer::Instance().infoEvent(QStringLiteral("MediaBoxVPlayer: команда %1 — %2 (%3)")
                                          .arg(command, message, code), Informer::ERROR);
        });
        connect(mVideoController, &MediaBoxPlayerClient::commandOutcomeUnknown, this,
                [](const QString &, const QString &command) {
            Informer::Instance().infoEvent(QStringLiteral("MediaBoxVPlayer: результат команды %1 неизвестен. "
                                          "Проверьте состояние перед повторным действием.").arg(command), Informer::ERROR);
        });
        connect(mVideoController, &MediaBoxPlayerClient::commandCancelled, this,
                [](const QString &, const QString &command) {
            if (command != "status")
                Informer::Instance().infoEvent(QStringLiteral("MediaBoxVPlayer: команда %1 не отправлена.").arg(command), Informer::ERROR);
        });
        connect(settings, &SettingsDialog::playerConnectionChanged,
                mMediaController, &MediaController::reloadConnection);
        connect(settings, &SettingsDialog::videoPlayerConnectionChanged,
                mVideoController, &VideoController::reloadConnection);
        connect(mVideoController, &MediaBoxPlayerClient::connectionError, this,
                [this](const QString &message) {
            const QString text = QStringLiteral("MediaBoxVPlayer: %1").arg(message);
            mVideoConnectionMessage = text;
            mOperationState->setText(text);
            mOperationState->setToolTip(text);
        });
        connect(mVideoController, &MediaBoxPlayerClient::connectionStateChanged, this,
                [this](MediaBoxPlayerClient::ConnectionState state) {
            if (state != MediaBoxPlayerClient::ConnectionState::Ready)
                return;
            if (!mVideoConnectionMessage.isEmpty() && mOperationState->text() == mVideoConnectionMessage) {
                const QString text = QStringLiteral("MediaBoxVPlayer подключён.");
                mOperationState->setText(text);
                mOperationState->setToolTip(text);
            }
            mVideoConnectionMessage.clear();
            mScheduleUpdateTimer->start();
        });
        connect(mMediaController, &MediaBoxPlayerClient::connectionStateChanged,
                this, &MainWindow::updatePlayerState);
        connect(mMediaController, &MediaBoxPlayerClient::statusChanged,
                this, &MainWindow::updatePlayerState);
        auto report = [this](const QString &message) {
            const QString text = mUnknownPlayerCommand.isEmpty() ? message
                : QStringLiteral("Результат %1 неизвестен. %2").arg(mUnknownPlayerCommand, message);
            mOperationState->setText(text);
            mOperationState->setToolTip(text);
        };
        connect(mMediaController, &MediaBoxPlayerClient::connectionError, this,
                [this, report](const QString &message) {
            report(message);
            mAudioConnectionMessage = mUnknownPlayerCommand.isEmpty() ? message : QString();
        });
        connect(mMediaController, &MediaBoxPlayerClient::connectionStateChanged, this,
                [this](MediaBoxPlayerClient::ConnectionState state) {
            if (state != MediaBoxPlayerClient::ConnectionState::Ready)
                return;
            if (!mAudioConnectionMessage.isEmpty() && mOperationState->text() == mAudioConnectionMessage) {
                const QString text = QStringLiteral("MediaBoxPlayer подключён.");
                mOperationState->setText(text);
                mOperationState->setToolTip(text);
            }
            mAudioConnectionMessage.clear();
            mScheduleUpdateTimer->start();
        });
        connect(mMediaController, &MediaBoxPlayerClient::commandFailed, this,
                [report](const QString &, const QString &command, const QString &code, const QString &message) {
            report(QStringLiteral("Команда %1: %2 (%3)").arg(command, message, code));
        });
        connect(mMediaController, &MediaBoxPlayerClient::commandOutcomeUnknown, this,
                [this, report](const QString &, const QString &command) {
            mUnknownPlayerCommand = command;
            report(QStringLiteral("Ответ потерян. Проверьте состояние плеера перед новым действием."));
        });
        connect(mMediaController, &MediaBoxPlayerClient::commandCancelled, this,
                [report](const QString &, const QString &command) {
            if (command != "status")
                report(QStringLiteral("Неотправленная команда %1 отменена.").arg(command));
        });
        connect(mMediaController, &MediaBoxPlayerClient::commandSucceeded, this,
                [this, report](const QString &, const QString &command) {
            if (command != "status") {
                mUnknownPlayerCommand.clear();
                report(QStringLiteral("Плеер обработал команду %1").arg(command));
            }
        });
    }
    updatePlayerState();
    connect(&Informer::Instance(), &Informer::infoEventSignal, this, [this](const QString &s) {
        mOperationState->setText(s);
        mOperationState->setToolTip(s);
    });
    changePage(0);
    QStringList loadErrors;
    for (auto *manager : mChannelManagers)
        if (!manager->lastError().isEmpty())
            loadErrors.append(manager->lastError());
    if (!mAdvertManager->lastError().isEmpty())
        loadErrors.append(mAdvertManager->lastError());
    if (!loadErrors.isEmpty())
        Informer::Instance().infoEvent(loadErrors.join(QLatin1Char('\n')), Informer::ERROR);
    QSize available = screen()->availableGeometry().size();
    adaptLayout(available.width() - 24);
    resize(QSize(1440, 900).boundedTo(available - QSize(24, 48)));
    if (qApp->property("restylePreviewStation").toString().isEmpty()) {
        restoreGeometry(Settings().mainWindowGeometry());
        const QByteArray splitter = Settings().mainWindowSplitter();
        for (auto &page : mPages)
            if (!splitter.isEmpty() && page.splitter)
                page.splitter->restoreState(splitter);
    }
    adaptLayout(width());
    // The advert summary has no date picker: it always shows the current minute.
    auto *advertClock = new QTimer(this);
    advertClock->setInterval(60 * 1000);
    connect(advertClock, &QTimer::timeout, this, [this] { updateSummary(PAGE_ADVERT); });
    advertClock->start();
    setupShortcuts();
    QTimer::singleShot(0, this, &MainWindow::updateScheduleDocumentPreview);
}

void MainWindow::setupShortcuts() {
    const auto shortcut = [this](const QKeySequence &keys, QWidget *owner, Qt::ShortcutContext context,
                                 const std::function<void()> &action) {
        auto *s = new QShortcut(keys, owner);
        s->setContext(context);
        connect(s, &QShortcut::activated, this, action);
    };
    // Library and rule commands act on the media page that is on screen. They
    // click the visible control, so its enabled state and checks stay in one place.
    const auto onMediaPage = [this](const std::function<void(MediaPage &)> &action) {
        return [this, action] {
            if (mStack->currentIndex() == mPage)
                action(mPages[mPage]);
        };
    };
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_F), this, Qt::WindowShortcut, onMediaPage([](MediaPage &p) {
        p.search->setFocus(Qt::ShortcutFocusReason);
        p.search->selectAll();
    }));
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_N), this, Qt::WindowShortcut, onMediaPage([](MediaPage &p) {
        (p.createChannel ? p.createChannel : p.addAdvert)->click();
    }));
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_O), this, Qt::WindowShortcut, onMediaPage([](MediaPage &p) {
        if (p.addFiles->isEnabled())
            p.addFilesAction->trigger();
    }));
    for (int page = 0; page < 3; ++page)
        shortcut(QKeySequence(Qt::CTRL | Qt::Key(Qt::Key_1 + page)), this, Qt::WindowShortcut, [this, page] { changePage(page); });
    shortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma), this, Qt::WindowShortcut, [this] { changePage(4); });
    shortcut(QKeySequence(Qt::Key_F5), this, Qt::WindowShortcut, [this] {
        if (playerAvailable())
            mMediaController->refreshPlayer();
        updateScheduleDocumentPreview();
    });
    // Del, F2 and Enter belong to the list that has focus.
    for (int page = 0; page < 3; ++page) {
        auto &p = mPages[page];
        shortcut(QKeySequence(Qt::Key_Delete), p.files, Qt::WidgetWithChildrenShortcut,
                 [this, page] { mPages[page].deleteFiles->click(); });
        QAbstractItemView *rules = p.channels ? static_cast<QAbstractItemView *>(p.channels) : p.adverts;
        QPushButton *edit = p.channels ? p.editChannel : p.editAdvert;
        QPushButton *remove = p.channels ? p.deleteChannel : p.deleteAdvert;
        shortcut(QKeySequence(Qt::Key_Delete), rules, Qt::WidgetWithChildrenShortcut, [remove] { remove->click(); });
        for (const auto key : {Qt::Key_F2, Qt::Key_Return, Qt::Key_Enter})
            shortcut(QKeySequence(key), rules, Qt::WidgetWithChildrenShortcut, [edit] { edit->click(); });
    }
}
MainWindow::~MainWindow() {
    if (mMediaImport)
        mMediaImport->cancel();
    // Stop selection/model callbacks while tearing down the widgets. The data
    // managers must outlive every view that can still ask its model for data.
    for (auto *child : findChildren<QObject *>())
        QObject::disconnect(child, nullptr, this, nullptr);
    // Managers hold a non-owning model pointer; detach it before destroying views/models.
    for (auto &page : mPages)
        page.source->setMediaManager(nullptr);
    delete takeCentralWidget();
    for (int i = 0; i < 2; ++i) {
        for (int r = 0; r < mChannelManagers[i]->channelCount(); ++r)
            mChannelManagers[i]->channel(r).mediaManager().setMediaModel(nullptr);
        delete mChannelManagers[i];
    }
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (mMediaImport) {
        mCloseAfterImport = true;
        mMediaImport->cancel();
        mOperationState->setText("Завершение импорта…");
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
    if (event->isAccepted() && qApp->property("restylePreviewStation").toString().isEmpty()) {
        Settings().setMainWindowGeometry(saveGeometry());
        if (mPages[mPage].splitter)
            Settings().setMainWindowSplitter(mPages[mPage].splitter->saveState());
    }
}

void MainWindow::buildShell() {
    auto *root = new QWidget;
    setCentralWidget(root);
    auto *layout = column(root, 0, 0);
    auto *header = new RestylePanel(root, "header");
    header->setObjectName("appHeader");
    header->setFixedHeight(56);
    auto *h = new QHBoxLayout(header);
    h->setContentsMargins(22, 0, 22, 0);
    h->setSpacing(12);
    auto *brand = button({}, "music", "primary");
    brand->setFixedSize(28, 28);
    brand->setFocusPolicy(Qt::NoFocus);
    brand->setAttribute(Qt::WA_TransparentForMouseEvents);
    h->addWidget(brand);
    auto *brandName = label("MediaBoxManager", 15, QFont::DemiBold);
    brandName->setProperty("compactHide", true);
    h->addWidget(brandName);
    auto *context = label(StationManager::Instance().typeText(), 11);
    context->setProperty("compactHide", true);
    h->addWidget(context);
    h->addStretch();
    auto *station =
        button(StationManager::Instance().type() == STATION_NETWORK ? "Сетевая станция" : "Локальная станция",
               "info", "quiet");
    station->setObjectName("stationInfoButton");
    station->setToolTip("Сведения о станции");
    station->setProperty("fullText", station->text());
    h->addWidget(station);
    connect(station, &QPushButton::clicked, this, &MainWindow::showStationInfo);
    layout->addWidget(header);
    auto *nav = new RestylePanel(root, "nav");
    nav->setObjectName("navigation");
    nav->setFixedHeight(45);
    auto *n = new QHBoxLayout(nav);
    n->setContentsMargins(22, 5, 22, 5);
    n->setSpacing(5);
    QStringList titles = {"Музыка", "Видео", "Реклама", "Отчёты", "Настройки", "О программе"};
    QStringList icons = {"music", "video", "ads", "report", "settings", "info"};
    QStringList objectNames = {"music", "video", "advert", "reports", "settings", "about"};
    QStringList keys = {"Ctrl+1", "Ctrl+2", "Ctrl+3", {}, "Ctrl+,", {}};
    for (int i = 0; i < 6; ++i) {
        if (i == 3)
            n->addStretch();
        auto *b = button(titles[i], icons[i], "nav");
        b->setCheckable(true);
        b->setObjectName("nav_" + objectNames[i]);
        b->setProperty("fullText", titles[i]);
        b->setToolTip(keys[i].isEmpty() ? titles[i] : QStringLiteral("%1 (%2)").arg(titles[i], keys[i]));
        mNavigation[i] = b;
        n->addWidget(b);
        connect(b, &QPushButton::clicked, this, [this, i] { changePage(i); });
    }
    layout->addWidget(nav);
    mStack = new QStackedWidget;
    mStack->setObjectName("pageStack");
    layout->addWidget(mStack, 1);
    auto *transport = new RestylePanel(root, "transport");
    transport->setObjectName("transport");
    transport->setFixedHeight(66);
    auto *t = new QHBoxLayout(transport);
    t->setContentsMargins(22, 9, 22, 9);
    t->setSpacing(10);
    auto *v = iconButton("volume", "Сведения о MediaBoxPlayer");
    t->addWidget(v);
    connect(v, &QPushButton::clicked, this, &MainWindow::showPlayerControls);
    auto *status = new QVBoxLayout;
    status->setSpacing(4);
    mPlayerState = label("Состояние плеера не подтверждено", 11);
    mPlayerState->setObjectName("playerState");
    mPlayerState->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    status->addWidget(mPlayerState);
    mPlayerDetail = label("MediaBoxPlayer · данные о текущем файле недоступны", 10);
    mPlayerDetail->setObjectName("playerTrack");
    mPlayerDetail->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    status->addWidget(mPlayerDetail);
    t->addLayout(status, 1);
    mPlay = button({}, "play", "play");
    mPlay->setFixedSize(34, 34);
    mPlay->setObjectName("playerPlayButton");
    mPlay->setToolTip("Продолжить аудиоочередь MediaBoxPlayer");
    mPlay->setAccessibleName(mPlay->toolTip());
    t->addWidget(mPlay);
    mStop = button({}, "stop", "stop");
    mStop->setFixedSize(30, 30);
    mStop->setObjectName("playerStopButton");
    mStop->setToolTip("Остановить аудио MediaBoxPlayer");
    mStop->setAccessibleName(mStop->toolTip());
    t->addWidget(mStop);
    connect(mPlay, &QPushButton::clicked, this, [this] {
        if (!playerAvailable())
            return;
        mMediaController->play();
    });
    connect(mStop, &QPushButton::clicked, this, [this] {
        if (!playerAvailable())
            return;
        mMediaController->stop();
    });
    // Segmented "Вручную | По расписанию": checked state is the mode the player
    // confirmed for the current page, a click requests that mode.
    auto *modes = new RestylePanel(transport, "inset");
    modes->setObjectName("playbackModes");
    modes->setFixedHeight(32);
    auto *m = new QHBoxLayout(modes);
    m->setContentsMargins(3, 3, 3, 3);
    m->setSpacing(3);
    auto *modeGroup = new QButtonGroup(this);
    modeGroup->setExclusive(false);
    for (auto **mode : {&mManualMode, &mScheduleMode}) {
        const bool schedule = mode == &mScheduleMode;
        *mode = new QPushButton(schedule ? "По расписанию" : "Вручную", modes);
        (*mode)->setObjectName(schedule ? "scheduleModeButton" : "manualModeButton");
        (*mode)->setCheckable(true);
        (*mode)->setFixedHeight(26);
        Restyle::button(*mode, "day");
        (*mode)->setFont(Restyle::font(10));
        (*mode)->setAccessibleName((*mode)->text());
        modeGroup->addButton(*mode);
        m->addWidget(*mode);
    }
    t->addWidget(modes);
    connect(mManualMode, &QPushButton::clicked, this, [this] {
        playSelectedChannel(mPage == PAGE_VIDEO ? PAGE_VIDEO : PAGE_MUSIC);
        updateModeButtons();
    });
    connect(mScheduleMode, &QPushButton::clicked, this, [this] {
        startScheduledPlayback(mPage == PAGE_VIDEO ? PAGE_VIDEO : PAGE_MUSIC);
        updateModeButtons();
    });
    mOperationState = label({}, 10);
    mOperationState->setMinimumWidth(80);
    mOperationState->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    t->addWidget(mOperationState, 1);
    mOpenSettings = button("Открыть настройки", "settings", "quiet");
    mOpenSettings->setObjectName("openSettingsButton");
    mOpenSettings->setToolTip("Подключение к MediaBoxPlayer настраивается в разделе «Настройки»");
    mOpenSettings->setFixedHeight(29);
    mOpenSettings->setFont(Restyle::font(10, QFont::DemiBold));
    t->addWidget(mOpenSettings);
    connect(mOpenSettings, &QPushButton::clicked, this, [this] { changePage(4); });
    auto *refresh = button("Обновить состояние", "refresh", "quiet");
    auto *show = button("Аудиоплеер", "eye", "quiet");
    refresh->setObjectName("refreshPlayerButton");
    show->setObjectName("showPlayerButton");
    refresh->setFixedHeight(29);
    show->setFixedHeight(29);
    refresh->setFont(Restyle::font(10, QFont::DemiBold));
    show->setFont(refresh->font());
    t->addWidget(refresh);
    t->addWidget(show);
    connect(refresh, &QPushButton::clicked, this, [this] {
        if (!playerAvailable())
            return;
        mMediaController->refreshPlayer();
    });
    connect(show, &QPushButton::clicked, this, &MainWindow::showPlayerControls);
    if (!qApp->property("restylePreviewStation").toString().isEmpty())
        for (auto *b : {mPlay, mStop, refresh, show, mManualMode, mScheduleMode}) {
            b->setEnabled(false);
            b->setToolTip("Изолированный просмотр: управление плеером отключено");
        }
    layout->addWidget(transport);
}

QWidget *MainWindow::buildMediaPage(int page) {
    auto &p = mPages[page];
    const bool ads = page == 2;
    const QString suffix = page == 0 ? "" : page == 1 ? "_video" : "_advert";
    auto *scroll = new QScrollArea;
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    p.root = new QWidget;
    p.root->setObjectName("workspace" + suffix);
    p.root->setMinimumSize(1000, 600);
    scroll->setWidget(p.root);
    auto *l = column(p.root, 0, 0);
    l->setContentsMargins(22, 16, 22, 0);
    auto *heading = new QWidget;
    heading->setFixedHeight(52);
    auto *h = new QHBoxLayout(heading);
    h->setContentsMargins(0, 0, 0, 0);
    h->setSpacing(8);
    auto *headingText = new QVBoxLayout;
    headingText->setSpacing(3);
    auto *title = label(ads ? "Реклама" : page == 0 ? "Музыка" : "Видео", 28, QFont::DemiBold);
    title->setFont(Restyle::font(28, QFont::DemiBold, -.75));
    headingText->addWidget(title);
    p.subtitle = label(ads ? "Медиатека и правила выхода рекламных файлов"
                           : "Каналы · расписание, условия выхода и содержимое",
                       11);
    headingText->addWidget(p.subtitle);
    h->addLayout(headingText);
    h->addStretch();
    if (page == PAGE_VIDEO) {
        auto *screens = button("Видеоэкраны", "video");
        screens->setObjectName("videoScreensButton");
        screens->setToolTip("Экраны MediaBoxVPlayer, мониторы и отдельные плейлисты");
        h->addWidget(screens);
        connect(screens, &QPushButton::clicked, this, &MainWindow::showVideoControls);
    }
    if (!ads) {
        auto *project = button("Настройки расписания", "settings");
        project->setObjectName(page == PAGE_MUSIC ? "scheduleProjectButton" : "videoScheduleProjectButton");
        project->setToolTip("Чередование плейлистов, календари и дополнительные правила расписания");
        h->addWidget(project);
        connect(project, &QPushButton::clicked, this, [this, page] { editScheduleProject(page); });
    }
    auto *all = button("Все параметры", "grid");
    all->setObjectName("allSchedulesButton" + suffix);
    all->setToolTip(ads ? "Все рекламные правила одной таблицей" : "Все каналы и их условия одной таблицей");
    h->addWidget(all);
    connect(all, &QPushButton::clicked, this, &MainWindow::showAllSchedules);
    auto *create = button(ads ? "Добавить правило" : "Создать канал", "plus", "primary");
    create->setObjectName(ads ? "addAdvertButton" : "createChannelButton" + suffix);
    h->addWidget(create);
    connect(create, &QPushButton::clicked, this,
            ads ? &MainWindow::slot_addAdvert : &MainWindow::slot_addChannel);
    if (ads) {
        p.addAdvert = create;
    } else {
        p.createChannel = create;
        create->setToolTip("Создать канал (Ctrl+N)");
    }
    l->addWidget(heading);
    l->addSpacing(12);
    // One date picker per page: the schedule panel. Adverts use the current time.
    auto *summary = new RestylePanel(p.root);
    summary->setObjectName("planSummary" + suffix);
    summary->setFixedHeight(56);
    summary->setToolTip(ads ? "Расчёт плана на текущее время по календарным условиям. "
                              "Фактическое воспроизведение подтверждает плеер."
                            : "Расчёт плана на дату и время, выбранные в расписании. "
                              "Фактическое воспроизведение подтверждает плеер.");
    auto *sum = new QHBoxLayout(summary);
    sum->setContentsMargins(14, 8, 14, 8);
    sum->setSpacing(18);
    const QStringList captions = {"По расписанию", "Следующая смена канала", "Следующий точный выход рекламы"};
    QLabel **fields[] = {&p.planNow, &p.planNext, &p.planAd};
    // The advert page has no channels to plan: only the advert column applies.
    for (int i = ads ? 2 : 0; i < 3; ++i) {
        auto *col = new QVBoxLayout;
        col->setSpacing(3);
        col->addWidget(label(captions[i], 10));
        *fields[i] = label("Нет данных", 12, QFont::DemiBold);
        (*fields[i])->setMinimumWidth(20);
        (*fields[i])->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        col->addWidget(*fields[i]);
        sum->addLayout(col, 1);
    }
    l->addWidget(summary);
    l->addSpacing(10);
    // The operator sizes the schedule against the library; all pages share it.
    p.splitter = new QSplitter(Qt::Vertical);
    p.splitter->setObjectName("pageSplitter" + suffix);
    p.splitter->setChildrenCollapsible(false);
    p.splitter->setHandleWidth(12);
    auto *top = new QWidget;
    top->setObjectName("topRow" + suffix);
    top->setMinimumHeight(250);
    auto *topLayout = new QHBoxLayout(top);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->setSpacing(14);
    if (!ads) {
        auto *left = new QWidget;
        left->setObjectName("channelsSidebar" + suffix);
        left->setFixedWidth(205);
        auto *leftLayout = column(left, 0, 0);
        auto *channelPanel = new RestylePanel(left);
        channelPanel->setObjectName("channelPanel" + suffix);
        auto *cl = column(channelPanel, 8, 5);
        auto *ch = new QHBoxLayout;
        ch->setContentsMargins(3, 2, 3, 0);
        ch->setSpacing(2);
        ch->addWidget(label("Каналы", 13, QFont::DemiBold));
        ch->addStretch();
        p.playChannel = iconButton("play", "Играть канал");
        p.playChannel->setObjectName(page == PAGE_MUSIC ? "playChannelButton" : "playVideoChannelButton");
        p.playChannel->setToolTip("Играть канал целиком по кругу, без расписания");
        ch->addWidget(p.playChannel);
        connect(p.playChannel, &QPushButton::clicked, this, [this, page] { playSelectedChannel(page); });
        p.editChannel = iconButton("edit", "Редактировать канал");
        p.editChannel->setObjectName("editChannelButton" + suffix);
        p.editChannel->setToolTip("Редактировать канал (F2)");
        ch->addWidget(p.editChannel);
        connect(p.editChannel, &QPushButton::clicked, this, &MainWindow::openChannelEditor);
        p.deleteChannel = iconButton("trash", "Удалить канал");
        p.deleteChannel->setObjectName("deleteChannelButton" + suffix);
        p.deleteChannel->setToolTip("Удалить канал (Del)");
        ch->addWidget(p.deleteChannel);
        connect(p.deleteChannel, &QPushButton::clicked, this, &MainWindow::slot_deleteChannel);
        cl->addLayout(ch);
        p.channels = new QTableView;
        configureTable(p.channels);
        p.channels->setObjectName("channelList" + suffix);
        p.channels->horizontalHeader()->hide();
        p.channels->setSelectionMode(QAbstractItemView::SingleSelection);
        p.channels->setMouseTracking(true);
        p.channels->setModel(mChannelModels[page]);
        for (int i = 1; i < 7; ++i)
            p.channels->hideColumn(i);
        p.channels->setItemDelegate(new ChannelDelegate(p.channels));
        p.channels->verticalHeader()->setDefaultSectionSize(65);
        p.channels->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        connect(p.channels->selectionModel(), &QItemSelectionModel::currentRowChanged, this,
                [this, page](const QModelIndex &i) { selectChannel(page, i.row()); });
        connect(p.channels, &QTableView::doubleClicked, this, &MainWindow::openChannelEditor);
        p.channels->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(p.channels, &QWidget::customContextMenuRequested, this, [this, page](const QPoint &position) {
            auto &pageUi = mPages[page];
            const QModelIndex index = pageUi.channels->indexAt(position);
            if (index.isValid())
                pageUi.channels->selectRow(index.row());
            QMenu menu(this);
            if (index.isValid()) {
                auto *play = menu.addAction(Restyle::icon("play"), "Играть канал");
                play->setEnabled(pageUi.playChannel->isEnabled());
                connect(play, &QAction::triggered, this, [this, page] { playSelectedChannel(page); });
                auto *edit = menu.addAction(Restyle::icon("edit"), "Редактировать…\tF2");
                edit->setEnabled(pageUi.editChannel->isEnabled());
                connect(edit, &QAction::triggered, this, &MainWindow::openChannelEditor);
                auto *remove = menu.addAction(Restyle::icon("trash"), "Удалить канал…\tDel");
                remove->setEnabled(pageUi.deleteChannel->isEnabled());
                connect(remove, &QAction::triggered, this, &MainWindow::slot_deleteChannel);
                menu.addSeparator();
            }
            auto *create = menu.addAction(Restyle::icon("plus"), "Создать канал…\tCtrl+N");
            create->setEnabled(!mMediaImport);
            connect(create, &QAction::triggered, this, &MainWindow::slot_addChannel);
            menu.exec(pageUi.channels->viewport()->mapToGlobal(position));
        });
        cl->addWidget(p.channels, 1);
        leftLayout->addWidget(channelPanel, 1);
        topLayout->addWidget(left);
        p.schedule = new SchedulePreviewWidget;
        p.schedule->setObjectName("schedulePanel" + suffix);
        p.schedule->setModels(mChannelModels[page], mAdvertModel);
        topLayout->addWidget(p.schedule, 1);
        connect(p.schedule, &SchedulePreviewWidget::selectedRowChanged, this, [this, page](int row) {
            if (row >= 0)
                mPages[page].channels->selectRow(row);
        });
        connect(p.schedule, &SchedulePreviewWidget::editRequested, this, [this, page](int row) {
            mPages[page].channels->selectRow(row);
            openChannelEditor();
        });
        connect(p.schedule, &SchedulePreviewWidget::snapshotChanged, this,
                [this, page] { updateSummary(page); });
    } else {
        // The rule table is the only list of rules: no duplicate sidebar card.
        auto *rules = new RestylePanel(p.root);
        rules->setObjectName("advertRulesPanel");
        auto *rl = column(rules, 16, 10);
        auto *rh = new QHBoxLayout;
        rh->addWidget(label("Правила выхода рекламы", 14, QFont::DemiBold));
        rh->addStretch();
        p.editAdvert = button("Редактировать правило", "edit");
        p.editAdvert->setToolTip("Редактировать выбранное правило (F2)");
        p.deleteAdvert = iconButton("trash", "Удалить выбранные правила");
        p.deleteAdvert->setToolTip("Удалить выбранные правила (Del)");
        rh->addWidget(p.editAdvert);
        rh->addWidget(p.deleteAdvert);
        connect(p.editAdvert, &QPushButton::clicked, this, &MainWindow::openAdvertEditor);
        connect(p.deleteAdvert, &QPushButton::clicked, this, &MainWindow::slot_deleteAdvert);
        rl->addLayout(rh);
        auto *note = label("Точные минуты — расчёт выхода. Частота не задаёт гарантированную минуту.", 11);
        note->setWordWrap(true);
        rl->addWidget(note);
        p.adverts = new QTableView;
        configureTable(p.adverts);
        p.adverts->setObjectName("advertTable");
        auto *advertDisplay = new RuleEditors::RuleTableModel(RuleEditors::RuleTableModel::Adverts, p.adverts);
        advertDisplay->setSourceModel(mAdvertModel);
        p.adverts->setModel(advertDisplay);
        p.adverts->setSelectionMode(QAbstractItemView::ExtendedSelection);
        p.adverts->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        p.adverts->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        rl->addWidget(p.adverts, 1);
        connect(p.adverts, &QTableView::doubleClicked, this, &MainWindow::openAdvertEditor);
        connect(p.adverts->selectionModel(), &QItemSelectionModel::selectionChanged, this,
                [this] { updatePage(2); });
        topLayout->addWidget(rules, 1);
    }
    p.splitter->addWidget(top);
    auto *bottom = new QWidget;
    bottom->setObjectName("libraryRow" + suffix);
    bottom->setMinimumHeight(190);
    auto *bottomLayout = new QHBoxLayout(bottom);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    bottomLayout->setSpacing(14);
    auto *library = new RestylePanel(p.root);
    library->setObjectName("libraryPanel" + suffix);
    auto *ll = column(library, 14, 8);
    ll->setContentsMargins(14, 13, 14, 0);
    auto *lh = new QHBoxLayout;
    lh->setSpacing(6);
    lh->addWidget(label("Медиатека", 13, QFont::DemiBold));
    p.fileCount = label("0 файлов", 9);
    lh->addWidget(p.fileCount);
    lh->addStretch();
    p.addFiles = button("Добавить", "plus", "quiet");
    p.addFiles->setObjectName("addFilesButton" + suffix);
    p.addFiles->setToolTip("Добавить файлы (Ctrl+O) или целую папку. Можно перетащить файлы в таблицу.");
    auto *addMenu = new QMenu(p.addFiles);
    p.addFilesAction = addMenu->addAction(Restyle::icon("note"), "Файлы…\tCtrl+O");
    p.addFilesAction->setObjectName("addFilesAction" + suffix);
    connect(p.addFilesAction, &QAction::triggered, this, &MainWindow::slot_addMediaFiles);
    auto *addFolderAction = addMenu->addAction(Restyle::icon("folder"), "Все файлы из папки…");
    addFolderAction->setObjectName("addFolderAction" + suffix);
    connect(addFolderAction, &QAction::triggered, this, [this] {
        if (mMediaImport || !mediaManager(mPage))
            return;
        QString path = QFileDialog::getExistingDirectory(this, "Добавить все файлы из папки");
        if (!path.isEmpty())
            copyFiles({path});
    });
    p.addFiles->setMenu(addMenu);
    lh->addWidget(p.addFiles);
    if (!ads) {
        p.openFolder = button("Открыть папку", "folder", "quiet");
        p.openFolder->setObjectName("openMediaFolderButton" + suffix);
        p.openFolder->setToolTip("Показать файлы выбранного канала в Проводнике");
        lh->addWidget(p.openFolder);
        connect(p.openFolder, &QPushButton::clicked, this, [this, page] {
            auto *media = mediaManager(page);
            if (!media) return;
            const QString directory = media->getDirMediaFiles().absolutePath();
            if (!QDir(directory).exists()) {
                showError("Папка выбранного канала не найдена: " + directory);
                return;
            }
            if (!QDesktopServices::openUrl(QUrl::fromLocalFile(directory)))
                showError("Не удалось открыть папку: " + directory);
        });
    }
    p.search = new QLineEdit;
    p.search->setObjectName("searchEdit" + suffix);
    p.search->setPlaceholderText("Найти в медиатеке (Ctrl+F)");
    p.search->setFixedHeight(32);
    p.search->setMinimumWidth(120);
    p.search->setMaximumWidth(425);
    p.search->addAction(Restyle::icon("search"), QLineEdit::LeadingPosition);
    p.search->setClearButtonEnabled(true);
    lh->addWidget(p.search, 1);
    p.deleteFiles = iconButton("trash", "Удалить выбранные файлы");
    p.deleteFiles->setObjectName("deleteFilesButton" + suffix);
    p.deleteFiles->setToolTip("Удалить выбранные файлы (Del)");
    lh->addWidget(p.deleteFiles);
    connect(p.deleteFiles, &QPushButton::clicked, this, &MainWindow::slot_removeMediaFiles);
    ll->addLayout(lh);
    p.source = new MediaModel(ads ? mMediaAdvertManager.get() : nullptr,
                              ads         ? ADVERT
                              : page == 0 ? MUSIC
                                          : VIDEO,
                              this);
    p.proxy = new QSortFilterProxyModel(this);
    p.proxy->setSourceModel(p.source);
    p.proxy->setFilterKeyColumn(-1);
    p.proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    p.proxy->setSortCaseSensitivity(Qt::CaseInsensitive);
    p.proxy->setSortRole(MediaModel::SortRole);
    if (ads)
        mMediaAdvertManager->setMediaModel(p.source);
    p.files = new QTreeView;
    p.files->setObjectName("mediaTable" + suffix);
    p.files->setModel(p.proxy);
    p.files->setFrameShape(QFrame::NoFrame);
    p.files->setRootIsDecorated(false);
    p.files->setItemsExpandable(false);
    p.files->setUniformRowHeights(true);
    p.files->setSelectionMode(QAbstractItemView::ExtendedSelection);
    p.files->setSelectionBehavior(QAbstractItemView::SelectRows);
    p.files->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auto *mediaDelegate = new MediaLibraryDelegate(p.files);
    p.files->setItemDelegate(mediaDelegate);
    connect(mediaDelegate, &MediaLibraryDelegate::fileActionsRequested, this,
            [this, page](const QModelIndex &index, const QPoint &position) {
                auto &pageUi = mPages[page];
                if (!index.isValid())
                    return;
                pageUi.files->selectionModel()->setCurrentIndex(index.siblingAtColumn(0),
                                                                QItemSelectionModel::ClearAndSelect |
                                                                    QItemSelectionModel::Rows);
                QMenu menu(this);
                auto *info = menu.addAction(Restyle::icon("info"), "Все сведения");
                connect(info, &QAction::triggered, this, &MainWindow::showFileInfo);
                if (page != 1) {
                    auto *preview = menu.addAction(Restyle::icon("play"), "Воспроизвести на плеере…");
                    preview->setEnabled(playerAvailable());
                    connect(preview, &QAction::triggered, this, [this, index] { slot_playTrack(index); });
                } else {
                    auto *playlist = menu.addAction(Restyle::icon("video"), "В плейлист видеоэкрана…");
                    connect(playlist, &QAction::triggered, this, &MainWindow::addSelectedVideosToPlaylist);
                }
                menu.addSeparator();
                auto *remove = menu.addAction(Restyle::icon("trash"), "Удалить файл\tDel");
                remove->setEnabled(page != 2 || advertWritable());
                connect(remove, &QAction::triggered, this, &MainWindow::slot_removeMediaFiles);
                menu.exec(position);
            });
    p.files->setSortingEnabled(true);
    p.files->sortByColumn(0, Qt::AscendingOrder);
    p.files->header()->setFont(Restyle::font(10));
    p.files->header()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    p.files->header()->setStretchLastSection(false);
    p.files->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    for (int i = 1; i <= 3; ++i)
        p.files->hideColumn(i);
    p.files->header()->moveSection(p.files->header()->visualIndex(5), p.files->header()->visualIndex(4));
    p.files->setColumnWidth(5, 150);
    p.files->setColumnWidth(4, 190);
    p.files->setAcceptDrops(true);
    p.files->setDropIndicatorShown(true);
    p.files->setDragDropMode(QAbstractItemView::DropOnly);
    p.files->setDefaultDropAction(Qt::CopyAction);
    p.files->setMinimumHeight(145);
    ll->addWidget(p.files, 1);
    p.libraryEmpty = label("Медиатека пуста. Добавьте файлы или перетащите их в таблицу.", 11);
    p.libraryEmpty->setWordWrap(true);
    ll->addWidget(p.libraryEmpty);
    connect(p.search, &QLineEdit::textChanged, this, [this, page](const QString &text) {
        auto &pageUi = mPages[page];
        pageUi.proxy->setFilterFixedString(text);
        pageUi.libraryEmpty->setText(text.isEmpty()
                                         ? "Медиатека пуста. Добавьте файлы или перетащите их в таблицу."
                                         : "Ничего не найдено. Попробуйте другое название.");
        pageUi.libraryEmpty->setVisible(pageUi.proxy->rowCount() == 0);
        updateFileInfo(page);
    });
    connect(p.proxy, &QAbstractItemModel::rowsRemoved, this, [this, page] { updateFileInfo(page); });
    connect(p.files->selectionModel(), &QItemSelectionModel::selectionChanged, this,
            [this, page] { updateFileInfo(page); });
    connect(p.files->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this, page] { updateFileInfo(page); });
    if (page != 1)
        connect(p.files, &QTreeView::doubleClicked, this, &MainWindow::slot_playTrack);
    connect(p.source, &QAbstractItemModel::modelReset, this, [this, page] { updatePage(page); });
    bottomLayout->addWidget(library, 1);
    auto *inspector = new RestylePanel(p.root);
    inspector->setObjectName("fileInspector" + suffix);
    inspector->setFixedWidth(275);
    auto *inspectorLayout = column(inspector, 1, 0);
    auto *inspectorScroll = new QScrollArea;
    inspectorScroll->setWidgetResizable(true);
    inspectorScroll->setFrameShape(QFrame::NoFrame);
    inspectorScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *inspectorContent = new QWidget;
    inspectorScroll->setWidget(inspectorContent);
    inspectorContent->setAutoFillBackground(false);
    inspectorScroll->viewport()->setAutoFillBackground(false);
    inspectorScroll->setAutoFillBackground(false);
    inspectorLayout->addWidget(inspectorScroll);
    auto *il = column(inspectorContent, 14, 9);
    auto *inspectorHeading = label("Выбранный файл", 12, QFont::DemiBold);
    inspectorHeading->setFixedHeight(24);
    il->addWidget(inspectorHeading);
    // Actions sit under the heading so a short library row never clips them.
    auto *actions = new QHBoxLayout;
    actions->setSpacing(5);
    il->addLayout(actions);
    auto *separator = new QFrame;
    separator->setFrameShape(QFrame::HLine);
    separator->setFixedHeight(1);
    il->addWidget(separator);
    p.fileTitle = label("Файл не выбран", 16, QFont::DemiBold);
    p.fileTitle->setWordWrap(true);
    il->addWidget(p.fileTitle);
    p.fileArtist = label("Выберите файл в медиатеке", 11);
    p.fileArtist->setWordWrap(true);
    il->addWidget(p.fileArtist);
    auto *metadata = new QGridLayout;
    metadata->setContentsMargins(0, 8, 0, 0);
    metadata->setHorizontalSpacing(16);
    metadata->setVerticalSpacing(7);
    const QStringList metadataNames = {"Альбом", "Жанр", "Год", "Длительность"};
    for (int i = 0; i < 4; ++i) {
        metadata->addWidget(label(metadataNames[i], 9), i / 2 * 2, i % 2);
        p.fileFields[i] = label("—", 11);
        p.fileFields[i]->setWordWrap(true);
        metadata->addWidget(p.fileFields[i], i / 2 * 2 + 1, i % 2);
    }
    metadata->setColumnStretch(0, 1);
    metadata->setColumnStretch(1, 1);
    il->addLayout(metadata);
    il->addStretch();
    p.preview = button(page == PAGE_VIDEO ? "В плейлист…" : "На плеере…", "play", "quiet");
    p.preview->setToolTip(page == PAGE_VIDEO ? "Добавить выбранные файлы в плейлист выбранного видеоэкрана"
                                           : "Заменить очередь выбранным файлом и воспроизвести");
    p.preview->setFont(Restyle::font(10, QFont::DemiBold));
    p.fileInfo = button("Все сведения", "info", "quiet");
    p.fileInfo->setObjectName("fileInfoButton" + suffix);
    p.fileInfo->setFont(Restyle::font(10, QFont::DemiBold));
    actions->addWidget(p.preview);
    actions->addWidget(p.fileInfo);
    connect(p.preview, &QPushButton::clicked, this,
            [this, page] {
                if (page == PAGE_VIDEO)
                    addSelectedVideosToPlaylist();
                else
                    slot_playTrack(mPages[page].files->currentIndex());
            });
    connect(p.fileInfo, &QPushButton::clicked, this, &MainWindow::showFileInfo);
    bottomLayout->addWidget(inspector);
    p.splitter->addWidget(bottom);
    p.splitter->setStretchFactor(0, 0);
    p.splitter->setStretchFactor(1, 1);
    p.splitter->setSizes({349, 400});
    connect(p.splitter, &QSplitter::splitterMoved, this, [this, page] {
        const QByteArray state = mPages[page].splitter->saveState();
        for (auto &other : mPages)
            if (other.splitter && other.splitter != mPages[page].splitter) {
                const QSignalBlocker blocker(other.splitter);
                other.splitter->restoreState(state);
            }
    });
    l->addWidget(p.splitter, 1);
    updateSummary(page);
    return scroll;
}

void MainWindow::changePage(int page) {
    if (page < 0 || page >= 6)
        return;
    if (page < 3)
        mPage = page;
    mStack->setCurrentIndex(page);
    for (int i = 0; i < 6; ++i)
        mNavigation[i]->setChecked(i == page);
    if (page < 3)
        updatePage(page);
}
void MainWindow::slotChangeChannel(const QModelIndex &i, const QModelIndex &) {
    if (mPage < 2 && i.isValid())
        selectChannel(mPage, i.row());
}
void MainWindow::selectChannel(int page, int row) {
    if (page < 0 || page >= 2 || row < 0 || row >= mChannelManagers[page]->channelCount())
        return;
    if (mPages[page].channels->currentIndex().row() != row) {
        mPages[page].channels->selectRow(row);
        return;
    }
    auto *manager = mChannelManagers[page];
    manager->setCurrentChannel(row);
    auto &media = manager->channel(row).mediaManager();
    // The storage layer binds rule identities without reading media. Load on
    // selection, except the target whose complete snapshot is being imported.
    if (!mMediaImport || media.getDirMediaFiles().absolutePath() != mImportTargetDirectory)
        media.collectMediaFiles();
    mPages[page].source->setMediaManager(&media);
    mPages[page].schedule->setSelectedRow(row);
    updatePage(page);
    if (page == PAGE_VIDEO) updateVideoPlaybackContext();
}
MediaManager *MainWindow::mediaManager(int page) const {
    if (page == 2)
        return mMediaAdvertManager.get();
    if (page < 0 || page > 1 || !mChannelManagers[page]->channelCount())
        return nullptr;
    auto *manager = mChannelManagers[page];
    if (manager->currentChannelNum() < 0 || manager->currentChannelNum() >= manager->channelCount())
        return nullptr;
    return &manager->currentChannel().mediaManager();
}
bool MainWindow::advertWritable() const {
    return StationManager::Instance().type() != STATION_NETWORK;
}
void MainWindow::updatePage(int page) {
    auto &p = mPages[page];
    if (!p.files)
        return;
    auto *manager = mediaManager(page);
    const int count = manager ? manager->mediaCount() : 0;
    p.fileCount->setText(QStringLiteral("%1 файлов").arg(count));
    p.libraryEmpty->setVisible(count == 0);
    p.addFiles->setEnabled(!mMediaImport && manager && (page != 2 || advertWritable()));
    p.files->setEnabled(manager);
    if (page < 2) {
        p.createChannel->setEnabled(!mMediaImport);
        int row = mChannelManagers[page]->currentChannelNum();
        bool selected = row >= 0 && row < mChannelModels[page]->rowCount();
        p.subtitle->setText(QStringLiteral("%1 каналов · расписание, условия выхода и содержимое")
                                .arg(mChannelModels[page]->rowCount()));
        p.editChannel->setEnabled(!mMediaImport && selected);
        p.deleteChannel->setEnabled(!mMediaImport && selected);
    } else {
        bool selected = p.adverts->currentIndex().isValid();
        bool allowed = advertWritable();
        p.editAdvert->setEnabled(allowed && selected);
        p.deleteAdvert->setEnabled(allowed && p.adverts->selectionModel()->hasSelection());
        p.subtitle->setText(QStringLiteral("%1 правил · %2 файлов · %3")
                                .arg(mAdvertModel->rowCount())
                                .arg(count)
                                .arg(allowed ? "точные минуты или частота" : "сетевая реклама, только просмотр"));
    }
    updateFileInfo(page);
    updateSummary(page);
    updatePlaybackActions();
}
void MainWindow::updateSummary(int page) {
    auto &p = mPages[page];
    if (!p.planAd)
        return;
    const SchedulePreview::Snapshot snapshot = p.schedule
        ? p.schedule->snapshot()
        : SchedulePreview::evaluate(nullptr, mAdvertModel, QDateTime::currentDateTime());
    if (page < 2 && p.channels) {
        QStringList statuses, counts;
        QVariantList active;
        for (int row = 0; row < mChannelManagers[page]->channelCount(); ++row) {
            const auto planned = std::find_if(snapshot.channels.cbegin(), snapshot.channels.cend(),
                [row](const auto &channel) { return channel.sourceRow == row; });
            statuses << (planned != snapshot.channels.cend() ? planned->status
                : snapshot.hasUnresolvedRules ? QStringLiteral("Ошибка расписания")
                : QStringLiteral("Правила проекта"));
            active << (planned != snapshot.channels.cend() && planned->active);
            counts << QString::number(channelFiles(mChannelManagers[page]->channel(row).mediaManager()).size());
        }
        p.channels->setProperty("channelStatuses", statuses);
        p.channels->setProperty("channelFileCounts", counts);
        p.channels->setProperty("channelActive", active);
        p.channels->viewport()->update();
    }
    if (p.planNow) {
        p.planNow->setText(snapshot.currentSummary);
        p.planNow->setToolTip(snapshot.currentSummary);
        p.planNext->setText(snapshot.nextChannelSummary);
        p.planNext->setToolTip(snapshot.nextChannelSummary);
    }
    p.planAd->setText(snapshot.nextAdvertSummary);
    p.planAd->setToolTip(snapshot.nextAdvertSummary);
}
void MainWindow::updateFileInfo(int page) {
    auto &p = mPages[page];
    if (!p.fileTitle)
        return;
    auto *manager = mediaManager(page);
    auto index = p.proxy->mapToSource(p.files->currentIndex());
    bool selected = manager && index.isValid() && p.files->selectionModel()->hasSelection() &&
                    index.row() < manager->count();
    p.deleteFiles->setEnabled(!mMediaImport && p.files->selectionModel()->hasSelection() && (page != 2 || advertWritable()));
    p.preview->setEnabled(selected && (page == PAGE_VIDEO || playerAvailable()));
    p.fileInfo->setEnabled(selected);
    if (page == 2) {
        // The rule editor offers the whole advert library; a selected file is only preselected.
        const bool files = manager && manager->mediaCount() > 0;
        p.addAdvert->setEnabled(!mMediaImport && advertWritable() && files);
        p.addAdvert->setToolTip(!advertWritable() ? "Рекламой сетевой станции управляют централизованно"
                                : files ? "Новое правило выхода для ролика из рекламной медиатеки (Ctrl+N)"
                                        : "Сначала добавьте ролики в рекламную медиатеку");
    }
    if (!selected) {
        p.fileTitle->setText("Файл не выбран");
        p.fileArtist->setText("Выберите файл в медиатеке");
        for (auto *field : p.fileFields)
            field->setText("—");
        return;
    }
    const auto &media = manager->mediaData(index.row());
    p.fileTitle->setText(media.title().isEmpty() ? media.fileName() : media.title());
    p.fileTitle->setToolTip(media.fileName());
    p.fileArtist->setText(known(media.artist()));
    const QStringList values = {known(media.album()), known(media.genre()),
                                media.year() ? QString::number(media.year()) : "Не указан",
                                media.length() ? manager->calculateLength(media.length()) : "Неизвестно"};
    for (int i = 0; i < 4; ++i)
        p.fileFields[i]->setText(values[i]);
}
void MainWindow::showError(const QString &message) {
    QMessageBox::warning(this, "MediaBoxManager", message);
}
bool MainWindow::playerAvailable() const {
    return mMediaController && mMediaController->isReady();
}
void MainWindow::updatePlayerState() {
    const bool ready = playerAvailable();
    const PlayerStatus snapshot = mMediaController ? mMediaController->status() : PlayerStatus{};
    QString state = "Нет связи с MediaBoxPlayer";
    if (mMediaController) {
        using Connection = MediaBoxPlayerClient::ConnectionState;
        switch (mMediaController->connectionState()) {
        case Connection::Connecting: state = "Подключение к MediaBoxPlayer…"; break;
        case Connection::Synchronizing: state = "Получение состояния плеера…"; break;
        case Connection::Reconnecting: state = "Нет связи · ожидается переподключение"; break;
        case Connection::AuthenticationFailed: state = "Не принят токен плеера · проверьте настройки"; break;
        case Connection::ProtocolMismatch: state = "Несовместимая версия протокола плеера"; break;
        case Connection::Disconnected: break;
        case Connection::Ready:
            if (snapshot.state == "playing") state = "Воспроизведение";
            else if (snapshot.state == "loading") state = "Загрузка аудио…";
            else if (snapshot.state == "paused") state = "Пауза";
            else if (snapshot.state == "error") state = "Ошибка воспроизведения";
            else state = snapshot.playbackRequested ? "Переход к следующему файлу…" : "Остановлено";
            if (snapshot.muted) state += " · звук выключен";
            else state += QStringLiteral(" · %1 %").arg(snapshot.volumePercent);
            if (!snapshot.publicationId.isEmpty()) state += QStringLiteral(" · выпуск %1 принят").arg(snapshot.revision);
            break;
        }
    }
    mPlayerState->setText(state);
    mPlayerState->setToolTip(ready && !snapshot.scheduleError.isEmpty() ? snapshot.scheduleError
                           : ready && !snapshot.error.isEmpty() ? snapshot.error : state);
    QString track = snapshot.currentTrack;
    if (track.isEmpty())
        track = ready ? "Очередь пуста · выберите канал или включите воспроизведение по расписанию"
                      : "Нет данных о текущем файле";
    else if (!ready)
        track.prepend("Последние данные, связь потеряна: ");
    if (ready && !snapshot.channelName.isEmpty()) track.prepend(snapshot.channelName + " · ");
    if (ready && !snapshot.scheduleError.isEmpty()) track = snapshot.scheduleError;
    mPlayerDetail->setText(track);
    mPlayerDetail->setToolTip(track);
    const bool preview = !qApp->property("restylePreviewStation").toString().isEmpty();
    mOpenSettings->setVisible(!ready && !preview);
    mPlay->setEnabled(ready && !snapshot.queue.isEmpty() && !snapshot.playbackRequested);
    mStop->setEnabled(ready && (!snapshot.queue.isEmpty() || snapshot.state == "error" || snapshot.playbackMode == "schedule"));
    // A disabled control explains why: the reason is the tooltip, the name stays.
    if (!preview) {
        mPlay->setToolTip(!ready ? "Продолжить аудиоочередь · нет связи с плеером"
                          : snapshot.queue.isEmpty() ? "Продолжить аудиоочередь · очередь пуста"
                          : snapshot.playbackRequested ? "Продолжить аудиоочередь · уже играет"
                                                       : "Продолжить аудиоочередь MediaBoxPlayer");
        mStop->setToolTip(!ready ? "Остановить аудио · нет связи с плеером"
                          : mStop->isEnabled() ? "Остановить аудио MediaBoxPlayer"
                                               : "Остановить аудио · очередь пуста");
    }
    findChild<QPushButton *>("refreshPlayerButton")->setEnabled(ready);
    for (int page = 0; page < 3; ++page)
        updateFileInfo(page);
    updatePlaybackActions();
}
void MainWindow::slot_addMediaFiles() {
    if (mMediaImport || (mPage == 2 && !advertWritable()))
        return;
    if (!mediaManager(mPage))
        return;
    Settings s;
    QStringList formats = mPage == 0   ? s.availablelAudioFileFormats()
                          : mPage == 1 ? s.availablelVideoFileFormats()
                                       : s.availablelAllFileFormats();
    auto files = QFileDialog::getOpenFileNames(this, "Добавить медиафайлы", {},
                                               "Медиафайлы (" + formats.join(' ') + ")");
    if (!files.isEmpty())
        copyFiles(files);
}
void MainWindow::copyFiles(const QStringList &paths) {
    if (mMediaImport || paths.isEmpty() || (mPage == 2 && !advertWritable()))
        return;
    auto *manager = mediaManager(mPage);
    if (!manager)
        return;
    const int targetPage = mPage;
    const QString targetDirectory = manager->getDirMediaFiles().absolutePath();
    MediaImportRequest request;
    request.paths = paths;
    request.targetDirectory = targetDirectory;
    request.acceptedFormats = manager->importFormats();
    request.libraryFormats = manager->libraryFormats();
    request.initialSnapshot = manager->snapshot();
    if (StationManager::Instance().trial()) {
        request.maximumFiles = 10;
        request.maximumDurationSeconds = 300;
    }
    mMediaImport = new MediaImportService(this);
    mImportTargetDirectory = targetDirectory;
    auto *service = mMediaImport;
    auto *progress = new QProgressDialog("Подготовка медиатеки…", "Отменить", 0, 0, this);
    progress->setObjectName("mediaImportProgress");
    // Modal QProgressDialog::setValue processes nested events. The import uses
    // ordinary queued signals and keeps navigation/playback available instead.
    progress->setWindowModality(Qt::NonModal);
    progress->setMinimumDuration(0);
    progress->setAutoClose(false);
    progress->setAutoReset(false);
    connect(progress, &QProgressDialog::canceled, service, &MediaImportService::cancel);
    connect(service, &MediaImportService::progress, progress,
            [progress](int completed, int total, const QString &file, qint64 copied, qint64 bytes) {
        if (progress->wasCanceled())
            return;
        progress->setRange(0, total);
        progress->setValue(completed);
        const QString detail = bytes > 0
            ? QStringLiteral("\n%1 / %2 МБ").arg(copied / 1048576.0, 0, 'f', 1).arg(bytes / 1048576.0, 0, 'f', 1)
            : QString();
        progress->setLabelText((total ? QStringLiteral("Добавление файлов…\n")
                                      : QStringLiteral("Подготовка медиатеки…\n")) + file + detail);
    });
    connect(service, &MediaImportService::finished, this,
            [this, service, progress, targetPage, targetDirectory](const MediaImportResult &result) {
        // Resolve the original destination by identity, never by the current
        // selection and never through a manager pointer captured by the worker.
        MediaManager *target = nullptr;
        if (targetPage == PAGE_ADVERT) {
            if (mMediaAdvertManager->getDirMediaFiles().absolutePath() == targetDirectory)
                target = mMediaAdvertManager.get();
        } else {
            for (int row = 0; row < mChannelManagers[targetPage]->channelCount(); ++row) {
                auto &candidate = mChannelManagers[targetPage]->channel(row).mediaManager();
                if (candidate.getDirMediaFiles().absolutePath() == targetDirectory) {
                    target = &candidate;
                    break;
                }
            }
        }
        if (target)
            target->applySnapshot(result.snapshot);
        mMediaImport = nullptr;
        mImportTargetDirectory.clear();
        progress->close();
        progress->deleteLater();
        service->deleteLater();
        for (int page = 0; page < 3; ++page)
            updatePage(page);
        mScheduleUpdateTimer->start();
        mOperationState->setText(QStringLiteral("%1 · добавлено файлов: %2")
                                    .arg(result.cancelled ? "Импорт отменён" : "Импорт завершён")
                                    .arg(result.imported));
        mOperationState->setToolTip(mOperationState->text());
        if (mCloseAfterImport) {
            close();
            return;
        }
        if (!result.errors.isEmpty())
            showError("Не удалось добавить некоторые файлы:\n" + result.errors.mid(0, 15).join('\n'));
    });
    for (int page = 0; page < 3; ++page)
        updatePage(page);
    service->start(std::move(request));
    progress->show();
}
void MainWindow::slot_removeMediaFiles() {
    if (mMediaImport || (mPage == 2 && !advertWritable()))
        return;
    auto &p = mPages[mPage];
    auto *manager = mediaManager(mPage);
    if (!manager)
        return;
    QStringList names;
    for (const auto &index : p.files->selectionModel()->selectedRows()) {
        auto source = p.proxy->mapToSource(index);
        if (source.isValid())
            names << source.data().toString();
    }
    if (names.isEmpty())
        return;
    if (mPage == 2) {
        QStringList used;
        for (int r = 0; r < mAdvertModel->rowCount(); ++r) {
            QString name = mAdvertModel->index(r, 0).data().toString();
            if (names.contains(name))
                used << name;
        }
        if (!used.isEmpty()) {
            showError("Файлы используются в рекламных правилах. Сначала удалите соответствующие правила:\n" +
                      used.mid(0, 15).join('\n'));
            return;
        }
    }
    const QString more = names.size() > 8 ? QStringLiteral("\n…и ещё %1").arg(names.size() - 8) : QString();
    if (!confirmDelete(this, "Удалить файлы",
                       QStringLiteral("Переместить выбранные файлы (%1) в Корзину?\n\n%2%3\n\n"
                                      "Файлы исчезнут из медиатеки и расписания. "
                                      "Вернуть их можно из Корзины Windows.")
                           .arg(names.size())
                           .arg(names.mid(0, 8).join('\n'), more)))
        return;
    QStringList failed, untrashable;
    for (const auto &name : names)
        if (!manager->trashFile(name))
            untrashable << name;
    // Some volumes (network shares, removable media) have no recycle bin.
    if (!untrashable.isEmpty()
        && confirmDelete(this, "Корзина недоступна",
                         QStringLiteral("Корзина недоступна для файлов (%1):\n\n%2\n\n"
                                        "Удалить их безвозвратно? Восстановить их будет нельзя.")
                             .arg(untrashable.size())
                             .arg(untrashable.mid(0, 8).join('\n'))))
        for (const auto &name : std::as_const(untrashable))
            if (!manager->delFile(name))
                failed << name;
    manager->collectMediaFiles();
    updatePage(mPage);
    mScheduleUpdateTimer->start();
    if (!failed.isEmpty())
        showError("Не удалось удалить файлы:\n" + failed.join('\n'));
}
void MainWindow::slot_addChannel() {
    if (mMediaImport || mPage >= 2)
        return;
    auto *manager = mChannelManagers[mPage];
    if (StationManager::Instance().trial() && manager->channelCount()) {
        TrialMessageBox("Вы можете создать только 1 плейлист.");
        return;
    }
    QStringList names;
    for (int r = 0; r < mChannelModels[mPage]->rowCount(); ++r)
        names << mChannelModels[mPage]->index(r, 0).data().toString();
    auto values = RuleEditors::newChannel(this, names);
    if (!values)
        return;
    const int previousRow = manager->currentChannelNum();
    mPages[mPage].source->setMediaManager(nullptr);
    const QVariantList fields{values->name, values->start, values->end, values->weekdays,
                              values->days, values->months, values->volume, values->order, values->untilDayOffset};
    if (!manager->createChannel(fields)) {
        selectChannel(mPage, previousRow);
        showError(manager->lastError());
        return;
    }
    int row = manager->channelCount() - 1;
    mPages[mPage].channels->selectRow(row);
    selectChannel(mPage, row);
    updatePage(mPage);
}
void MainWindow::openChannelEditor() {
    if (mMediaImport || mPage >= 2)
        return;
    auto index = mPages[mPage].channels->currentIndex();
    if (!index.isValid())
        return;
    RuleEditors::editChannel(mChannelModels[mPage], index.row(), this, [this] { slot_deleteChannel(); });
    updatePage(mPage);
}
void MainWindow::slot_deleteChannel() {
    if (mMediaImport || mPage >= 2)
        return;
    auto *manager = mChannelManagers[mPage];
    if (!manager->channelCount() || !mPages[mPage].channels->currentIndex().isValid())
        return;
    if (!confirmDelete(this, "Удалить канал",
                       QStringLiteral("Удалить канал «%1»?\n\nКанал исчезнет из расписания, а его файлы "
                                      "будут удалены с диска безвозвратно.")
                           .arg(manager->currentChannel().channelName())))
        return;
    mPages[mPage].source->setMediaManager(nullptr);
    if (!manager->deleteCurrentChannel()) {
        showError(manager->lastError());
        selectChannel(mPage, manager->currentChannelNum());
        return;
    }
    if (manager->channelCount()) {
        int row = qBound(0, manager->currentChannelNum(), manager->channelCount() - 1);
        mPages[mPage].channels->selectRow(row);
        selectChannel(mPage, row);
    }
    updatePage(mPage);
}
void MainWindow::slot_addAdvert() {
    if (mPage != 2 || !advertWritable())
        return;
    auto &p = mPages[2];
    QStringList library, names;
    for (int row = 0; row < p.source->rowCount(); ++row)
        library << p.source->index(row, 0).data().toString();
    if (library.isEmpty())
        return;
    for (const auto &i : p.files->selectionModel()->selectedRows()) {
        auto source = p.proxy->mapToSource(i);
        if (source.isValid())
            names << source.data().toString();
    }
    // One editor per selected file; without a selection the editor offers the first clip.
    if (names.isEmpty())
        names << QString();
    for (const auto &name : names) {
        if (StationManager::Instance().trial() && mAdvertManager->count() >= 2) {
            TrialMessageBox("Вы можете добавить не больше двух рекламных правил.");
            break;
        }
        auto values = RuleEditors::newAdvert(library, name, this);
        if (!values)
            continue;
        const QVariantList fields{values->fileName, values->hours, values->minutes, values->weekdays,
                                  values->start, values->end, values->volume, values->startMode};
        if (!mAdvertManager->addAdvert(fields)) {
            showError(mAdvertManager->lastError());
            break;
        }
        int row = mAdvertManager->count() - 1;
        p.adverts->selectRow(row);
    }
    updatePage(2);
}
void MainWindow::openAdvertEditor() {
    if (mPage != 2 || !advertWritable())
        return;
    auto index = mPages[2].adverts->currentIndex();
    if (!index.isValid())
        return;
    RuleEditors::editAdvert(mAdvertModel, index.row(), this);
    updatePage(2);
}
void MainWindow::slot_deleteAdvert() {
    if (mPage != 2 || !advertWritable())
        return;
    QList<int> rows;
    for (const auto &i : mPages[2].adverts->selectionModel()->selectedRows())
        rows << i.row();
    if (rows.isEmpty())
        return;
    if (!confirmDelete(
            this, "Удалить правила",
            QStringLiteral("Удалить выбранные рекламные правила (%1)? Файлы останутся в медиатеке.")
                .arg(rows.size())))
        return;
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    for (int row : rows)
        if (!mAdvertManager->delAdvert(row)) {
            showError(mAdvertManager->lastError());
            break;
        }
    updatePage(2);
}
void MainWindow::slot_playTrack(QModelIndex index) {
    if (mPage == 1 || !index.isValid() || !playerAvailable())
        return;
    auto *manager = mediaManager(mPage);
    if (!manager)
        return;
    auto source = mPages[mPage].proxy->mapToSource(index.siblingAtColumn(0));
    if (!source.isValid())
        return;
    QString path = manager->getDirMediaFiles().filePath(source.data().toString());
    if (!QFileInfo::exists(path)) {
        showError("Выбранный файл больше недоступен.");
        return;
    }
    QMessageBox confirmation(QMessageBox::Question, "Воспроизвести на MediaBoxPlayer",
                             QStringLiteral("Заменить всю очередь этим файлом и начать воспроизведение?\n\n%1\n\n"
                                            "Путь должен быть доступен на машине плеера. "
                                            "Предыдущая очередь автоматически не восстановится.").arg(path),
                             QMessageBox::Yes | QMessageBox::Cancel, this);
    confirmation.setTextFormat(Qt::PlainText);
    confirmation.button(QMessageBox::Yes)->setText("Заменить и воспроизвести");
    confirmation.button(QMessageBox::Cancel)->setText("Отмена");
    confirmation.setDefaultButton(QMessageBox::Cancel);
    if (confirmation.exec() != QMessageBox::Yes || !playerAvailable())
        return;
    mMediaController->playTrack(path);
}
QJsonObject MainWindow::playbackSchedule(int page, QString *error) const {
    error->clear();
    auto *manager = mChannelManagers[page];
    if (!manager->scheduleLoaded() || !mAdvertManager->scheduleLoaded()) {
        *error = !manager->scheduleLoaded() ? manager->lastError() : mAdvertManager->lastError();
        return {};
    }
    QJsonArray channels, adverts;
    for (int row = 0; row < manager->channelCount(); ++row) {
        auto &channel = manager->channel(row);
        QJsonObject item{{"id", channel.ruleId()}, {"name", channel.channelName()},
            {"start", channel.startTime().toString("HH:mm")}, {"end", channel.endTime().toString("HH:mm")},
            {"weekdays", channel.daysOfWeek()}, {"days", channel.days()}, {"months", channel.months()},
            {"volume", channel.volume()}, {"order", channel.playbackOrder()},
            {"untilDayOffset", channel.untilDayOffset()},
            {"paths", QJsonArray::fromStringList(channelFiles(channel.mediaManager()))}};
        // Compiler metadata only; TCP sends the publication's file paths.
        item.insert("directory", channel.storageDirectory());
        channels.append(item);
    }
    const QDir advertDirectory = mMediaAdvertManager->getDirMediaFiles();
    for (int row = 0; row < mAdvertManager->count(); ++row) {
        const auto &advert = mAdvertManager->advert(row);
        QJsonArray minutes, paths;
        for (int minute : mAdvertManager->compiledMinutes(row)) minutes.append(minute);
        const QString path = advertDirectory.absoluteFilePath(advert.name());
        if (QFileInfo::exists(path)) paths.append(path);
        adverts.append(QJsonObject{{"id", mAdvertManager->ruleId(row)}, {"name", advert.name()},
            {"hours", advert.hours()}, {"weekdays", advert.days()}, {"from", advert.startDate().toString(Qt::ISODate)},
            {"until", advert.endDate().toString(Qt::ISODate)}, {"timing", advert.minuts()},
            {"compiledMinutes", minutes}, {"volume", advert.volume()}, {"paths", paths},
            {"startMode", advert.startMode()}});
    }
    return {{"channels", channels}, {"adverts", adverts}};
}

void MainWindow::updatePlaybackActions() {
    updatePlayingMedia();
    const bool preview = !qApp->property("restylePreviewStation").toString().isEmpty();
    for (int page = PAGE_MUSIC; page <= PAGE_VIDEO; ++page) {
        auto &p = mPages[page];
        if (!p.playChannel) continue;
        auto *media = mediaManager(page);
        const bool ready = page == PAGE_MUSIC ? playerAvailable()
            : mVideoController && mVideoController->isReady();
        p.playChannel->setEnabled(!preview && ready && media && media->mediaCount() > 0);
        p.openFolder->setEnabled(!preview && media && media->getDirMediaFiles().exists());
    }
    updateModeButtons();
}

void MainWindow::updateModeButtons() {
    if (!mScheduleMode || !qApp->property("restylePreviewStation").toString().isEmpty())
        return;
    const bool video = mPage == PAGE_VIDEO;
    const QString player = video ? "MediaBoxVPlayer" : "MediaBoxPlayer";
    const bool ready = video ? mVideoController && mVideoController->isReady() : playerAvailable();
    bool schedule = false;
    if (ready && video) {
        for (const auto &window : mVideoController->videoStatus().windows)
            schedule |= window.playback.playbackMode == QStringLiteral("schedule");
    } else if (ready) {
        schedule = mMediaController->status().playbackMode == QStringLiteral("schedule");
    }
    auto *media = mediaManager(video ? PAGE_VIDEO : PAGE_MUSIC);
    const bool hasFiles = media && media->mediaCount() > 0;
    mScheduleMode->setChecked(ready && schedule);
    mManualMode->setChecked(ready && !schedule);
    mScheduleMode->setEnabled(ready);
    mManualMode->setEnabled(ready && hasFiles);
    mScheduleMode->setToolTip(!ready ? QStringLiteral("Нет связи с %1").arg(player)
                              : QStringLiteral("Опубликовать расписание и включить его воспроизведение в %1").arg(player));
    mManualMode->setToolTip(!ready ? QStringLiteral("Нет связи с %1").arg(player)
                            : !hasFiles ? QStringLiteral("В выбранном канале нет файлов")
                            : QStringLiteral("Играть выбранный канал целиком по кругу в %1, без расписания").arg(player));
}

void MainWindow::updatePlayingMedia() {
    QStringList audioFiles, videoFiles;
    auto appendPlaying = [](QStringList &files, const PlayerStatus &status) {
        if (status.state == QStringLiteral("playing") && !status.currentTrack.isEmpty())
            files.append(status.currentTrack);
    };
    if (playerAvailable())
        appendPlaying(audioFiles, mMediaController->status());
    if (mVideoController && mVideoController->isReady()) {
        for (const auto &window : mVideoController->videoStatus().windows)
            appendPlaying(videoFiles, window.playback);
    }
    if (mPages[PAGE_MUSIC].source)
        mPages[PAGE_MUSIC].source->setPlayingFiles(audioFiles);
    if (mPages[PAGE_VIDEO].source)
        mPages[PAGE_VIDEO].source->setPlayingFiles(videoFiles);
    if (mPages[PAGE_ADVERT].source)
        mPages[PAGE_ADVERT].source->setPlayingFiles(audioFiles + videoFiles);
}

void MainWindow::playSelectedChannel(int page) {
    auto *media = mediaManager(page);
    if (!media) return;
    auto &channel = mChannelManagers[page]->currentChannel();
    const QStringList paths = channelFiles(*media);
    if (paths.isEmpty()) { showError("В выбранном канале нет файлов для воспроизведения."); return; }
    if (page == PAGE_VIDEO) {
        showVideoControls();
        if (auto *controls = findChild<VideoControlWidget *>()) controls->playSelectedChannel();
    } else if (playerAvailable()) {
        mMediaController->playChannel(channel.channelName(), paths, channel.volume(), channel.playbackOrder());
    }
}

void MainWindow::startScheduledPlayback(int page) {
    if (page == PAGE_VIDEO) {
        showVideoControls();
        if (auto *controls = findChild<VideoControlWidget *>()) controls->startSelectedSchedule();
        return;
    }
    if (!playerAvailable()) return;
    QString error;
    if (!publishMusicSchedule(true, &error)) showError(error);
}

void MainWindow::editScheduleProject(int page) {
    QString error;
    const auto source = playbackSchedule(page, &error);
    if (!error.isEmpty()) { showError(error); return; }
    const QString directory = page == PAGE_VIDEO ? QDir(STATIONPATH).filePath("video-schedule") : STATIONPATH;
    const QString mediaType = page == PAGE_VIDEO ? QStringLiteral("video") : QStringLiteral("audio");
    QJsonObject document;
    if (!SchedulePublication::draft(directory, STATIONMEDIA, source, &document, nullptr, &error, mediaType)) {
        showError(error); return;
    }
    ScheduledDocumentDialog dialog(document, this, mediaType);
    if (dialog.exec() != QDialog::Accepted) return;
    if (!SchedulePublication::saveDraft(directory, dialog.document(), &error)) { showError(error); return; }
    updateScheduleDocumentPreview();
    mScheduleUpdateTimer->start();
}

void MainWindow::updateScheduleDocumentPreview() {
    for (int page : {PAGE_MUSIC, PAGE_VIDEO}) {
        QString error;
        const auto source = playbackSchedule(page, &error);
        QJsonObject document;
        const QString directory = page == PAGE_VIDEO ? QDir(STATIONPATH).filePath("video-schedule") : STATIONPATH;
        const QString mediaType = page == PAGE_VIDEO ? QStringLiteral("video") : QStringLiteral("audio");
        auto &previous = mScheduleDocuments[page];
        if (!error.isEmpty()
                || !SchedulePublication::draft(directory, STATIONMEDIA, source, &document, nullptr, &error, mediaType)) {
            previous = {};
            if (mPages[page].schedule) mPages[page].schedule->setDocumentError(error);
            continue;
        }
        previous = document;
        // Every schedule uses the same compiled document and grid, regardless
        // of whether optional rules have been added in its settings.
        if (mPages[page].schedule) mPages[page].schedule->setDocument(document);
    }
}

bool MainWindow::publishMusicSchedule(bool autoplay, QString *error) {
    const auto source = playbackSchedule(PAGE_MUSIC, error);
    if (!error->isEmpty()) return false;
    QJsonObject document;
    if (!SchedulePublication::draft(STATIONPATH, STATIONMEDIA, source, &document, nullptr, error)) return false;
    SchedulePublication::Publication publication;
    if (!SchedulePublication::publish(STATIONPATH, STATIONMEDIA, document, &publication, error)) return false;
    if ((playerAvailable() && mMediaController->loadPublication(publication.activePath, publication.contentRoot, autoplay).isEmpty())
        || (autoplay && !playerAvailable()))
    {
        *error = QStringLiteral("Плеер не принял команду загрузки выпуска. Проверьте подключение и обновите плеер до текущей версии.");
        return false;
    }
    updateScheduleDocumentPreview();
    return true;
}

bool MainWindow::updateVideoPlaybackContext(QString *error) {
    QString reason;
    const auto schedule = playbackSchedule(PAGE_VIDEO, &reason);
    QJsonObject document;
    SchedulePublication::Publication publication;
    const QString directory = QDir(STATIONPATH).filePath("video-schedule");
    const bool prepared = reason.isEmpty()
        && SchedulePublication::draft(directory, STATIONMEDIA, schedule, &document, nullptr, &reason, "video")
        && SchedulePublication::publish(directory, STATIONMEDIA, document, &publication, &reason);
    mVideoScheduleActivePath = prepared ? publication.activePath : QString();
    mVideoScheduleContentRoot = prepared ? publication.contentRoot : QString();
    if (error) *error = reason;
    auto *controls = findChild<VideoControlWidget *>();
    if (!controls) return prepared;
    controls->setSchedulePublication(mVideoScheduleActivePath, mVideoScheduleContentRoot);
    if (auto *media = mediaManager(PAGE_VIDEO)) {
        auto &channel = mChannelManagers[PAGE_VIDEO]->currentChannel();
        controls->setSelectedChannel(channel.channelName(), channelFiles(*media), channel.volume(), channel.playbackOrder());
    } else {
        controls->setSelectedChannel({}, {}, 100);
    }
    return prepared;
}

void MainWindow::refreshPlaybackSchedules() {
    QString videoError;
    const bool videoPrepared = updateVideoPlaybackContext(&videoError);
    updateScheduleDocumentPreview();
    QString error;
    if (!publishMusicSchedule(false, &error)) Informer::Instance().infoEvent(error, Informer::ERROR);
    if (mVideoController && mVideoController->isReady()) {
        if (videoPrepared) {
            for (const auto &window : mVideoController->videoStatus().windows)
                mVideoController->loadPublication(window.id, mVideoScheduleActivePath, mVideoScheduleContentRoot, false);
        } else Informer::Instance().infoEvent(videoError, Informer::ERROR);
    }
}

void MainWindow::showPlayerControls() {
    if (!mMediaController)
        return;
    QDialog dialog(this);
    dialog.setObjectName("playerControlDialog");
    dialog.setWindowTitle("Управление MediaBoxPlayer");
    auto *layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(0, 0, 0, 0);
    auto *controls = new PlayerControlWidget(mMediaController, &dialog);
    layout->addWidget(controls);
    connect(controls, &PlayerControlWidget::settingsRequested, &dialog, [this, &dialog] {
        dialog.accept();
        changePage(4);
    });
    dialog.resize(QSize(780, 720).boundedTo(screen()->availableGeometry().size() - QSize(40, 60)));
    dialog.exec();
}
void MainWindow::showVideoControls() {
    auto *dialog = findChild<QDialog *>("videoControlDialog");
    if (!dialog) {
        dialog = new QDialog(this);
        dialog->setObjectName("videoControlDialog");
        dialog->setWindowTitle("Видеоэкраны · MediaBoxVPlayer");
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        auto *layout = new QVBoxLayout(dialog);
        layout->setContentsMargins(0, 0, 0, 0);
        MediaBoxVPlayerClient *client = mVideoController;
        if (!client)
            client = new MediaBoxVPlayerClient(dialog);
        auto *controls = new VideoControlWidget(dialog, client);
        layout->addWidget(controls);
        connect(controls, &VideoControlWidget::settingsRequested, dialog, [this, dialog] {
            dialog->hide();
            changePage(4);
        });
        dialog->resize(QSize(1100, 780).boundedTo(screen()->availableGeometry().size() - QSize(40, 60)));
    }
    updateVideoPlaybackContext();
    dialog->show();
    dialog->raise();
    dialog->activateWindow();
}
void MainWindow::addSelectedVideosToPlaylist() {
    auto *manager = mediaManager(PAGE_VIDEO);
    if (!manager)
        return;
    auto &page = mPages[PAGE_VIDEO];
    auto rows = page.files->selectionModel()->selectedRows();
    std::sort(rows.begin(), rows.end(), [](const QModelIndex &left, const QModelIndex &right) {
        return left.row() < right.row();
    });
    QStringList paths;
    for (const auto &index : rows) {
        const auto source = page.proxy->mapToSource(index);
        if (source.isValid())
            paths.append(manager->getDirMediaFiles().absoluteFilePath(source.data(MediaModel::FileNameRole).toString()));
    }
    if (paths.isEmpty())
        return;
    showVideoControls();
    auto *dialog = findChild<QDialog *>("videoControlDialog");
    if (auto *controls = dialog->findChild<VideoControlWidget *>())
        controls->addPlaylistPaths(paths);
}
void MainWindow::showFileInfo() {
    auto &p = mPages[mPage];
    auto *manager = mediaManager(mPage);
    auto source = p.proxy->mapToSource(p.files->currentIndex());
    if (!manager || !source.isValid())
        return;
    const auto &m = manager->mediaData(source.row());
    QDialog dialog(this);
    dialog.setObjectName("fileInfoDialog");
    dialog.setWindowTitle("Все сведения о файле");
    dialog.resize(560, 430);
    auto *l = column(&dialog, 24, 14);
    l->addWidget(label("Сведения о файле", 22, QFont::DemiBold));
    auto *name = label(m.fileName(), 16, QFont::DemiBold);
    name->setWordWrap(true);
    l->addWidget(name);
    QString details = QStringLiteral("Название: %1\nИсполнитель: %2\nАльбом: %3\nЖанр: %4\nГод: "
                                     "%5\nДлительность: %6\nФормат: %7\nРазмер: %8 байт\nПуть: %9")
                          .arg(known(m.title()), known(m.artist()), known(m.album()), known(m.genre()),
                               m.year() ? QString::number(m.year()) : "Не указан",
                               m.length() ? manager->calculateLength(m.length()) : "Неизвестно",
                               QFileInfo(m.fileName()).suffix().toUpper(), QString::number(m.fileSize()),
                               manager->getDirMediaFiles().filePath(m.fileName()));
    auto *body = label(details, 12);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(body, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    buttons->button(QDialogButtonBox::Close)->setText("Закрыть");
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    l->addWidget(buttons);
    dialog.exec();
}
void MainWindow::showStationInfo() {
    QDialog dialog(this);
    dialog.setObjectName("stationInfoDialog");
    dialog.setWindowTitle("Сведения о станции");
    dialog.resize(560, 350);
    auto *l = column(&dialog, 24, 14);
    l->addWidget(label("Сведения о станции", 22, QFont::DemiBold));
    auto &s = StationManager::Instance();
    auto *body =
        label(QStringLiteral("%1\nИдентификатор: %2\nПуть: %3\nКонфигурация: %4\n\n%5\nMediaBoxPlayer: %6\n%7")
                  .arg(s.typeText(), QString::number(s.id()), s.get(), s.configFile(), s.lastError(),
                       mPlayerState->text(), mPlayerDetail->text()),
              12);
    body->setWordWrap(true);
    body->setTextInteractionFlags(Qt::TextSelectableByMouse);
    l->addWidget(body, 1);
    auto *close = button("Закрыть", "close", "primary");
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    l->addWidget(close, 0, Qt::AlignRight);
    dialog.exec();
}
void MainWindow::showAllSchedules() {
    QDialog dialog(this);
    dialog.setObjectName("allSchedulesDialog");
    dialog.setWindowTitle("Все параметры расписания");
    dialog.resize(1000, 620);
    auto *l = column(&dialog, 24, 12);
    l->addWidget(label("Все параметры расписания", 22, QFont::DemiBold));
    auto *table = new QTableView;
    configureTable(table);
    auto *display = new RuleEditors::RuleTableModel(mPage < 2 ? RuleEditors::RuleTableModel::Channels
                                                              : RuleEditors::RuleTableModel::Adverts, table);
    display->setSourceModel(mPage < 2 ? static_cast<QAbstractItemModel *>(mChannelModels[mPage]) : mAdvertModel);
    table->setModel(display);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    connect(table, &QTableView::doubleClicked, &dialog, [this, &dialog](const QModelIndex &i) {
        if (mMediaImport || !i.isValid())
            return;
        if (mPage < 2)
            RuleEditors::editChannel(mChannelModels[mPage], i.row(), &dialog);
        else if (advertWritable())
            RuleEditors::editAdvert(mAdvertModel, i.row(), &dialog);
    });
    l->addWidget(table, 1);
    auto *actions = new QHBoxLayout;
    actions->addStretch();
    auto *edit = button("Редактировать правило", "edit");
    actions->addWidget(edit);
    auto *close = button("Закрыть", "close", "primary");
    actions->addWidget(close);
    l->addLayout(actions);
    connect(close, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(edit, &QPushButton::clicked, &dialog, [this, table, &dialog] {
        if (mMediaImport)
            return;
        auto i = table->currentIndex();
        if (!i.isValid())
            return;
        if (mPage < 2)
            RuleEditors::editChannel(mChannelModels[mPage], i.row(), &dialog);
        else if (advertWritable())
            RuleEditors::editAdvert(mAdvertModel, i.row(), &dialog);
    });
    edit->setEnabled(!mMediaImport && (mPage < 2 || advertWritable()));
    dialog.exec();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    adaptLayout(width());
}

void MainWindow::adaptLayout(int width) {
    const bool compact = width < 1100;
    for (auto *widget : centralWidget()->findChildren<QWidget *>())
        if (widget->property("compactHide").toBool())
            widget->setVisible(!compact);
    for (auto *nav : mNavigation) {
        const QString text = nav->property("fullText").toString();
        nav->setText(compact ? QString() : text);
        nav->setMinimumWidth(compact ? 32 : 0);
        nav->setMaximumWidth(compact ? 38 : QWIDGETSIZE_MAX);
    }
    mOperationState->setVisible(!compact);
    if (auto *station = findChild<QPushButton *>("stationInfoButton")) {
        station->setText(compact ? QString() : station->property("fullText").toString());
        station->setToolTip(compact ? station->property("fullText").toString() + " · сведения о станции"
                                    : "Сведения о станции");
    }
    for (const QString &name : {QStringLiteral("refreshPlayerButton"), QStringLiteral("showPlayerButton")}) {
        auto *control = findChild<QPushButton *>(name);
        if (!control)
            continue;
        QString title = name == "refreshPlayerButton" ? "Обновить состояние" : "Аудиоплеер";
        control->setText(compact ? QString() : title);
        control->setToolTip(name == "refreshPlayerButton" ? "Обновить состояние плеера и расписание (F5)" : title);
        control->setMinimumWidth(compact ? 32 : 0);
        control->setMaximumWidth(compact ? 32 : QWIDGETSIZE_MAX);
    }
}
