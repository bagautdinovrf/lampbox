#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "settings.h"
#include "mediacontroller.h"
#include "restylewidgets.h"

#include <QApplication>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyledItemDelegate>
#include <QStyleOptionButton>
#include <QVBoxLayout>

namespace {
// Keep formats in a model/view: extra formats from existing configurations remain
// available without rebuilding the page when a theme changes.
class FormatDelegate final : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const override
    {
        const auto *view = qobject_cast<const QListView *>(parent());
        return view && view->gridSize().isValid() ? view->gridSize() : QSize(150, 46);
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override
    {
        const auto &t = Restyle::tokens();
        const bool checked = index.data(Qt::CheckStateRole).toInt() == Qt::Checked;
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        const QRect card = option.rect.adjusted(2, 2, -7, -6);
        Restyle::paintSurface(*p, card, checked ? QStringLiteral("inset") : QStringLiteral("panel"));
        if ((option.state & QStyle::State_MouseOver) && !checked) {
            QColor hover = t.accentSoft;
            hover.setAlpha(95);
            p->setPen(Qt::NoPen);
            p->setBrush(hover);
            p->drawRoundedRect(card.adjusted(1, 1, -1, -1), 8, 8);
        }
        QStyleOptionButton check;
        check.rect = QRect(card.left() + 12, card.center().y() - 7, 14, 14);
        check.state = QStyle::State_Enabled | (checked ? QStyle::State_On : QStyle::State_Off);
        check.palette = Restyle::palette();
        QApplication::style()->drawPrimitive(QStyle::PE_IndicatorCheckBox, &check, p);
        p->setFont(Restyle::font(11));
        p->setPen(checked ? t.accentText : t.text);
        p->drawText(card.adjusted(38, 0, -8, 0), Qt::AlignVCenter | Qt::AlignLeft,
                    index.data().toString().toUpper());
        if (option.state & QStyle::State_HasFocus) {
            p->setPen(QPen(t.focus, 2));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(card.adjusted(1, 1, -1, -1), 7, 7);
        }
        p->restore();
    }
    bool editorEvent(QEvent *event, QAbstractItemModel *model,
                     const QStyleOptionViewItem &option, const QModelIndex &index) override
    {
        if (event->type() == QEvent::MouseButtonRelease) {
            auto *mouse = static_cast<QMouseEvent *>(event);
            if (mouse->button() == Qt::LeftButton && option.rect.contains(mouse->position().toPoint()))
                return model->setData(index, index.data(Qt::CheckStateRole).toInt() == Qt::Checked
                                     ? Qt::Unchecked : Qt::Checked, Qt::CheckStateRole);
        }
        return QStyledItemDelegate::editorEvent(event, model, option, index);
    }
};

class FormatList final : public QListWidget
{
public:
    explicit FormatList(QWidget *parent) : QListWidget(parent)
    {
        setViewMode(QListView::IconMode);
        setFlow(QListView::LeftToRight);
        setWrapping(true);
        setMovement(QListView::Static);
        setResizeMode(QListView::Adjust);
        setGridSize(QSize(150, 46));
        setUniformItemSizes(true);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setMouseTracking(true);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setFrameShape(QFrame::NoFrame);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setItemDelegate(new FormatDelegate(this));
        setFont(Restyle::font(11));
        setMinimumHeight(88);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        viewport()->setAutoFillBackground(false);
        setAutoFillBackground(false);
    }
protected:
    void resizeEvent(QResizeEvent *event) override
    {
        QListWidget::resizeEvent(event);
        const int columns = viewport()->width() < 320 ? 2 : 3;
        // QListView wraps a grid cell whose right edge equals viewport width;
        // reserve one pixel so three columns also fit even viewport widths.
        setGridSize(QSize(qMax(80, (viewport()->width() - 1) / columns), 46));
        setFixedHeight(qMax(2, (count() + columns - 1) / columns) * 46);
    }
};

class AppearanceSummary final : public RestyleLabel
{
public:
    explicit AppearanceSummary(QWidget *parent) : RestyleLabel({}, 12, QFont::Normal, parent)
    {
        setObjectName(QStringLiteral("appearanceSummary"));
        setColorRole(QStringLiteral("muted"));
        setWordWrap(true);
        refresh();
    }
protected:
    bool event(QEvent *event) override
    {
        const bool handled = RestyleLabel::event(event);
        if (event->type() == QEvent::ApplicationPaletteChange)
            refresh();
        return handled;
    }
    void changeEvent(QEvent *event) override
    {
        RestyleLabel::changeEvent(event);
        if (event->type() == QEvent::ApplicationPaletteChange || event->type() == QEvent::PaletteChange
                || event->type() == QEvent::StyleChange)
            refresh();
    }
private:
    void refresh()
    {
        setText(QObject::tr("Сейчас: %1 · %2")
                .arg(Restyle::appearanceName(Restyle::appearanceId()),
                     Restyle::themeName(Restyle::themeId())));
    }
};

RestyleLabel *caption(const QString &text, QWidget *parent)
{
    auto *label = new RestyleLabel(text, 11, QFont::Normal, parent);
    label->setColorRole(QStringLiteral("muted"));
    label->setWordWrap(true);
    return label;
}
}

