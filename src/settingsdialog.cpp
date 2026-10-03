#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "settings.h"
#include "restylewidgets.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
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
    auto *subtitle = caption(tr("Форматы медиафайлов и удобство работы."), body);
    subtitle->setMinimumHeight(16);
    page->addWidget(subtitle);
    page->addSpacing(12);

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
