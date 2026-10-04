#include "aboutmediaboxmanager.h"
#include "restylewidgets.h"

#include <QDesktopServices>
#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollArea>
#include <QUrl>
#include <QVBoxLayout>

namespace {
constexpr int buildYear = (__DATE__[7] - '0') * 1000
        + (__DATE__[8] - '0') * 100
        + (__DATE__[9] - '0') * 10
        + (__DATE__[10] - '0');
}

AboutMediaBoxManager::AboutMediaBoxManager(QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f)
{
    setObjectName(QStringLiteral("AboutMediaBoxManager"));
    setWindowTitle(tr("О MediaBoxManager"));
    setFont(Restyle::font());
    resize(1124, 570);
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
    auto *title = new RestyleLabel(tr("О программе"), 28, QFont::DemiBold, body);
    title->setFont(Restyle::font(28, QFont::DemiBold, -.75));
    title->setFixedHeight(33);
    page->addWidget(title);
    page->addSpacing(3);
    auto *subtitle = new RestyleLabel(tr("Единое пространство для музыки, видео и рекламы."), 11, QFont::Normal, body);
    subtitle->setColorRole(QStringLiteral("muted"));
    subtitle->setWordWrap(true);
    subtitle->setMinimumHeight(16);
    page->addWidget(subtitle);
    page->addSpacing(12);

    auto *card = new RestylePanel(body);
    card->setObjectName(QStringLiteral("aboutCard"));
    card->setMaximumWidth(1080);
    auto *content = new QVBoxLayout(card);
    content->setContentsMargins(21, 20, 21, 21);
    content->setSpacing(15);
    auto *brand = new RestyleLabel(QStringLiteral("MediaBoxManager"), 32, QFont::DemiBold, card);
    brand->setFont(Restyle::font(32, QFont::DemiBold, -1));
    brand->setMinimumHeight(46);
    content->addWidget(brand);
    content->addSpacing(5);
    auto *heading = new RestyleLabel(tr("Управление медиастанцией."), 20, QFont::DemiBold, card);
    heading->setFont(Restyle::font(20, QFont::DemiBold, -.4));
    content->addWidget(heading);
    const auto paragraph = [&](const QString &text, const QString &name = QString()) {
        auto *label = new RestyleLabel(text, 12, QFont::Normal, card);
        label->setColorRole(QStringLiteral("muted"));
        label->setWordWrap(true);
        label->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard);
        label->setObjectName(name);
        content->addWidget(label);
        return label;
    };
    paragraph(tr("Версия: %1").arg(QStringLiteral(VERSION)), QStringLiteral("label_version"));
    paragraph(tr("Автор программы: Руслан Багаутдинов."), QStringLiteral("label_author"));
    paragraph(tr("Управление музыкальными и видеоканалами, рекламными выходами и медиатекой. Настройки и расписания сохраняются для выбранной станции."));
    paragraph(tr("Просмотр плана показывает каналы, календарные условия и ближайшие события. Он не подтверждает звучащий трек."));
    paragraph(tr("Программа защищается законодательством об авторском праве. Незаконное копирование или распространение программы или её части запрещено."));
    paragraph(QStringLiteral("© 2012–%1 Руслан Багаутдинов").arg(buildYear), QStringLiteral("label_years"));
    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    auto *email = new QPushButton(QStringLiteral("bagautdinovrf@ya.ru"), card);
    email->setObjectName(QStringLiteral("label_email"));
    email->setAccessibleName(tr("Написать автору: bagautdinovrf@ya.ru"));
    Restyle::button(email);
    email->setFixedHeight(32);
    email->setAutoDefault(false);
    actions->addWidget(email);
    auto *done = new QPushButton(tr("Готово"), card);
    done->setObjectName(QStringLiteral("pb_close"));
    Restyle::button(done, QStringLiteral("primary"));
    done->setFixedHeight(32);
    done->setAutoDefault(false);
    actions->addWidget(done);
    actions->addStretch();
    content->addLayout(actions);
    page->addWidget(card);
    page->addStretch();
    scroll->setWidget(body);
    outer->addWidget(scroll);
    connect(email, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("mailto:bagautdinovrf@ya.ru")));
    });
    connect(done, &QPushButton::clicked, this, [this] {
        if (isWindow()) accept();
        else emit doneRequested();
    });
}

AboutMediaBoxManager::~AboutMediaBoxManager() = default;