SettingsDialog::SettingsDialog(QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f), ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setFont(Restyle::font());
    resize(1124, 630);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *body = new QWidget(scroll);
    auto *page = new QVBoxLayout(body);
    page->setContentsMargins(22, 17, 22, 24);
    page->setSpacing(0);
    auto *title = new RestyleLabel(tr("Настройки"), 28, QFont::DemiBold, body);
    title->setFont(Restyle::font(28, QFont::DemiBold, -.75));
    title->setFixedHeight(33);
    page->addWidget(title);
    page->addSpacing(3);
    auto *subtitle = caption(tr("Подключение плеера, медиаформаты и оформление."), body);
    subtitle->setMinimumHeight(16);
    page->addWidget(subtitle);
    page->addSpacing(12);

    auto *player = new RestylePanel(body);
    player->setObjectName(QStringLiteral("playerConnectionCard"));
    player->setMaximumWidth(1080);
    auto *playerLayout = new QVBoxLayout(player);
    playerLayout->setContentsMargins(21, 20, 21, 21);
    playerLayout->setSpacing(12);
    playerLayout->addWidget(new RestyleLabel(tr("MediaBoxPlayer"), 20, QFont::DemiBold, player));
    playerLayout->addWidget(caption(MediaController::supportsLocalStart()
                                    ? tr("На этом компьютере Manager сам запустит MediaBoxPlayer, если нет подключения. "
                                       "На удалённой машине плеер нужно запустить заранее. "
                                       "Пути медиафайлов относятся к машине плеера.")
                                    : tr("MediaBoxPlayer должен быть запущен на указанной машине. "
                                         "Пути медиафайлов относятся к машине плеера."), player));

    auto *fields = new QGridLayout;
    fields->setHorizontalSpacing(12);
    fields->setVerticalSpacing(6);
    fields->setColumnStretch(0, 1);
    mPlayerHost = new QLineEdit(player);
    mPlayerHost->setObjectName(QStringLiteral("playerHost"));
    mPlayerHost->setAccessibleName(tr("Адрес машины плеера"));
    mPlayerHost->setPlaceholderText(QStringLiteral("127.0.0.1"));
    mPlayerHost->setMinimumHeight(32);
    mPlayerPort = new QSpinBox(player);
    mPlayerPort->setObjectName(QStringLiteral("playerPort"));
    mPlayerPort->setAccessibleName(tr("TCP-порт плеера"));
    mPlayerPort->setRange(1, 65535);
    mPlayerPort->setMinimumHeight(32);
    mPlayerPort->setFixedWidth(112);
    mPlayerToken = new QLineEdit(player);
    mPlayerToken->setObjectName(QStringLiteral("playerToken"));
    mPlayerToken->setAccessibleName(tr("Токен доступа к плееру"));
    mPlayerToken->setEchoMode(QLineEdit::Password);
    mPlayerToken->setPlaceholderText(MediaController::supportsLocalStart()
                                   ? tr("Для локального плеера можно оставить пустым")
                                   : tr("Токен из control.token"));
    mPlayerToken->setMinimumHeight(32);
    auto *hostLabel = caption(tr("Адрес / DNS-имя"), player);
    hostLabel->setBuddy(mPlayerHost);
    auto *portLabel = caption(tr("TCP-порт"), player);
    portLabel->setBuddy(mPlayerPort);
    auto *tokenLabel = caption(tr("Токен доступа"), player);
    tokenLabel->setBuddy(mPlayerToken);
    fields->addWidget(hostLabel, 0, 0);
    fields->addWidget(portLabel, 0, 1);
    fields->addWidget(mPlayerHost, 1, 0);
    fields->addWidget(mPlayerPort, 1, 1);
    fields->addWidget(tokenLabel, 2, 0, 1, 2);
    fields->addWidget(mPlayerToken, 3, 0, 1, 2);
    playerLayout->addLayout(fields);
    playerLayout->addWidget(caption(MediaController::supportsLocalStart()
                                    ? tr("Для localhost, 127.0.0.1 или ::1 токен определяется автоматически. "
                                         "Для удалённого плеера скопируйте 64 символа из его файла control.token.")
                                    : tr("Скопируйте 64 символа из файла control.token в каталоге данных плеера."), player));
    mPlayerConnectionMessage = caption({}, player);
    mPlayerConnectionMessage->setObjectName(QStringLiteral("playerConnectionMessage"));
    mPlayerConnectionMessage->setTextFormat(Qt::PlainText);
    mPlayerConnectionMessage->hide();
    playerLayout->addWidget(mPlayerConnectionMessage);
    auto *saveConnection = new QPushButton(tr("Сохранить подключение"), player);
    saveConnection->setObjectName(QStringLiteral("savePlayerConnection"));
    Restyle::button(saveConnection, QStringLiteral("primary"));
    saveConnection->setAutoDefault(false);
    saveConnection->setFixedHeight(32);
    playerLayout->addWidget(saveConnection, 0, Qt::AlignLeft);
    page->addWidget(player);
    page->addSpacing(16);

    auto *videoPlayer = new RestylePanel(body);
    videoPlayer->setObjectName(QStringLiteral("videoPlayerConnectionCard"));
    videoPlayer->setMaximumWidth(1080);
    auto *videoPlayerLayout = new QVBoxLayout(videoPlayer);
    videoPlayerLayout->setContentsMargins(21, 20, 21, 21);
    videoPlayerLayout->setSpacing(12);
    videoPlayerLayout->addWidget(new RestyleLabel(tr("MediaBoxVPlayer"), 20, QFont::DemiBold, videoPlayer));
    videoPlayerLayout->addWidget(caption(MediaController::supportsLocalStart()
                                    ? tr("На этом компьютере Manager сам запустит MediaBoxVPlayer, если нет подключения. "
                                         "На удалённой машине видеоплеер нужно запустить заранее. "
                                         "Пути медиафайлов относятся к машине плеера.")
                                    : tr("MediaBoxVPlayer должен быть запущен на указанной машине. "
                                         "Пути медиафайлов относятся к машине плеера."), videoPlayer));

    auto *videoFields = new QGridLayout;
    videoFields->setHorizontalSpacing(12);
    videoFields->setVerticalSpacing(6);
    videoFields->setColumnStretch(0, 1);
    mVideoPlayerHost = new QLineEdit(videoPlayer);
    mVideoPlayerHost->setObjectName(QStringLiteral("videoPlayerHost"));
    mVideoPlayerHost->setAccessibleName(tr("Адрес машины плеера"));
    mVideoPlayerHost->setPlaceholderText(QStringLiteral("127.0.0.1"));
    mVideoPlayerHost->setMinimumHeight(32);
    mVideoPlayerPort = new QSpinBox(videoPlayer);
    mVideoPlayerPort->setObjectName(QStringLiteral("videoPlayerPort"));
    mVideoPlayerPort->setAccessibleName(tr("TCP-порт плеера"));
    mVideoPlayerPort->setRange(1, 65535);
    mVideoPlayerPort->setMinimumHeight(32);
    mVideoPlayerPort->setFixedWidth(112);
    mVideoPlayerToken = new QLineEdit(videoPlayer);
    mVideoPlayerToken->setObjectName(QStringLiteral("videoPlayerToken"));
    mVideoPlayerToken->setAccessibleName(tr("Токен доступа к плееру"));
    mVideoPlayerToken->setEchoMode(QLineEdit::Password);
    mVideoPlayerToken->setPlaceholderText(MediaController::supportsLocalStart()
                                       ? tr("Для локального видеоплеера можно оставить пустым")
                                       : tr("Токен из control.token"));
    mVideoPlayerToken->setMinimumHeight(32);
    auto *videoHostLabel = caption(tr("Адрес / DNS-имя"), videoPlayer);
    videoHostLabel->setBuddy(mVideoPlayerHost);
    auto *videoPortLabel = caption(tr("TCP-порт"), videoPlayer);
    videoPortLabel->setBuddy(mVideoPlayerPort);
    auto *videoTokenLabel = caption(tr("Токен доступа"), videoPlayer);
    videoTokenLabel->setBuddy(mVideoPlayerToken);
    videoFields->addWidget(videoHostLabel, 0, 0);
    videoFields->addWidget(videoPortLabel, 0, 1);
    videoFields->addWidget(mVideoPlayerHost, 1, 0);
    videoFields->addWidget(mVideoPlayerPort, 1, 1);
    videoFields->addWidget(videoTokenLabel, 2, 0, 1, 2);
    videoFields->addWidget(mVideoPlayerToken, 3, 0, 1, 2);
    videoPlayerLayout->addLayout(videoFields);
    videoPlayerLayout->addWidget(caption(MediaController::supportsLocalStart()
                                    ? tr("Для localhost, 127.0.0.1 или ::1 токен определяется автоматически. "
                                         "Для удалённого видеоплеера скопируйте 64 символа из его файла control.token.")
                                    : tr("Скопируйте 64 символа из файла control.token видеоплеера в каталоге данных плеера."), videoPlayer));
    mVideoPlayerConnectionMessage = caption({}, videoPlayer);
    mVideoPlayerConnectionMessage->setObjectName(QStringLiteral("videoPlayerConnectionMessage"));
    mVideoPlayerConnectionMessage->setTextFormat(Qt::PlainText);
    mVideoPlayerConnectionMessage->hide();
    videoPlayerLayout->addWidget(mVideoPlayerConnectionMessage);
    auto *saveVideoConnection = new QPushButton(tr("Сохранить подключение"), videoPlayer);
    saveVideoConnection->setObjectName(QStringLiteral("saveVideoPlayerConnection"));
    Restyle::button(saveVideoConnection, QStringLiteral("primary"));
    saveVideoConnection->setAutoDefault(false);
    saveVideoConnection->setFixedHeight(32);
    videoPlayerLayout->addWidget(saveVideoConnection, 0, Qt::AlignLeft);
    auto *videoScreens = new QPushButton(tr("Настроить видеоэкраны и плейлисты"), videoPlayer);
    videoScreens->setObjectName(QStringLiteral("configureVideoScreens"));
    Restyle::button(videoScreens);
    videoScreens->setAutoDefault(false);
    videoScreens->setMinimumHeight(32);
    videoPlayerLayout->addWidget(videoScreens, 0, Qt::AlignLeft);
    connect(videoScreens, &QPushButton::clicked, this, &SettingsDialog::videoScreensRequested);
    page->addWidget(videoPlayer);
    page->addSpacing(16);

    auto *card = new RestylePanel(body);
    card->setObjectName(QStringLiteral("mediaFormatsCard"));
    card->setMaximumWidth(1080);
    auto *content = new QVBoxLayout(card);
    content->setContentsMargins(21, 20, 21, 21);
    content->setSpacing(0);
    auto *heading = new QHBoxLayout;
    auto *headingLabel = new RestyleLabel(tr("Медиаформаты"), 20, QFont::DemiBold, card);
    headingLabel->setFont(Restyle::font(20, QFont::DemiBold, -.4));
    heading->addWidget(headingLabel);
    heading->addStretch();
    auto *automatic = caption(tr("Сохраняется автоматически"), card);
    automatic->setFont(Restyle::font(9));
    automatic->setWordWrap(false);
    heading->addWidget(automatic);
    content->addLayout(heading);
    content->addSpacing(21);
    auto *description = caption(tr("Выберите форматы, доступные при добавлении медиа."), card);
    description->setFont(Restyle::font(12));
    description->setMinimumHeight(19);
    content->addWidget(description);
    content->addSpacing(18);
    auto *formats = new QHBoxLayout;
    formats->setSpacing(17);
    for (bool audio : {true, false}) {
        auto *column = new QVBoxLayout;
        column->setSpacing(13);
        column->addWidget(new RestyleLabel(audio ? tr("Аудио") : tr("Видео"), 14, QFont::DemiBold, card));
        auto *list = new FormatList(card);
        list->setObjectName(audio ? QStringLiteral("lw_AudioFileType") : QStringLiteral("lw_VideoFileType"));
        list->setAccessibleName(audio ? tr("Форматы аудио") : tr("Форматы видео"));
        column->addWidget(list);
        formats->addLayout(column, 1);
        (audio ? mAudioFormats : mVideoFormats) = list;
    }
    content->addLayout(formats);
    content->addSpacing(18);
    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    actions->addStretch();
    auto *none = new QPushButton(tr("Очистить все"), card);
    none->setObjectName(QStringLiteral("pb_deselectAll"));
    auto *all = new QPushButton(tr("Выбрать все"), card);
    all->setObjectName(QStringLiteral("pb_selectAll"));
    for (auto *button : {none, all}) {
        Restyle::button(button);
        button->setAutoDefault(false);
        button->setFixedHeight(32);
        actions->addWidget(button);
    }
    content->addLayout(actions);
    page->addWidget(card);
    page->addSpacing(16);

    auto *comfort = new RestylePanel(body);
    comfort->setMaximumWidth(1080);
    auto *comfortLayout = new QVBoxLayout(comfort);
    comfortLayout->setContentsMargins(21, 20, 21, 21);
    comfortLayout->setSpacing(12);
    comfortLayout->addWidget(new RestyleLabel(tr("Оформление"), 20, QFont::DemiBold, comfort));
    comfortLayout->addWidget(new AppearanceSummary(comfort));
    comfortLayout->addWidget(caption(tr("Оформление и тема выбираются в шапке окна. Изменения сохраняются автоматически."), comfort));
    comfortLayout->addWidget(caption(tr("Смена оформления сохраняет открытый раздел, выделение и введённые данные."), comfort));
    page->addWidget(comfort);
    page->addSpacing(16);
    auto *done = new QPushButton(tr("Готово"), body);
    done->setObjectName(QStringLiteral("pb_close"));
    done->setIcon(Restyle::icon(QStringLiteral("check")));
    Restyle::button(done, QStringLiteral("primary"));
    done->setFixedHeight(32);
    done->setAutoDefault(false);
    page->addWidget(done, 0, Qt::AlignLeft);
    page->addStretch();
    scroll->setWidget(body);
    outer->addWidget(scroll);

    init();
    connect(saveConnection, &QPushButton::clicked, this, &SettingsDialog::savePlayerConnection);
    connect(mPlayerHost, &QLineEdit::returnPressed, this, &SettingsDialog::savePlayerConnection);
    connect(mPlayerToken, &QLineEdit::returnPressed, this, &SettingsDialog::savePlayerConnection);
    const auto connectionEdited = [this] {
        mPlayerConnectionMessage->setColorRole(QStringLiteral("muted"));
        mPlayerConnectionMessage->setText(tr("Изменения ещё не сохранены."));
        mPlayerConnectionMessage->show();
    };
    connect(mPlayerHost, &QLineEdit::textEdited, this, connectionEdited);
    connect(mPlayerPort, &QSpinBox::valueChanged, this, connectionEdited);
    connect(mPlayerToken, &QLineEdit::textEdited, this, connectionEdited);
    connect(saveVideoConnection, &QPushButton::clicked, this, &SettingsDialog::saveVideoPlayerConnection);
    connect(mVideoPlayerHost, &QLineEdit::returnPressed, this, &SettingsDialog::saveVideoPlayerConnection);
    connect(mVideoPlayerToken, &QLineEdit::returnPressed, this, &SettingsDialog::saveVideoPlayerConnection);
    const auto videoConnectionEdited = [this] {
        mVideoPlayerConnectionMessage->setColorRole(QStringLiteral("muted"));
        mVideoPlayerConnectionMessage->setText(tr("Изменения ещё не сохранены."));
        mVideoPlayerConnectionMessage->show();
    };
    connect(mVideoPlayerHost, &QLineEdit::textEdited, this, videoConnectionEdited);
    connect(mVideoPlayerPort, &QSpinBox::valueChanged, this, videoConnectionEdited);
    connect(mVideoPlayerToken, &QLineEdit::textEdited, this, videoConnectionEdited);
    connect(none, &QPushButton::clicked, this, &SettingsDialog::deselectAllFileFormats);
    connect(all, &QPushButton::clicked, this, &SettingsDialog::selectAllFileFormats);
    connect(done, &QPushButton::clicked, this, [this] {
        if (isWindow()) accept();
        else emit doneRequested();
    });
}

