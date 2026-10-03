#include <QAbstractButton>
#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QCryptographicHash>
#include <QDateEdit>
#include <QDateTimeEdit>
#include <QDialog>
#include <QDirIterator>
#include <QFile>
#include <QFontInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QListView>
#include <QMimeData>
#include <QMessageBox>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QSettings>
#include <QSignalSpy>
#include <QSortFilterProxyModel>
#include <QStandardPaths>
#include <QTableView>
#include <QTemporaryDir>
#include <QTest>
#include <QTimeEdit>
#include <QTimer>
#include <QTreeView>

#include "channelmanager.h"
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
    for (const QString &folder : {"timetable", "media/music", "media/video", "media/ads", "cron"})
        QDir().mkpath(fixtureRoot + '/' + folder);
    writeFile(fixtureRoot + "/timetable/timetable", {});
    writeFile(fixtureRoot + "/timetable/vtimetable", {});
    writeFile(fixtureRoot + "/timetable/advertView",
              QStringLiteral("Осенняя коллекция.mp3;9,10,11,12,13,14,15,16,17,18,19,20;00m,30m;*;01.10.2026;31.10.2026;75\n"
                             "Кофе с собой.mp3;8,9,10,11,12;3;1,2,3,4,5;01.10.2026;31.10.2026;70\n").toUtf8());
}

void mediaFile(const QString &folder, const QString &title, const QString &artist,
               const QString &album, const QString &genre, int year, int seconds,
               const QString &extension = QStringLiteral("mp3"))
{
    const QString file = folder + '/' + title + '.' + extension;
    writeFile(file, QByteArray(1024, '\0'));
    QSettings tags(file + ".tag", QSettings::IniFormat);
    tags.setValue("title", title);
    tags.setValue("artist", artist);
    tags.setValue("album", album);
    tags.setValue("genre", genre);
    tags.setValue("year", year);
    tags.setValue("length", seconds);
    tags.sync();
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

void planFixture(QWidget &window)
{
    for (auto *edit : window.findChildren<QDateTimeEdit *>())
        if (edit->objectName().startsWith("planDateTime"))
            edit->setDateTime(QDateTime(QDate(2026, 10, 3), QTime(10, 24)));
    for (auto *edit : window.findChildren<QDateEdit *>())
        edit->setDate(QDate(2026, 10, 3));
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
            if (!manager->createChannel())
                qFatal("Cannot create isolated channel");
            auto &channel = manager->channel(row);
            channel.mediaManager().renameChannelDir(names[row]);
            channel.setChannelName(names[row]);
            channel.setStartTime(type == 0 ? QTime(row == 0 ? 8 : row == 1 ? 12 : 17, 0)
                                          : QTime(row == 0 ? 8 : 18, 0));
            channel.setEndTime(type == 0 ? QTime(row == 0 ? 12 : row == 1 ? 17 : 22, 0)
                                        : QTime(row == 0 ? 18 : 22, 0));
            channel.setVolume(65);
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
        manager->saveChannels();
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
        QVERIFY(paths.cronPath.startsWith(fixtureRoot + '/'));
        QVERIFY(paths.advertTask.startsWith(fixtureRoot + '/'));
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
                    if (!spin->objectName().startsWith("plan"))
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
        MediaManager manager(directory.path(), LampBox::MUSIC);
        MediaModel source(&manager, LampBox::MUSIC);
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
        {
            QSettings tags(directory.filePath("02_delete.mp3.tag"), QSettings::IniFormat);
            tags.setValue("title", "Первое название");
        }
        MediaManager manager(directory.path(), LampBox::MUSIC);
        MediaModel source(&manager, LampBox::MUSIC);
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
        auto firstManager = std::make_unique<MediaManager>(first.path(), LampBox::MUSIC);
        auto secondManager = std::make_unique<MediaManager>(second.path(), LampBox::MUSIC);
        MediaModel source(firstManager.get(), LampBox::MUSIC);
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

        MediaManager survivingManager(first.path(), LampBox::MUSIC);
        {
            MediaModel temporaryModel(&survivingManager, LampBox::MUSIC);
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
        MediaModel source(nullptr, LampBox::MUSIC);
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
        MediaManager manager(directory.path(), LampBox::MUSIC);
        MediaModel model(&manager, LampBox::MUSIC);
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
        QCOMPARE(files->model()->rowCount(), originalFileCount + 1);
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
};

int capture(MainWindow &window, const QString &directory, bool smoke)
{
    if (!QDir().mkpath(directory))
        return 2;
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
            theme->showPopup();
            save(prefix + "theme-popup-1440x900");
            theme->hidePopup();
            window.resize(1366, 768);
            save(prefix + "music-1366x768");
            window.resize(1440, 900);
        }
    }
    const QRect available = qApp->primaryScreen()->availableGeometry();
    const QJsonObject report{{"captures", captures}, {"fontFamily", Restyle::fontFamily()},
                             {"cyrillicVerified", Restyle::verifiedCyrillicFont()},
                             {"platform", QGuiApplication::platformName()},
                             {"availableGeometry", QJsonArray{available.x(), available.y(), available.width(), available.height()}},
                             {"fixture", "Temporary station; legacy-safe underscore channel names; metadata sidecars; player controller absent"},
                             {"limitations", "Offscreen Qt captures are not Windows physical-screen acceptance. Segoe UI is not redistributed; the installed fallback font is recorded."}};
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
    if (arguments.size() == 3 && (arguments[1] == "--capture" || arguments[1] == "--capture-smoke")) {
        MainWindow window;
        seedRestyleWindow(window);
        return capture(window, QDir(arguments[2]).absolutePath(), arguments[1] == "--capture-smoke");
    }
    RestyleTest test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_restyle.moc"
