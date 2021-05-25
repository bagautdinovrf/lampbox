#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>


class QListWidgetItem;

namespace Ui {
    class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent, Qt::WindowFlags f = 0);

private:
    /**
     * @brief init                              - Инициализация окна настроек типов файлов
     */
    void init();

    /**
     * @brief selectAllFileFormats              - Выбрать все типы файлов
     */
    void selectAllFileFormats();

    /**
     * @brief deselectAllFileFormats            - Убрать выбор со всех типов файлов
     */
    void deselectAllFileFormats();

    /**
     * @brief changeFileFormatItemsCheckState   - Изменяет состояние всех итемов типов файлов
     * @param state     - состояние
     */
    void changeAudioTypesCheckState( Qt::CheckState state);
    void changeVideoTypesCheckState( Qt::CheckState state);

private slots:

    /**
     * @brief checkItem             - Выбор типов файлов
     * @param item
     */
    void checkAudioItem( QListWidgetItem * item );

    /**
     * @brief checkVideoItem
     * @param item
     */
    void checkVideoItem( QListWidgetItem * item );

    /**
     * @brief slot_changePage       - Изменени страницы настроек
     * @param page
     */
    void slot_changePage(int page);

signals:
    /**
     * @brief fileFormats
     */
    void fileFormats();
    void fileFormatsAudio();
    void fileFormatsVideo();

private:
    Ui::SettingsDialog *ui;

};

#endif // SETTINGSDIALOG_H