SettingsDialog::~SettingsDialog() { delete ui; }

void SettingsDialog::init()
{
    Settings settings;
    const PlayerConnectionSettings connection = settings.playerConnection();
    mPlayerHost->setText(connection.host);
    mPlayerPort->setValue(connection.port);
    mPlayerToken->setText(connection.token);
    const PlayerConnectionSettings videoConnection = settings.videoPlayerConnection();
    mVideoPlayerHost->setText(videoConnection.host);
    mVideoPlayerPort->setValue(videoConnection.port);
    mVideoPlayerToken->setText(videoConnection.token);
    const auto fill = [](QListWidget *list, const QMap<QString, bool> &formats, QStringList order) {
        for (auto it = formats.cbegin(); it != formats.cend(); ++it)
            if (!order.contains(it.key())) order.append(it.key());
        for (const QString &key : order) {
            if (!formats.contains(key)) continue;
            auto *item = new QListWidgetItem(key, list);
            item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            item->setCheckState(formats.value(key) ? Qt::Checked : Qt::Unchecked);
        }
    };
    fill(mAudioFormats, settings.fileFormatsAudio(), {"mp3", "flac", "ogg", "wma", "acc"});
    fill(mVideoFormats, settings.fileFormatsVideo(), {"mp4", "avi", "mkv", "wmv"});
    connect(mAudioFormats, &QListWidget::itemChanged, this, &SettingsDialog::checkAudioItem);
    connect(mVideoFormats, &QListWidget::itemChanged, this, &SettingsDialog::checkVideoItem);
}

