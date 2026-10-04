#ifndef SETTINGSDIALOG_H
#define SETTINGSDIALOG_H

#include <QDialog>


class QListWidgetItem;
class QLineEdit;
class QSpinBox;
class RestyleLabel;

namespace Ui {
    class SettingsDialog;
}

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent, Qt::WindowFlags f = {});
    ~SettingsDialog() override;

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
    void savePlayerConnection();
    void saveVideoPlayerConnection();

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
    void doneRequested();
    void playerConnectionChanged();
    void videoPlayerConnectionChanged();
    void videoScreensRequested();
    /**
     * @brief fileFormats
     */
    void fileFormats();
    void fileFormatsAudio();
    void fileFormatsVideo();

private:
    Ui::SettingsDialog *ui;
    class QListWidget *mAudioFormats = nullptr;
    class QListWidget *mVideoFormats = nullptr;
    QLineEdit *mPlayerHost = nullptr;
    QSpinBox *mPlayerPort = nullptr;
    QLineEdit *mPlayerToken = nullptr;
    RestyleLabel *mPlayerConnectionMessage = nullptr;
    QLineEdit *mVideoPlayerHost = nullptr;
    QSpinBox *mVideoPlayerPort = nullptr;
    QLineEdit *mVideoPlayerToken = nullptr;
    RestyleLabel *mVideoPlayerConnectionMessage = nullptr;

};

#endif // SETTINGSDIALOG_H
