#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QFile>
#include <QFontInfo>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMimeData>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpression>
#include <QScreen>
#include <QScrollArea>
#include <QSettings>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QStyleOption>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeEdit>
#include <QTimer>
#include <QTreeView>

#include "channelmanager.h"
#include "projectfixture.h"
#include "mediafixture.h"
#include "mainwindow.h"
#include "mediacontroller.h"
#include "mediamodel.h"
#include "medialibrarydelegate.h"
#include "restyletheme.h"
#include "report.h"
#include "ruleeditors.h"
#include "schedulepreview.h"
#include "settings.h"
#include "stationmanager.h"

namespace {
QString fixtureRoot;

void writeFile(const QString &path, const QByteArray &contents)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(contents) != contents.size())
        qFatal("Cannot create isolated fixture file");
}

void resetStation()
{
    const QString project = fixtureRoot + "/project.json";
    if (QFileInfo::exists(project) && !QFile::remove(project))
        qFatal("Cannot reset isolated project fixture");
    // Each test owns these channels; preserve only the shared advert fixtures.
    for (const QString &kind : {QStringLiteral("music"), QStringLiteral("video")}) {
        QDir channels(fixtureRoot + "/media/" + kind);
        if (channels.exists() && !channels.removeRecursively())
            qFatal("Cannot reset isolated media fixture");
    }
    for (const QString &folder : {"media/music", "media/video", "media/ads"})
        QDir().mkpath(fixtureRoot + '/' + folder);
    // The compiled schedule starts its validity today: keep the campaign around it.
    const QDate today = QDate::currentDate();
    const QDate monthStart(today.year(), today.month(), 1);
    const QDate monthEnd = monthStart.addMonths(1).addDays(-1);
    ProjectRepository::Project fixture;
    fixture.advert = {
        ProjectFixture::advert("Осенняя коллекция.mp3", "9,10,11,12,13,14,15,16,17,18,19,20", "00m,30m",
            monthStart, monthEnd, 75),
        ProjectFixture::advert("Кофе с собой.mp3", "8,9,10,11,12", "3",
            monthStart, monthEnd, 70, {}, "1,2,3,4,5")};
    writeFile(project, ProjectRepository::encode(fixture));
}

void mediaFile(const QString &folder, const QString &title, const QString &artist,
               const QString &album, const QString &genre, int year, int seconds,
               const QString &extension = QStringLiteral("mp3"))
{
    const QString file = folder + '/' + title + '.' + extension;
    writeFile(file, extension == QStringLiteral("mp4")
        ? MediaFixture::mp4(title, artist, album, genre, year, seconds)
        : MediaFixture::mp3(title, artist, album, genre, year, seconds));
}

QByteArray stationDigest()
{
    QStringList paths;
    QDirIterator entries(fixtureRoot, QDir::Files, QDirIterator::Subdirectories);
    while (entries.hasNext()) {
        const QString path = entries.next();
        if (path != qApp->property("restylePreviewSettings").toString())
            paths << path;
    }
    paths.sort();
    QCryptographicHash digest(QCryptographicHash::Sha256);
    for (const QString &path : paths) {
        digest.addData(path.toUtf8());
        QFile file(path);
        if (file.open(QIODevice::ReadOnly))
            digest.addData(file.readAll());
    }
    return digest.result();
}

void settle()
{
    QApplication::sendPostedEvents();
    QApplication::processEvents();
    QTest::qWait(35);
}

QAbstractButton *button(QWidget &window, const QString &name)
{
    return window.findChild<QAbstractButton *>(name);
}

void choose(QComboBox *picker, const QString &id)
{
    if (!picker || picker->findData(id) < 0)
        qFatal("Missing restyle picker item");
    picker->setCurrentIndex(picker->findData(id));
    settle();
}

// The schedule panels' previewDate / previewTime are the window's only date pickers.
void planFixture(QWidget &window)
{
    for (auto *edit : window.findChildren<QDateEdit *>())
        edit->setDate(QDate::currentDate());
    for (auto *edit : window.findChildren<QTimeEdit *>())
        edit->setTime(QTime(10, 24));
    settle();
}

QPixmap composite(QWidget &window)
{
    QPixmap result = window.grab();
    QPainter painter(&result);
    for (QWidget *top : QApplication::topLevelWidgets()) {
        if (top != &window && top->isVisible() && (top->isModal() || top->windowType() == Qt::Popup))
            painter.drawPixmap(window.mapFromGlobal(top->mapToGlobal(QPoint())), top->grab());
    }
    return result;
}
}