void SettingsDialog::savePlayerConnection()
{
    const auto showError = [this](const QString &message, QWidget *field) {
        mPlayerConnectionMessage->setColorRole(QStringLiteral("error"));
        mPlayerConnectionMessage->setText(message);
        mPlayerConnectionMessage->show();
        if (field)
            field->setFocus();
    };
    PlayerConnectionSettings connection;
    connection.host = mPlayerHost->text().trimmed();
    if (connection.host.isEmpty()) {
        showError(tr("Укажите адрес машины плеера. Для этого компьютера — 127.0.0.1."), mPlayerHost);
        return;
    }
    if (!mPlayerPort->hasAcceptableInput()) {
        showError(tr("Укажите TCP-порт от 1 до 65535."), mPlayerPort);
        return;
    }
    connection.port = static_cast<quint16>(mPlayerPort->value());
    connection.token = mPlayerToken->text().trimmed();
    static const QRegularExpression tokenPattern(QStringLiteral("\\A[0-9a-f]{64}\\z"));
    const bool automaticLocalToken = MediaController::supportsLocalStart()
                                     && connection.token.isEmpty() && MediaController::isLocalHost(connection.host);
    if (!automaticLocalToken && !tokenPattern.match(connection.token).hasMatch()) {
        showError(tr("Токен должен содержать 64 символа: цифры 0–9 и строчные буквы a–f."), mPlayerToken);
        return;
    }
    if (!Settings().setPlayerConnection(connection)) {
        showError(tr("Не удалось сохранить подключение. Проверьте доступ к файлу настроек."), nullptr);
        return;
    }
    mPlayerHost->setText(connection.host);
    mPlayerToken->setText(connection.token);
    mPlayerConnectionMessage->setColorRole(QStringLiteral("success"));
    mPlayerConnectionMessage->setText(tr("Подключение сохранено."));
    mPlayerConnectionMessage->show();
    emit playerConnectionChanged();
}

