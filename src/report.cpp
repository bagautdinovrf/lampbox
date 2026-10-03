#include "report.h"
#include "ui_report.h"
#include "restylewidgets.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLocale>
#include <QPushButton>
#include <QScrollArea>
#include <QStackedWidget>
#include <QTableView>
#include <QVBoxLayout>

Report::Report(QWidget *parent, Qt::WindowFlags f) : QWidget(parent, f), ui(new Ui::Report)
{
    ui->setupUi(this);
    setFont(Restyle::font());
    resize(1124, 600);
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
    auto *title = new RestyleLabel(tr("Отчёты"), 28, QFont::DemiBold, body);
    title->setFont(Restyle::font(28, QFont::DemiBold, -.75));
    title->setFixedHeight(33);
    page->addWidget(title);
    page->addSpacing(3);
    auto *subtitle = new RestyleLabel(tr("Композиции и частота звучания за выбранный месяц."), 11, QFont::Normal, body);
    subtitle->setColorRole(QStringLiteral("muted"));
    subtitle->setWordWrap(true);
    subtitle->setMinimumHeight(16);
    page->addWidget(subtitle);
    page->addSpacing(12);

    auto *card = new RestylePanel(body);
    card->setObjectName(QStringLiteral("reportCard"));
    card->setMaximumWidth(1080);
    auto *content = new QVBoxLayout(card);
    content->setContentsMargins(21, 20, 21, 21);
    content->setSpacing(20);
    auto *form = new QHBoxLayout;
    form->setSpacing(12);
    mMonth = new QComboBox(card);
    mMonth->setObjectName(QStringLiteral("cbMonth"));
    const QLocale locale = QLocale::system();
    for (int month = 1; month <= 12; ++month)
        mMonth->addItem(locale.monthName(month, QLocale::LongFormat));
    mMonth->setCurrentIndex(QDate::currentDate().month() - 1);
    mMonth->setFixedHeight(35);
    mMonth->setFixedWidth(140);
    mMonth->setFont(Restyle::font(11));
    mYear = new QDateEdit(QDate::currentDate(), card);
    mYear->setObjectName(QStringLiteral("yearEdit"));
    mYear->setDisplayFormat(QStringLiteral("yyyy"));
    mYear->setDateRange(QDate(2000, 1, 1), QDate(2100, 12, 31));
    mYear->setFixedHeight(35);
    mYear->setFixedWidth(140);
    mYear->setFont(Restyle::font(11));
    const auto addField = [&](const QString &text, QWidget *field) {
        auto *column = new QVBoxLayout;
        column->setSpacing(7);
        auto *label = new RestyleLabel(text, 11, QFont::Normal, card);
        label->setColorRole(QStringLiteral("muted"));
        label->setBuddy(field);
        label->setFixedHeight(16);
        column->addWidget(label);
        column->addWidget(field);
        form->addLayout(column);
    };
    addField(tr("Месяц"), mMonth);
    addField(tr("Год"), mYear);
    auto *generate = new QPushButton(tr("Просмотр"), card);
    generate->setObjectName(QStringLiteral("pbGenerate"));
    generate->setIcon(Restyle::icon(QStringLiteral("report")));
    Restyle::button(generate, QStringLiteral("primary"));
    generate->setFixedHeight(32);
    generate->setMinimumWidth(qMax(95, generate->fontMetrics().horizontalAdvance(generate->text()) + 42));
    form->addWidget(generate, 0, Qt::AlignBottom);
    form->addStretch();
    content->addLayout(form);

    mResults = new QStackedWidget(card);
    mResults->setObjectName(QStringLiteral("reportResults"));
    auto *empty = new QWidget(mResults);
    auto *emptyLayout = new QVBoxLayout(empty);
    emptyLayout->setContentsMargins(20, 38, 20, 38);
    emptyLayout->setSpacing(8);
    auto *emptyTitle = new RestyleLabel(tr("Выберите период"), 13, QFont::DemiBold, empty);
    emptyTitle->setAlignment(Qt::AlignCenter);
    emptyTitle->setWordWrap(true);
    emptyTitle->setObjectName(QStringLiteral("reportEmptyTitle"));
    auto *emptyDescription = new RestyleLabel(tr("Нажмите «Просмотр», чтобы открыть отчёт."), 13, QFont::Normal, empty);
    emptyDescription->setColorRole(QStringLiteral("muted"));
    emptyDescription->setWordWrap(true);
    emptyDescription->setAlignment(Qt::AlignCenter);
    emptyDescription->setObjectName(QStringLiteral("reportEmptyDescription"));
    mEmptyTitle = emptyTitle;
    mEmptyDescription = emptyDescription;
    emptyLayout->addStretch();
    emptyLayout->addWidget(emptyTitle);
    emptyLayout->addWidget(emptyDescription);
    emptyLayout->addStretch();
    mResults->addWidget(empty);
    mTable = new QTableView(mResults);
    mTable->setObjectName(QStringLiteral("tableView"));
    mTable->setFrameShape(QFrame::NoFrame);
    mTable->setShowGrid(false);
    mTable->setAlternatingRowColors(false);
    mTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    mTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    mTable->verticalHeader()->hide();
    mTable->verticalHeader()->setDefaultSectionSize(38);
    mTable->horizontalHeader()->setMinimumHeight(31);
    mTable->horizontalHeader()->setStretchLastSection(false);
    mResults->addWidget(mTable);
    mResults->setMinimumHeight(150);
    mResults->setMaximumHeight(520);
    content->addWidget(mResults);
    page->addWidget(card);
    page->addStretch();
    scroll->setWidget(body);
    outer->addWidget(scroll);
    mCompositionsList = new CompositionsList(this);
    connect(generate, &QPushButton::clicked, this, &Report::Generate);
}

Report::~Report() { delete ui; }

void Report::setReportDirectory(const QString &directory)
{
    mReportDirectory = directory;
}

void Report::Generate()
{
    // The on-disk report filename remains the historical MMYY.csv contract.
    const QDate period(mYear->date().year(), mMonth->currentIndex() + 1, 1);
    const QString filePath = QDir(mReportDirectory).filePath(period.toString(QStringLiteral("MMyy")) + QStringLiteral(".csv"));
    mTable->setModel(nullptr);
    mResults->setMinimumHeight(150);
    if (!mCompositionsList->Init(filePath)) {
        const bool exists = QFileInfo::exists(filePath);
        mEmptyTitle->setText(exists ? tr("Не удалось открыть отчёт") : tr("За этот период отчётов нет"));
        mEmptyDescription->setText(exists
            ? tr("Проверьте доступ к файлу и формат записей отчёта.")
            : tr("Выберите другой месяц или дождитесь появления отчёта станции."));
        mEmptyDescription->setToolTip(QDir::toNativeSeparators(filePath));
        mResults->setCurrentIndex(0);
        return;
    }
    if (mCompositionsList->rowCount(QModelIndex()) == 0) {
        mEmptyTitle->setText(tr("В отчёте нет записей"));
        mEmptyDescription->setText(tr("За выбранный месяц композиции не зарегистрированы."));
        mEmptyDescription->setToolTip(QDir::toNativeSeparators(filePath));
        mResults->setCurrentIndex(0);
        return;
    }
    mTable->setModel(mCompositionsList);
    mTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    mTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    mTable->horizontalHeader()->setMinimumSectionSize(160);
    mResults->setCurrentIndex(1);
    mResults->setMinimumHeight(qMin(360, 31 + mCompositionsList->rowCount(QModelIndex()) * 38));
}