// This friend is linked only into the verification executable. The application
// never receives example channels, fake media or a command-line preview switch.
void seedRestyleWindow(MainWindow &window)
{
    for (auto *report : window.findChildren<Report *>())
        report->setReportDirectory(fixtureRoot + "/reports");
    const QStringList musicNames = {"Мягкое_утро", "Дневной_ритм", "Ближе_к_вечеру"};
    const QStringList videoNames = {"Природа", "Вечерние_истории"};
    const QStringList titles = {"Тихий свет", "Первый кофе", "За окном", "Между строк", "Воздух", "Новый день"};
    const QStringList artists = {"Aster", "Room Service", "Маяк", "North Ensemble", "Forma", "Aster"};
    const QStringList albums = {"Soft Focus", "Everyday Rituals", "Медленно", "Still Life", "Open Spaces", "Soft Focus"};
    const QStringList genres = {"Ambient", "Downtempo", "Electronic", "Jazz", "Ambient", "Ambient"};
    const int seconds[] = {234, 218, 263, 196, 247, 221};
    for (int type = 0; type < 2; ++type) {
        ChannelManager *manager = window.mChannelManagers[type];
        const QStringList names = type == 0 ? musicNames : videoNames;
        for (int row = 0; row < names.size(); ++row) {
            window.mPages[type].source->setMediaManager(nullptr);
            const QTime start = type == 0 ? QTime(row == 0 ? 8 : row == 1 ? 12 : 17, 0)
                                          : QTime(row == 0 ? 8 : 18, 0);
            const QTime end = type == 0 ? QTime(row == 0 ? 12 : row == 1 ? 17 : 22, 0)
                                        : QTime(row == 0 ? 18 : 22, 0);
            if (!manager->createChannel({names[row], start, end, "*", "*", "*", 65}))
                qFatal("Cannot create isolated channel");
            auto &channel = manager->channel(row);
            const QString folder = fixtureRoot + "/media/" + (type == 0 ? "music/" : "video/") + names[row];
            if (type == 0) {
                for (int track = row; track < (row == 0 ? 6 : 5); ++track)
                    mediaFile(folder, titles[track], artists[track], albums[track], genres[track],
                              track == 1 ? 2024 : track == 3 ? 2023 : 2025, seconds[track]);
            } else {
                for (const QString &title : (row == 0 ? QStringList{"Линия берега", "Свет и тени", "Тихий сад"}
                                                       : QStringList{"Город в деталях"}))
                    mediaFile(folder, title, "—", "—", "—", 2026, 128, "mp4");
            }
            channel.mediaManager().collectMediaFiles();
        }
    }
    window.updatePage(0);
    window.updatePage(1);
    window.mPages[1].channels->selectRow(0);
    window.mPages[0].channels->selectRow(0);
    window.selectChannel(1, 0);
    window.selectChannel(0, 0);
    auto *files = window.mPages[0].files;
    for (int row = 0; row < files->model()->rowCount(); ++row) {
        const QModelIndex index = files->model()->index(row, 0);
        if (index.data().toString() == QStringLiteral("Тихий свет.mp3"))
            files->selectionModel()->setCurrentIndex(index,
                    QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    }
    planFixture(window);
}

class RestyleTest final : public QObject
{
    Q_OBJECT
private slots:
    void isolatedStation()
    {
        QVERIFY(QStandardPaths::isTestModeEnabled());
        QCOMPARE(StationManager::Instance().get(), fixtureRoot);
        const SPathData paths;
        QCOMPARE(paths.projectFile, fixtureRoot + "/project.json");
        QVERIFY(paths.advertDir.startsWith(fixtureRoot + '/'));
        QVERIFY(qApp->property("restylePreviewSettings").toString().startsWith(fixtureRoot + '/'));
    }

    void appearanceAndThemePreserveUiState()
    {
        resetStation();
        Restyle::apply("tide-relief", "denim");
        MainWindow window;
        seedRestyleWindow(window);
        window.resize(1440, 900);
        window.show();
        settle();
        auto *appearance = window.findChild<QComboBox *>("appearancePicker");
        auto *theme = window.findChild<QComboBox *>("themePicker");
        auto *search = window.findChild<QLineEdit *>("searchEdit");
        auto *media = window.findChild<QAbstractItemView *>("mediaTable");
        QVERIFY(appearance);
        QVERIFY(theme);
        QVERIFY(search);
        QVERIFY(media);
        QCOMPARE(appearance->count(), 2);
        QCOMPARE(theme->count(), 7);
        QVERIFY(media->model()->rowCount() >= 3);
        search->setText("свет");
        settle();
        QCOMPARE(media->model()->rowCount(), 1);
        media->selectionModel()->setCurrentIndex(media->model()->index(0, 0),
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        const QPersistentModelIndex selected(media->currentIndex());
        const QVariant selectedName = selected.data();
        const QByteArray before = stationDigest();
        const QRect searchRect = search->geometry();
        for (const QString &appearanceId : Restyle::appearanceIds()) {
            choose(appearance, appearanceId);
            QCOMPARE(Settings().themeId(), theme->currentData().toString());
            for (const QString &themeId : Restyle::themeIds()) {
                choose(theme, themeId);
                QCOMPARE(Settings().appearanceId(), appearanceId);
                QCOMPARE(Settings().themeId(), themeId);
                QCOMPARE(search->text(), QString("свет"));
                QCOMPARE(media->model()->rowCount(), 1);
                QVERIFY(selected.isValid());
                QCOMPARE(media->currentIndex(), QModelIndex(selected));
                QCOMPARE(selected.data(), selectedName);
                QCOMPARE(search->geometry(), searchRect);
                for (auto *spin : window.findChildren<QDateTimeEdit *>()) {
                    if (!spin->objectName().startsWith("preview"))
                        continue;
                    auto *field = spin->findChild<QLineEdit *>();
                    QVERIFY(field);
                    const QMargins margins = field->textMargins();
                    const int visibleWidth = field->contentsRect().width() - margins.left() - margins.right();
                    QVERIFY2(QFontMetrics(field->font()).horizontalAdvance(field->displayText()) <= visibleWidth,
                             qPrintable(spin->objectName() + ": " + field->displayText()));
                }
                QCOMPARE(stationDigest(), before);
                QVERIFY(window.findChildren<MediaController *>().isEmpty());
                QVERIFY(window.findChildren<QProcess *>().isEmpty());
            }
        }
        search->clear();
        settle();
        QVERIFY(media->model()->rowCount() >= 3);
        QCOMPARE(stationDigest(), before);
    }

    void openEditorRetainsDraftWhenThemeChanges()
    {
        RuleEditors::ChannelRuleValues values;
        values.name = "Исходный";
        RuleEditors::ChannelRuleDialog dialog(values);
        dialog.show();
        auto *name = dialog.findChild<QLineEdit *>("channelName");
        auto *days = dialog.findChild<QLineEdit *>("channelDays");
        QVERIFY(name);
        QVERIFY(days);
        name->setText("Черновик");
        days->setText("2-8,15");
        name->setSelection(1, 3);
        const QByteArray before = stationDigest();
        for (const QString &appearance : Restyle::appearanceIds()) {
            for (const QString &theme : Restyle::themeIds()) {
                Restyle::apply(appearance, theme);
                settle();
                QCOMPARE(name->text(), QString("Черновик"));
                QCOMPARE(days->text(), QString("2-8,15"));
                QCOMPARE(name->selectedText(), QString("ерн"));
                QCOMPARE(stationDigest(), before);
            }
        }
        dialog.reject();
        QCOMPARE(dialog.result(), int(QDialog::Rejected));
        QCOMPARE(stationDigest(), before);
    }

    void resizedHeadersKeepTheirTitles()
    {
        // Short cells must not squeeze the header title into «Нач…».
        const QStringList titles = {"Начало", "Месяцы", "Часы", "Когда в часе", "Громкость"};
        QStandardItemModel model(1, titles.size());
        model.setHorizontalHeaderLabels(titles);
        for (int column = 0; column < titles.size(); ++column)
            model.setItem(0, column, new QStandardItem("08:00"));
        QTableView view;
        view.setModel(&model);
        auto *header = view.horizontalHeader();
        header->setFont(Restyle::font(10));
        header->setSectionResizeMode(QHeaderView::ResizeToContents);
        view.show();
        settle();
        for (int section = 0; section < titles.size(); ++section) {
            QStyleOptionHeader option;
            option.initFrom(header);
            option.state |= QStyle::State_Horizontal;
            option.rect = QRect(0, 0, header->sectionSize(section), header->height());
            option.text = model.headerData(section, Qt::Horizontal).toString();
            // CE_HeaderLabel draws inside SE_HeaderLabel with 8 px more per side. The
            // fractional advance plus 2 px must fit: at 125 % rounding alone elides «Часы».
            const QRect label = header->style()->subElementRect(QStyle::SE_HeaderLabel, &option, header);
            QVERIFY2(QFontMetricsF(Restyle::font(10)).horizontalAdvance(option.text) + 2 <= label.width() - 16,
                     qPrintable(option.text));
        }
    }

    void cyrillicFont()
    {
        QVERIFY(Restyle::verifiedCyrillicFont());
        QVERIFY(!Restyle::fontFamily().isEmpty());
        QVERIFY(qApp->styleSheet().isEmpty());
    }

    void metadataFilterMapsSelectionToSource()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        mediaFile(directory.path(), "Короткая", "Aster", "Album", "Ambient", 2025, 9);
        mediaFile(directory.path(), "Длинная", "Маяк", "Album", "Electronic", 2026, 120);
        MediaManager manager(directory.path(), MediaBoxManager::MUSIC);
        MediaModel source(&manager, MediaBoxManager::MUSIC);
        QSortFilterProxyModel proxy;
        proxy.setSourceModel(&source);
        proxy.setSortRole(MediaModel::SortRole);
        proxy.setFilterKeyColumn(-1);
        proxy.setFilterCaseSensitivity(Qt::CaseInsensitive);
        proxy.sort(MediaModel::DurationColumn, Qt::AscendingOrder);
        QCOMPARE(proxy.rowCount(), 2);
        QCOMPARE(proxy.index(0, 0).data(MediaModel::DurationSecondsRole).toInt(), 9);
        QCOMPARE(proxy.index(1, 0).data(MediaModel::DurationSecondsRole).toInt(), 120);
        proxy.setFilterFixedString("маяк");
        QCOMPARE(proxy.rowCount(), 1);
        const QModelIndex mapped = proxy.mapToSource(proxy.index(0, 0));
        QCOMPARE(mapped.data().toString(), QString("Длинная.mp3"));
        QCOMPARE(mapped.data(MediaModel::ArtistRole).toString(), QString("Маяк"));
        QCOMPARE(proxy.mapFromSource(mapped), proxy.index(0, 0));
    }

    void sortedSelectionDeletesTheChosenFiles()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        mediaFile(directory.path(), "01_keep", "Studio", "Album", "Ambient", 2026, 150);
        mediaFile(directory.path(), "02_delete", "Live", "Album", "Ambient", 2026, 240);
        mediaFile(directory.path(), "03_delete", "Live", "Album", "Ambient", 2026, 30);
        // Display titles deliberately differ from disk identities and disk sort order.
        writeFile(directory.filePath("02_delete.mp3"),
            MediaFixture::mp3("Первое название", "Live", "Album", "Ambient", 2026, 240));
        MediaManager manager(directory.path(), MediaBoxManager::MUSIC);
        MediaModel source(&manager, MediaBoxManager::MUSIC);
        QSortFilterProxyModel proxy;
        proxy.setSourceModel(&source);
        proxy.setSortRole(MediaModel::SortRole);
        proxy.setFilterKeyColumn(-1);
        proxy.sort(MediaModel::DurationColumn, Qt::AscendingOrder);
        QItemSelectionModel selection(&proxy);
        selection.select(proxy.index(0, MediaModel::DurationColumn),
                         QItemSelectionModel::Select | QItemSelectionModel::Rows);
        const QPersistentModelIndex chosen(proxy.index(0, 0));
        QCOMPARE(chosen.data().toString(), QString("03_delete.mp3"));
        proxy.sort(MediaModel::DurationColumn, Qt::DescendingOrder);
        QVERIFY(chosen.isValid());
        QCOMPARE(chosen.row(), 2);
        QCOMPARE(selection.selectedRows().size(), 1);
        QCOMPARE(selection.selectedRows().first().data(), chosen.data());
        proxy.setFilterFixedString("Live");
        QCOMPARE(proxy.rowCount(), 2);
        QCOMPARE(selection.selectedRows().first().data(), chosen.data());
        selection.select(proxy.index(0, MediaModel::FormatColumn),
                         QItemSelectionModel::Select | QItemSelectionModel::Rows);

        QStringList fileNames;
        for (const QModelIndex &selected : selection.selectedRows()) {
            const QModelIndex mapped = proxy.mapToSource(selected);
            QCOMPARE(mapped.data(MediaModel::FileNameRole), mapped.data());
            QCOMPARE(manager.mediaData(mapped.row()).fileName(), mapped.data().toString());
            fileNames.append(mapped.data(MediaModel::FileNameRole).toString());
        }
        QCOMPARE(fileNames.size(), 2);
        // Snapshot file identities before any reset can invalidate selected rows.
        for (const QString &name : fileNames)
            QVERIFY(manager.delFile(name));
        manager.collectMediaFiles();
        QCOMPARE(source.rowCount(), 1);
        QCOMPARE(source.index(0, 0).data().toString(), QString("01_keep.mp3"));
        QVERIFY(QFileInfo::exists(directory.filePath("01_keep.mp3")));
        QVERIFY(!QFileInfo::exists(directory.filePath("02_delete.mp3")));
        QVERIFY(!QFileInfo::exists(directory.filePath("03_delete.mp3")));
        QVERIFY(!chosen.isValid());
        QVERIFY(selection.selectedRows().isEmpty());
        proxy.setFilterFixedString({});
        QCOMPARE(proxy.rowCount(), 1);
    }

    void mediaManagerLifetimesDetachModels()
    {
        QTemporaryDir first;
        QTemporaryDir second;
        QVERIFY(first.isValid());
        QVERIFY(second.isValid());
        mediaFile(first.path(), "Первый", "Artist", "Album", "Ambient", 2026, 100);
        mediaFile(second.path(), "Второй", "Artist", "Album", "Ambient", 2026, 200);
        auto firstManager = std::make_unique<MediaManager>(first.path(), MediaBoxManager::MUSIC);
        auto secondManager = std::make_unique<MediaManager>(second.path(), MediaBoxManager::MUSIC);
        MediaModel source(firstManager.get(), MediaBoxManager::MUSIC);
        source.setMediaManager(secondManager.get());
        firstManager.reset();
        QCOMPARE(source.rowCount(), 1);
        QCOMPARE(source.index(0, 0).data().toString(), QString("Второй.mp3"));
        QCOMPARE(source.rowCount(source.index(0, 0)), 0);
        QCOMPARE(source.columnCount(source.index(0, 0)), 0);
        const QPersistentModelIndex selected(source.index(0, 0));
        source.setMediaManager(secondManager.get());
        QVERIFY(selected.isValid());
        secondManager.reset();
        QCOMPARE(source.rowCount(), 0);
        QVERIFY(!selected.isValid());
        QVERIFY(!source.data(selected, Qt::DisplayRole).isValid());

        MediaManager survivingManager(first.path(), MediaBoxManager::MUSIC);
        {
            MediaModel temporaryModel(&survivingManager, MediaBoxManager::MUSIC);
            QCOMPARE(temporaryModel.rowCount(), 1);
        }
        // Destroying a view/model first must not leave reset callbacks dangling.
        survivingManager.collectMediaFiles();
        QCOMPARE(survivingManager.mediaCount(), 1);
    }

    void fileDropsKeepLocalCopySemantics()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        const QString file = directory.filePath("Файл.mp3");
        writeFile(file, "isolated fixture");
        MediaModel source(nullptr, MediaBoxManager::MUSIC);
        QSignalSpy dropped(&source, &MediaModel::dropFileList);
        QMimeData remote;
        remote.setUrls({QUrl("https://example.invalid/remote.mp3")});
        QVERIFY(!source.canDropMimeData(&remote, Qt::CopyAction, -1, -1, {}));
        QVERIFY(!source.dropMimeData(&remote, Qt::CopyAction, -1, -1, {}));
        QVERIFY(!source.canDropMimeData(nullptr, Qt::CopyAction, -1, -1, {}));
        QCOMPARE(dropped.size(), 0);

        QMimeData local;
        local.setUrls({QUrl::fromLocalFile(file), QUrl::fromLocalFile(directory.path()),
                       QUrl::fromLocalFile(file), remote.urls().first()});
        QVERIFY(!source.dropMimeData(&local, Qt::MoveAction, -1, -1, {}));
        QVERIFY(source.dropMimeData(&local, Qt::IgnoreAction, -1, -1, {}));
        QCOMPARE(dropped.size(), 0);
        QVERIFY(source.dropMimeData(&local, Qt::CopyAction, -1, -1, {}));
        QCOMPARE(dropped.size(), 1);
        QCOMPARE(dropped.takeFirst().first().toStringList(), QStringList({file, directory.path()}));
        QVERIFY(QFileInfo::exists(file));
    }

    void mediaCheckboxesUseNativeSelection()
    {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());
        mediaFile(directory.path(), "Первый", "Artist", "Album", "Ambient", 2026, 100);
        mediaFile(directory.path(), "Второй", "Artist", "Album", "Ambient", 2026, 200);
        MediaManager manager(directory.path(), MediaBoxManager::MUSIC);
        MediaModel model(&manager, MediaBoxManager::MUSIC);
        QTreeView view;
        view.setRootIsDecorated(false);
        view.setModel(&model);
        view.setSelectionMode(QAbstractItemView::ExtendedSelection);
        view.setSelectionBehavior(QAbstractItemView::SelectRows);
        view.setEditTriggers(QAbstractItemView::NoEditTriggers);
        auto *delegate = new MediaLibraryDelegate(&view);
        view.setItemDelegate(delegate);
        for (int column = 1; column < MediaModel::ColumnCount; ++column)
            if (column != MediaModel::DurationColumn)
                view.hideColumn(column);
        view.setColumnWidth(MediaModel::FileNameColumn, 290);
        view.resize(500, 180);
        view.show();
        settle();
        const auto first = model.index(0, 0);
        const auto second = model.index(1, 0);
        auto checkbox = [&](const QModelIndex &index) {
            const QRect rect = view.visualRect(index);
            return QPoint(rect.left() + 14, rect.center().y());
        };
        view.selectionModel()->select(first, QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, checkbox(second));
        QCOMPARE(view.selectionModel()->selectedRows().size(), 2);
        QCOMPARE(view.currentIndex(), second);
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, checkbox(first));
        QCOMPARE(view.selectionModel()->selectedRows(), QModelIndexList{second});
        QCOMPARE(view.currentIndex(), first);
        QTest::keyClick(&view, Qt::Key_Space);
        QCOMPARE(view.selectionModel()->selectedRows().size(), 2);

        QSignalSpy doubleClicks(&view, &QTreeView::doubleClicked);
        QTest::mouseDClick(view.viewport(), Qt::LeftButton, Qt::NoModifier, checkbox(first));
        QTest::mouseRelease(view.viewport(), Qt::LeftButton, Qt::NoModifier, checkbox(first));
        QCOMPARE(doubleClicks.size(), 0);
        QCOMPARE(view.selectionModel()->selectedRows().size(), 2);
        QSignalSpy actions(delegate, &MediaLibraryDelegate::fileActionsRequested);
        const QRect duration = view.visualRect(second.siblingAtColumn(MediaModel::DurationColumn));
        QTest::mouseClick(view.viewport(), Qt::LeftButton, Qt::NoModifier,
                          QPoint(duration.right() - 22, duration.center().y()));
        QCOMPARE(actions.size(), 1);
        QCOMPARE(qvariant_cast<QModelIndex>(actions.first().first()), second);
        QCOMPARE(view.selectionModel()->selectedRows().size(), 2);
        view.selectionModel()->setCurrentIndex(second, QItemSelectionModel::NoUpdate);
        QContextMenuEvent keyboardMenu(QContextMenuEvent::Keyboard, QPoint(), QPoint());
        QApplication::sendEvent(&view, &keyboardMenu);
        QCOMPARE(actions.size(), 2);
        QCOMPARE(qvariant_cast<QModelIndex>(actions.last().first()), second);
    }

    void importsIntoActiveChannelAndCancelKeepsFile()
    {
        resetStation();
        MainWindow window;
        seedRestyleWindow(window);
        window.show();
        settle();
        QVERIFY(window.findChildren<MediaController *>().isEmpty());
        auto *channels = window.findChild<QTableView *>("channelList");
        auto *files = window.findChild<QTreeView *>("mediaTable");
        auto *search = window.findChild<QLineEdit *>("searchEdit");
        QVERIFY(channels);
        QVERIFY(files);
        QVERIFY(search);
        QVERIFY(channels->model()->rowCount() >= 2);
        channels->selectRow(1);
        settle();
        const QString activeChannel = channels->model()->index(1, 0).data().toString();
        const QString otherChannel = channels->model()->index(0, 0).data().toString();
        const int originalFileCount = files->model()->rowCount();

        QTemporaryDir input;
        QVERIFY(input.isValid());
        const QString fileName = QStringLiteral("Проверка_импорта.mp3");
        const QString source = input.filePath(fileName);
        // Small MPEG frame fixture: only import and file identity are exercised;
        // the isolated window deliberately has no playback controller.
        QByteArray payload;
        for (int frame = 0; frame < 50; ++frame)
            payload += QByteArray::fromHex("fffb9064") + QByteArray(413, '\0');
        writeFile(source, payload);
        const QString destination = fixtureRoot + "/media/music/" + activeChannel + '/' + fileName;
        const QString otherDestination = fixtureRoot + "/media/music/" + otherChannel + '/' + fileName;
        QVERIFY(!QFileInfo::exists(destination));
        QVERIFY(QMetaObject::invokeMethod(&window, "copyFiles", Qt::DirectConnection,
                                          Q_ARG(QStringList, QStringList{source})));
        QTRY_COMPARE(files->model()->rowCount(), originalFileCount + 1);
        QVERIFY(QFileInfo::exists(source));
        QVERIFY(QFileInfo::exists(destination));
        QVERIFY(!QFileInfo::exists(otherDestination));
        {
            QFile imported(destination);
            QVERIFY(imported.open(QIODevice::ReadOnly));
            QCOMPARE(imported.readAll(), payload);
        }

        search->setText(fileName);
        QCOMPARE(files->model()->rowCount(), 1);
        const QModelIndex importedRow = files->model()->index(0, MediaModel::FileNameColumn);
        QCOMPARE(importedRow.data(MediaModel::FileNameRole).toString(), fileName);
        files->selectionModel()->setCurrentIndex(importedRow,
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        bool sawDeleteConfirmation = false;
        QTimer cancelConfirmation;
        cancelConfirmation.setInterval(10);
        connect(&cancelConfirmation, &QTimer::timeout, &window, [&] {
            for (QWidget *top : QApplication::topLevelWidgets()) {
                auto *dialog = qobject_cast<QMessageBox *>(top);
                if (!dialog || !dialog->isVisible() || dialog->parentWidget() != &window)
                    continue;
                sawDeleteConfirmation = dialog->windowTitle() == QStringLiteral("Удалить файлы")
                    && dialog->text().contains(fileName)
                    && dialog->standardButtons().testFlag(QMessageBox::Cancel);
                if (auto *cancel = dialog->button(QMessageBox::Cancel))
                    cancel->click();
                else
                    dialog->reject();
            }
        });
        cancelConfirmation.start();
        QVERIFY(QMetaObject::invokeMethod(&window, "slot_removeMediaFiles", Qt::DirectConnection));
        cancelConfirmation.stop();
        QVERIFY(sawDeleteConfirmation);
        QVERIFY(QFileInfo::exists(destination));
        QCOMPARE(files->model()->rowCount(), 1);
        QCOMPARE(files->model()->index(0, 0).data(MediaModel::FileNameRole).toString(), fileName);
        QVERIFY(window.findChildren<MediaController *>().isEmpty());
        QVERIFY(window.findChildren<QProcess *>().isEmpty());
    }

    void narrowScheduleHeadingHidesWeekStrip()
    {
        resetStation();
        MainWindow window;
        seedRestyleWindow(window);
        window.resize(1440, 900);
        window.show();
        settle();
        auto *panel = window.findChild<SchedulePreviewWidget *>("schedulePanel");
        QVERIFY(panel);
        auto *week = panel->findChild<QWidget *>("previewWeek");
        auto *next = panel->findChild<QWidget *>("nextDayButton");
        auto *today = panel->findChild<QWidget *>("todayButton");
        QVERIFY(week);
        QVERIFY(next);
        QVERIFY(today);
        QVERIFY(week->isVisible());
        // Below 1100 px the page keeps its fixed minimum width: the strip gives way.
        window.resize(1024, 700);
        settle();
        QVERIFY(!week->isVisible());
        const QRect nextRect(next->mapTo(panel, QPoint()), next->size());
        const QRect todayRect(today->mapTo(panel, QPoint()), today->size());
        QVERIFY(!nextRect.intersects(todayRect));
        QVERIFY(todayRect.right() < panel->width());
        window.resize(1440, 900);
        settle();
        QVERIFY(week->isVisible());
    }

    void keyboardShortcuts()
    {
        // The media table loses focus to dialogs here; its delegate must not treat it as an editor.
        QTest::failOnWarning(QRegularExpression("does not belong to this view"));
        resetStation();
        MainWindow window;
        seedRestyleWindow(window);
        window.resize(1440, 900);
        window.show();
        window.activateWindow();
        QVERIFY(QTest::qWaitForWindowActive(&window));
        settle();
        const QByteArray before = stationDigest();
        auto *stack = window.findChild<QStackedWidget *>("pageStack");
        auto *search = window.findChild<QLineEdit *>("searchEdit");
        auto *videoSearch = window.findChild<QLineEdit *>("searchEdit_video");
        QVERIFY(stack);
        QVERIFY(search);
        QVERIFY(videoSearch);
        search->setText("свет");
        QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
        QCOMPARE(QApplication::focusWidget(), search);
        QCOMPARE(search->selectedText(), QString("свет"));
        search->clear();

        QTest::keyClick(&window, Qt::Key_2, Qt::ControlModifier);
        QCOMPARE(stack->currentIndex(), 1);
        QVERIFY(button(window, "nav_video")->isChecked());
        QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
        QCOMPARE(QApplication::focusWidget(), videoSearch);
        QTest::keyClick(&window, Qt::Key_Comma, Qt::ControlModifier);
        QCOMPARE(stack->currentIndex(), 4);
        // Library shortcuts stay with the media pages.
        QTest::keyClick(&window, Qt::Key_F, Qt::ControlModifier);
        QVERIFY(!search->hasFocus());
        QVERIFY(!videoSearch->hasFocus());
        QTest::keyClick(&window, Qt::Key_1, Qt::ControlModifier);
        QCOMPARE(stack->currentIndex(), 0);
        settle();

        // Arrows switch the day from the timeline; the time field keeps them.
        auto *panel = window.findChild<SchedulePreviewWidget *>("schedulePanel");
        QVERIFY(panel);
        auto *timeline = panel->findChild<QWidget *>("scheduleDocumentTimeline");
        auto *time = panel->findChild<QTimeEdit *>("previewTime");
        QVERIFY(timeline);
        QVERIFY(time);
        const QDate today = QDate::currentDate();
        QCOMPARE(panel->previewDateTime().date(), today);
        timeline->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(timeline, Qt::Key_Right);
        QCOMPARE(panel->previewDateTime().date(), today.addDays(1));
        QTest::keyClick(timeline, Qt::Key_Left);
        QTest::keyClick(timeline, Qt::Key_Left);
        QCOMPARE(panel->previewDateTime().date(), today.addDays(-1));
        time->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(time, Qt::Key_Right);
        QCOMPARE(panel->previewDateTime().date(), today.addDays(-1));
        timeline->setFocus(Qt::OtherFocusReason);
        QTest::keyClick(timeline, Qt::Key_Home);
        QCOMPARE(panel->previewDateTime().date(), today);

        // F2, Enter and Del act on the focused list; deleting still asks first.
        QStringList dialogs;
        QTimer closer;
        closer.setInterval(10);
        connect(&closer, &QTimer::timeout, &window, [&] {
            for (QWidget *top : QApplication::topLevelWidgets()) {
                auto *dialog = qobject_cast<QDialog *>(top);
                if (!dialog || !dialog->isVisible() || !dialog->isModal())
                    continue;
                dialogs << (dialog->objectName().isEmpty() ? dialog->windowTitle() : dialog->objectName());
                dialog->reject();
            }
        });
        auto *channels = window.findChild<QTableView *>("channelList");
        auto *files = window.findChild<QTreeView *>("mediaTable");
        QVERIFY(channels);
        QVERIFY(files);
        const int channelCount = channels->model()->rowCount();
        const int fileCount = files->model()->rowCount();
        files->selectionModel()->setCurrentIndex(files->model()->index(0, 0),
                QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        // A closed modal dialog hands activation back asynchronously.
        const auto press = [&](QWidget *target, Qt::Key key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
            window.activateWindow();
            if (!QTest::qWaitForWindowActive(&window))
                return false;
            target->setFocus(Qt::OtherFocusReason);
            QTest::keyClick(target, key, modifiers);
            return true;
        };
        closer.start();
        QVERIFY(press(channels, Qt::Key_F2));
        QVERIFY(press(channels, Qt::Key_Return));
        QVERIFY(press(channels, Qt::Key_Delete));
        QVERIFY(press(files, Qt::Key_Delete));
        QVERIFY(press(files, Qt::Key_N, Qt::ControlModifier));
        closer.stop();
        QCOMPARE(dialogs, QStringList({"channelRuleDialog", "channelRuleDialog", "Удалить канал",
                                       "Удалить файлы", "channelRuleDialog"}));
        QCOMPARE(channels->model()->rowCount(), channelCount);
        QCOMPARE(files->model()->rowCount(), fileCount);
        QCOMPARE(stationDigest(), before);
    }
};

int capture(MainWindow &window, const QString &directory, bool smoke)
{
    if (!QDir().mkpath(directory))
        return 2;
    // The roll-out effect would finish after hidePopup() and leave the theme list on later shots.
    QApplication::setEffectEnabled(Qt::UI_AnimateCombo, false);
    QJsonArray captures;
    auto save = [&](const QString &name) {
        settle();
        const QPixmap pixmap = composite(window);
        if (!pixmap.save(directory + '/' + name + ".png"))
            qFatal("Cannot save preview image");
        QJsonObject item{{"id", name}, {"width", window.width()}, {"height", window.height()},
                         {"pixelWidth", pixmap.width()}, {"pixelHeight", pixmap.height()},
                         {"dpr", pixmap.devicePixelRatio()}, {"appearance", Restyle::appearanceId()},
                         {"theme", Restyle::themeId()}};
        QJsonObject geometry;
        for (auto *widget : window.findChildren<QWidget *>()) {
            if (widget->objectName().isEmpty() || !widget->isVisible())
                continue;
            const QPoint point = widget->mapTo(&window, QPoint());
            geometry.insert(widget->objectName(), QJsonArray{point.x(), point.y(), widget->width(), widget->height()});
        }
        item.insert("geometry", geometry);
        const QRect available = window.screen()->availableGeometry();
        const QRect frame = window.frameGeometry();
        item.insert("frame", QJsonArray{frame.x(), frame.y(), frame.width(), frame.height()});
        item.insert("fitsAvailableScreen", frame.width() <= available.width() && frame.height() <= available.height());
        QJsonArray dialogs;
        for (auto *top : QApplication::topLevelWidgets()) {
            if (top == &window || !top->isVisible() || !top->isWindow())
                continue;
            const QRect bounds = top->frameGeometry();
            dialogs.append(QJsonObject{{"objectName", top->objectName()},
                    {"frame", QJsonArray{bounds.x(), bounds.y(), bounds.width(), bounds.height()}},
                    {"fitsAvailableScreen", top->screen()->availableGeometry().contains(bounds)}});
        }
        item.insert("floatingWindows", dialogs);
        captures.append(item);
    };
    auto click = [&](const QString &name) {
        auto *control = button(window, name);
        if (!control)
            qFatal("Missing preview action: %s", qPrintable(name));
        control->click();
        settle();
    };
    auto modal = [&](const QString &action, const QString &name) {
        QTimer::singleShot(100, &window, [&] {
            save(name);
            for (auto *top : QApplication::topLevelWidgets())
                if (auto *dialog = qobject_cast<QDialog *>(top); dialog && dialog->isVisible())
                    dialog->reject();
        });
        click(action);
    };
    auto *appearance = window.findChild<QComboBox *>("appearancePicker");
    auto *theme = window.findChild<QComboBox *>("themePicker");
    const QSize normalSize = smoke ? window.size() : QSize(1440, 900);
    window.resize(normalSize);
    window.show();
    settle();
    for (const QString &appearanceId : Restyle::appearanceIds()) {
        choose(appearance, appearanceId);
        for (const QString &themeId : (smoke ? QStringList{"denim", "dark"} : Restyle::themeIds())) {
            choose(theme, themeId);
            const QString prefix = appearanceId + '-' + themeId + '-';
            save(prefix + "music-" + QString::number(window.width()) + 'x' + QString::number(window.height()));
            if (smoke)
                modal("editChannelButton", prefix + "channel-editor-screen-fit");
        }
        if (smoke)
            continue;
        for (const QString &themeId : {QString("denim"), QString("dark")}) {
            choose(theme, themeId);
            const QString prefix = appearanceId + '-' + themeId + '-';
            window.resize(1600, 1000);
            save(prefix + "music-1600x1000");
            window.resize(1440, 900);
            for (const QString &page : {"video", "advert", "reports", "settings", "about"}) {
                click("nav_" + page);
                save(prefix + page + "-1440x900");
            }
            click("nav_advert");
            auto *adverts = window.findChild<QTableView *>("advertTable");
            for (int row = 0; row < 2; ++row) {
                adverts->selectRow(row);
                settle();
                QTimer::singleShot(100, &window, [&] {
                    save(prefix + (row == 0 ? "ad-exact-1440x900" : "ad-frequency-1440x900"));
                    for (auto *top : QApplication::topLevelWidgets())
                        if (auto *dialog = qobject_cast<QDialog *>(top); dialog && dialog->isVisible())
                            dialog->reject();
                });
                window.openAdvertEditor();
            }
            click("nav_music");
            modal("editChannelButton", prefix + "channel-editor-1440x900");
            modal("allSchedulesButton", prefix + "all-schedules-1440x900");
            modal("fileInfoButton", prefix + "file-info-1440x900");
            modal("stationInfoButton", prefix + "station-1440x900");
            modal("deleteFilesButton", prefix + "delete-confirm-1440x900");
            QTimer::singleShot(100, &window, [&] {
                save(prefix + "plan-details-1440x900");
                for (auto *top : QApplication::topLevelWidgets())
                    if (auto *dialog = qobject_cast<QDialog *>(top); dialog && dialog->isVisible())
                        dialog->reject();
            });
            window.findChild<SchedulePreviewWidget *>("schedulePanel")->showConditions();
            // The theme picker lives in the «Оформление» card, below the fold of the settings page.
            click("nav_settings");
            for (QWidget *parent = theme->parentWidget(); parent; parent = parent->parentWidget())
                if (auto *area = qobject_cast<QScrollArea *>(parent)) {
                    area->ensureWidgetVisible(theme);
                    break;
                }
            settle();
            theme->showPopup();
            save(prefix + "theme-popup-1440x900");
            theme->hidePopup();
            click("nav_music");
            // 1280×680 is a 1080p screen at 150 %; below 1100 px the layout is compact.
            for (const QSize size : {QSize(1366, 768), QSize(1280, 680), QSize(1024, 700)}) {
                window.resize(size);
                save(prefix + "music-" + QString::number(size.width()) + 'x' + QString::number(size.height()));
            }
            window.resize(1440, 900);
        }
    }
    const QRect available = qApp->primaryScreen()->availableGeometry();
    const QJsonObject report{{"captures", captures}, {"fontFamily", Restyle::fontFamily()},
                             {"cyrillicVerified", Restyle::verifiedCyrillicFont()},
                             {"platform", QGuiApplication::platformName()},
                             {"availableGeometry", QJsonArray{available.x(), available.y(), available.width(), available.height()}},
                             {"fixture", "Temporary station; current project channel rules; in-file metadata; player controller absent"},
                             {"limitations", "Offscreen Qt captures are not Windows physical-screen acceptance. Segoe UI is not redistributed; the installed fallback font is recorded."}};
    writeFile(directory + "/capture.json", QJsonDocument(report).toJson());
    return 0;
}

int captureSchedule(MainWindow &window, const QString &directory)
{
    if (!QDir().mkpath(directory)) return 2;
    if (!Restyle::verifiedCyrillicFont())
        qFatal("Schedule capture requires a verified Cyrillic font");
    auto *appearance = window.findChild<QComboBox *>("appearancePicker");
    auto *theme = window.findChild<QComboBox *>("themePicker");
    choose(appearance, "tide-relief");
    choose(theme, "berry");
    window.resize(1440, 900);
    window.show();
    settle();
    auto *panel = window.findChild<SchedulePreviewWidget *>("schedulePanel");
    auto *table = panel ? panel->findChild<QTableView *>("scheduleDocumentTable") : nullptr;
    auto *timeline = panel ? panel->findChild<QWidget *>("scheduleDocumentTimeline") : nullptr;
    auto *settings = button(window, "scheduleProjectButton");
    if (!panel || !table || !timeline || !settings)
        qFatal("Missing schedule capture controls");
    const QDate date = QDate::currentDate();
    panel->setPreviewDateTime(QDateTime(date, QTime(10, 24)));
    settle();
    if (panel->snapshot().hasUnresolvedRules)
        qFatal("Schedule fixture has no valid plan: %s", qPrintable(panel->snapshot().issues.join('\n')));

    QJsonArray captures;
    auto save = [&](const QString &name) {
        settle();
        const QPixmap pixmap = composite(window);
        if (!pixmap.save(directory + '/' + name + ".png"))
            qFatal("Cannot save schedule preview image");
        QJsonArray rows;
        int visibleRowCount = 0;
        for (int row = 0; row < table->model()->rowCount(); ++row) {
            QJsonArray cells;
            for (int column = 0; column < table->model()->columnCount(); ++column)
                cells.append(table->model()->index(row, column).data().toString());
            rows.append(cells);
            if (table->isVisible() && !table->isRowHidden(row))
                ++visibleRowCount;
        }
        QJsonObject item{{"id", name}, {"width", window.width()}, {"height", window.height()},
            {"pixelWidth", pixmap.width()}, {"pixelHeight", pixmap.height()}, {"dpr", pixmap.devicePixelRatio()},
            {"appearance", Restyle::appearanceId()}, {"theme", Restyle::themeId()},
            {"tableVisible", table->isVisible()}, {"visibleRowCount", visibleRowCount},
            {"date", panel->previewDateTime().date().toString(Qt::ISODate)},
            {"time", panel->previewDateTime().time().toString("HH:mm")},
            {"summary", panel->snapshot().currentSummary}, {"rows", rows},
            {"timelineAccessibleName", timeline->accessibleName()}};
        if (timeline->property("laneNames").isValid())
            item.insert("laneNames", QJsonArray::fromStringList(timeline->property("laneNames").toStringList()));
        QJsonArray dialogs;
        for (auto *top : QApplication::topLevelWidgets()) {
            if (auto *dialog = qobject_cast<QDialog *>(top); dialog && dialog->isVisible()) {
                const auto bounds = dialog->frameGeometry();
                dialogs.append(QJsonObject{{"objectName", dialog->objectName()},
                    {"width", dialog->width()}, {"height", dialog->height()},
                    {"fitsAvailableScreen", dialog->screen()->availableGeometry().contains(bounds)}});
            }
        }
        item.insert("dialogs", dialogs);
        captures.append(item);
    };
    auto saveVariants = [&](const QString &prefix) {
        choose(appearance, "tide-relief");
        for (const QString &themeId : {QStringLiteral("berry"), QStringLiteral("denim"), QStringLiteral("dark")}) {
            choose(theme, themeId);
            save(prefix + "-tide-relief-" + themeId);
        }
        choose(appearance, "tide");
        choose(theme, "berry");
        save(prefix + "-tide-berry");
        choose(appearance, "tide-relief");
    };
    saveVariants("01-channel-lanes");
    bool saved = false;
    QTimer::singleShot(100, &window, [&] {
        auto *dialog = window.findChild<QDialog *>("scheduledDocumentDialog");
        auto *name = dialog ? dialog->findChild<QLineEdit *>("scheduleHolidayName") : nullptr;
        auto *playlist = dialog ? dialog->findChild<QComboBox *>("scheduleHolidayPlaylist") : nullptr;
        auto *from = dialog ? dialog->findChild<QDateEdit *>("scheduleHolidayFrom") : nullptr;
        auto *until = dialog ? dialog->findChild<QDateEdit *>("scheduleHolidayUntil") : nullptr;
        auto *add = dialog ? button(*dialog, "scheduleHolidayAdd") : nullptr;
        auto *rules = dialog ? dialog->findChild<QListWidget *>("scheduleHolidayRules") : nullptr;
        auto *buttons = dialog ? dialog->findChild<QDialogButtonBox *>() : nullptr;
        if (!dialog || !name || !playlist || !from || !until || !add || !rules || !buttons)
            qFatal("Missing schedule settings form controls");
        const int selected = playlist->findText(QStringLiteral("Дневной_ритм"));
        if (selected < 0) qFatal("Missing additional playlist in schedule fixture");
        name->setText(QStringLiteral("Праздничная программа"));
        playlist->setCurrentIndex(selected);
        from->setDate(date);
        until->setDate(date.addDays(6));
        save("02-schedule-settings");
        QTest::mouseClick(add, Qt::LeftButton);
        settle();
        if (rules->count() != 1)
            qFatal("Schedule form did not add the alternating playlist rule");
        auto *saveButton = buttons->button(QDialogButtonBox::Save);
        if (!saveButton || !saveButton->isEnabled())
            qFatal("Schedule save action is unavailable");
        QTest::mouseClick(saveButton, Qt::LeftButton);
        saved = dialog->result() == QDialog::Accepted;
        if (!saved) qFatal("Schedule settings were not accepted");
    });
    QTest::mouseClick(settings, Qt::LeftButton);
    settle();
    if (!saved || panel->snapshot().hasUnresolvedRules)
        qFatal("Updated schedule was not applied: %s", qPrintable(panel->snapshot().issues.join('\n')));
    if (panel->findChild<QTableView *>("scheduleDocumentTable") != table
            || panel->findChild<QWidget *>("scheduleDocumentTimeline") != timeline)
        qFatal("Adding a schedule rule replaced the schedule grid");
    if (!panel->snapshot().currentSummary.contains(QStringLiteral(" → ")))
        qFatal("Alternation is missing from the captured schedule plan");
    saveVariants("03-channel-lanes-with-alternation");
    const QJsonObject report{{"captures", captures}, {"fontFamily", Restyle::fontFamily()},
        {"cyrillicVerified", Restyle::verifiedCyrillicFont()}, {"platform", QGuiApplication::platformName()},
        {"sameGridAfterSaving", true}, {"fixture", "Temporary isolated station; rule added and saved through schedule settings"}};
    writeFile(directory + "/capture.json", QJsonDocument(report).toJson());
    return 0;
}

int main(int argc, char **argv)
{
    QApplication application(argc, argv);
    QCoreApplication::setApplicationName("MediaBoxManagerRestyleTest");
    QCoreApplication::setApplicationVersion(VERSION);
    QStandardPaths::setTestModeEnabled(true);
    QTemporaryDir directory;
    if (!directory.isValid())
        return 2;
    fixtureRoot = directory.path();
    application.setProperty("restylePreviewStation", fixtureRoot);
    application.setProperty("restylePreviewSettings", fixtureRoot + "/manager.conf");
    writeFile(fixtureRoot + "/mediabox.conf", "[mediastation]\nmediabox_id=-1\nmediabox_name=Preview\nmedia=media\ncrondir=cron\n");
    resetStation();
    mediaFile(fixtureRoot + "/media/ads", "Осенняя коллекция", "Студия бренда", "Октябрь", "Реклама", 2026, 25);
    mediaFile(fixtureRoot + "/media/ads", "Кофе с собой", "Студия бренда", "Октябрь", "Реклама", 2026, 18);
    mediaFile(fixtureRoot + "/media/ads", "Добро пожаловать", "Студия бренда", "Навигация", "Реклама", 2026, 12);
    Restyle::install(application);
    const QStringList arguments = application.arguments();
    if (arguments.size() == 2 && arguments[1] == "--preview") {
        MainWindow window;
        seedRestyleWindow(window);
        window.show();
        return application.exec();
    }
    if (arguments.size() == 3 && arguments[1] == "--capture-schedule") {
        MainWindow window;
        seedRestyleWindow(window);
        return captureSchedule(window, QDir(arguments[2]).absolutePath());
    }
    if (arguments.size() == 3 && (arguments[1] == "--capture" || arguments[1] == "--capture-smoke")) {
        MainWindow window;
        seedRestyleWindow(window);
        return capture(window, QDir(arguments[2]).absolutePath(), arguments[1] == "--capture-smoke");
    }
    RestyleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_restyle.moc"