void SettingsDialog::saveVideoPlayerConnection()
{
    const auto showError = [this](const QString &message, QWidget *field) {
        mVideoPlayerConnectionMessage->setColorRole(QStringLiteral("error"));
        mVideoPlayerConnectionMessage->setText(message);
        mVideoPlayerConnectionMessage->show();
        if (field)
            field->setFocus();
    };
    PlayerConnectionSettings connection;
    connection.host = mVideoPlayerHost->text().trimmed();
    if (connection.host.isEmpty()) {
        showError(tr("Укажите адрес машины видеоплеера. Для этого компьютера — 127.0.0.1."), mVideoPlayerHost);
        return;
    }
    if (!mVideoPlayerPort->hasAcceptableInput()) {
        showError(tr("Укажите TCP-порт от 1 до 65535."), mVideoPlayerPort);
        return;
    }
    connection.port = static_cast<quint16>(mVideoPlayerPort->value());
    connection.token = mVideoPlayerToken->text().trimmed();
    static const QRegularExpression tokenPattern(QStringLiteral("\\A[0-9a-f]{64}\\z"));
    const bool automaticLocalToken = MediaController::supportsLocalStart()
                                     && connection.token.isEmpty() && MediaController::isLocalHost(connection.host);
    if (!automaticLocalToken && !tokenPattern.match(connection.token).hasMatch()) {
        showError(tr("Токен должен содержать 64 символа: цифры 0–9 и строчные буквы a–f."), mVideoPlayerToken);
        return;
    }
    if (!Settings().setVideoPlayerConnection(connection)) {
        showError(tr("Не удалось сохранить подключение. Проверьте доступ к файлу настроек."), nullptr);
        return;
    }
    mVideoPlayerHost->setText(connection.host);
    mVideoPlayerToken->setText(connection.token);
    mVideoPlayerConnectionMessage->setColorRole(QStringLiteral("success"));
    mVideoPlayerConnectionMessage->setText(tr("Подключение сохранено."));
    mVideoPlayerConnectionMessage->show();
    emit videoPlayerConnectionChanged();
}

