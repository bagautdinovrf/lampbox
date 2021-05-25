

#include "settingsdialog.h"
#include "ui_settingsdialog.h"
#include "settings.h"

/// Qt
#include <QListWidgetItem>
#include <QDebug>
#include <QMap>
#include <QFileDialog>


SettingsDialog::SettingsDialog(QWidget *parent, Qt::WindowFlags f) :
    QDialog(parent, f),
    ui(new Ui::SettingsDialog)
{
    ui->setupUi(this);
    setWindowTitle("Настройки");
    ui->stackedWidget->setCurrentIndex(0);
    ui->lw_listSettings->setCurrentRow(0);

    init();

    connect( ui->pb_close,        &QPushButton::clicked, this, &SettingsDialog::close                  );
    // Типы файлов
    connect( ui->pb_selectAll,    &QPushButton::clicked, this, &SettingsDialog::selectAllFileFormats   );
    connect( ui->pb_deselectAll,  &QPushButton::clicked, this, &SettingsDialog::deselectAllFileFormats );
    // Переключение страниц в зависимости от выбранного элемента
    connect(ui->lw_listSettings, SIGNAL( currentRowChanged(int) ), this, SLOT( slot_changePage(int))   );

//     connect( this, SIGNAL( fileFormats()), parent, SLOT( changeFileFormats() ) );
}

/**
 * @brief init - Инициализация окна настроек типов файлов
 */
void SettingsDialog::init()
{
    QListWidget *lwAudio = ui->lw_AudioFileType;
    QListWidget *lwVideo = ui->lw_VideoFileType;

    Settings settings;

    auto fileFormatsAudio = settings.fileFormatsAudio();
    auto itA = fileFormatsAudio.begin();
    while ( itA != fileFormatsAudio.end() ) {
        QListWidgetItem *item = new QListWidgetItem( itA.key() , lwAudio );
        item->setFlags( item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState( itA.value() ? Qt::Checked : Qt::Unchecked );
        ++itA;
    }

    auto fileFormatsVideo = settings.fileFormatsVideo();
    auto itV = fileFormatsVideo.begin();
    while ( itV != fileFormatsVideo.end() ) {
        QListWidgetItem *item = new QListWidgetItem( itV.key() , lwVideo );
        item->setFlags( item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState( itV.value() ? Qt::Checked : Qt::Unchecked );
        ++itV;
    }

    connect( lwAudio, SIGNAL(itemChanged(QListWidgetItem*)), SLOT(checkAudioItem(QListWidgetItem*)) );
    connect( lwVideo, SIGNAL(itemChanged(QListWidgetItem*)), SLOT(checkVideoItem(QListWidgetItem*)) );
}


void SettingsDialog::checkAudioItem( QListWidgetItem * item )
{
    if( !item )
        return;

    Settings().writeFileFormatAudioValue(item->text(), item->checkState() == Qt::Checked ? true : false );
    emit fileFormatsAudio();
}


void SettingsDialog::checkVideoItem( QListWidgetItem * item )
{
    if( !item )
        return;

    Settings().writeFileFormatVideoValue(item->text(), item->checkState() == Qt::Checked ? true : false );
    emit fileFormatsVideo();
}

/**
 * @brief selectAllFileFormats                  - Выбрать все форматы
 */
void SettingsDialog::selectAllFileFormats()
{
    changeAudioTypesCheckState( Qt::Checked );
    changeVideoTypesCheckState( Qt::Checked );

}

/**
 * @brief deselectAllFileFormats                - Убрать выбор со всех форматов
 */
void SettingsDialog::deselectAllFileFormats()
{
    changeAudioTypesCheckState( Qt::Unchecked );
    changeVideoTypesCheckState( Qt::Unchecked );
}

/**
 * @brief changeFileFormatItemsCheckState       - Изменяет состояние всех итемов
 * @param state                 - состояние
 */
void SettingsDialog::changeAudioTypesCheckState( Qt::CheckState state )
{
    QListWidget *lw = ui->lw_AudioFileType;
    for( int i = 0; i < lw->count(); ++i ) {
        lw->item(i)->setCheckState( state );
    }
    emit fileFormatsAudio();
}

void SettingsDialog::changeVideoTypesCheckState( Qt::CheckState state)
{
    QListWidget *lw = ui->lw_VideoFileType;
    for( int i = 0; i < lw->count(); ++i ) {
        lw->item(i)->setCheckState( state );
    }
    emit fileFormatsVideo();
}

void SettingsDialog::slot_changePage(int page)
{
    ui->stackedWidget->setCurrentIndex(page);
}