void SettingsDialog::checkAudioItem(QListWidgetItem *item)
{
    if (!item) return;
    Settings().writeFileFormatAudioValue(item->text(), item->checkState() == Qt::Checked);
    emit fileFormatsAudio();
}

void SettingsDialog::checkVideoItem(QListWidgetItem *item)
{
    if (!item) return;
    Settings().writeFileFormatVideoValue(item->text(), item->checkState() == Qt::Checked);
    emit fileFormatsVideo();
}

void SettingsDialog::selectAllFileFormats()
{
    changeAudioTypesCheckState(Qt::Checked);
    changeVideoTypesCheckState(Qt::Checked);
}

void SettingsDialog::deselectAllFileFormats()
{
    changeAudioTypesCheckState(Qt::Unchecked);
    changeVideoTypesCheckState(Qt::Unchecked);
}

void SettingsDialog::changeAudioTypesCheckState(Qt::CheckState state)
{
    const QSignalBlocker block(mAudioFormats);
    Settings settings;
    for (int i = 0; i < mAudioFormats->count(); ++i) {
        auto *item = mAudioFormats->item(i);
        item->setCheckState(state);
        settings.writeFileFormatAudioValue(item->text(), state == Qt::Checked);
    }
    emit fileFormatsAudio();
}

void SettingsDialog::changeVideoTypesCheckState(Qt::CheckState state)
{
    const QSignalBlocker block(mVideoFormats);
    Settings settings;
    for (int i = 0; i < mVideoFormats->count(); ++i) {
        auto *item = mVideoFormats->item(i);
        item->setCheckState(state);
        settings.writeFileFormatVideoValue(item->text(), state == Qt::Checked);
    }
    emit fileFormatsVideo();
}

void SettingsDialog::slot_changePage(int page) { Q_UNUSED(page); }
