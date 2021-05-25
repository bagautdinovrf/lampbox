#ifndef SETTINGS_H
#define SETTINGS_H

#include <QObject>
#include <QMap>

namespace {
const QString DEFAULT_AUDIO_FORMAT = "*.mp3";
const QString DEFAULT_VIDEO_FORMAT = "*.mp4";
}

class Settings : public QObject
{
    Q_OBJECT
public:
    explicit Settings(QObject *parent = 0);

    /**
     * @brief writeFileFormatValue          - Запись значения в конфигурационный файл
     */
    void writeFileFormatAudioValue( QString format, bool value);

    /**
     * @brief writeFileFormatValue          - Запись значения в конфигурационный файл
     */
    void writeFileFormatVideoValue( QString format, bool value);

    /**
     * @brief writeStringSettings           - Запись значение в конфигурационный файл
     * @param key       - ключ
     * @param value     - значение
     */
    void writeStringSettings(QString key, QString value);

    /**
     * @brief fileFormats                   - Возвращает состояния всех форматов файлов
     * @return
     */
    const QMap<QString, bool>& fileFormats();
    const QMap<QString, bool>& fileFormatsAudio();
    const QMap<QString, bool>& fileFormatsVideo();

    /**
     * @brief availablelAllFileFormats
     * @return
     */
    const QStringList availablelAllFileFormats();

    const QStringList availablelAudioFileFormats();

    const QStringList availablelVideoFileFormats();


    /**
     * @brief allFormats        - Все форматы файлов
     * @return  - список всех доступных форматов файлов
     */
    static QStringList allFormats();

    static QStringList allAudioFormats();

    static QStringList allVideoFormats();

private:

    /**
     * @brief readSettings              - Считывание настроек из конфигурационного файла
     */
    void readSettings();

    /**
     * @brief initSettings              - Настройки по умолчанию
     */
    void initSettings();

    /**
     * @brief availableFileFormats
     * @return
     */
    const QStringList availableFileFormats( QMap<QString, bool> fileFormats );

private:
    /// Список форматов файлов
    QMap<QString, bool>                     mFileFormats;
    /// Список форматов файлов
    QMap<QString, bool>                     mFileAudioFormats;
    /// Список форматов файлов
    QMap<QString, bool>                     mFileVideoFormats;

    /// Конфигурационный файл, полный путь
    QString                                 mConfigFile;

    /// Имя секции в конфигурационном файле
    const QString                           mFileFormatsGroup;
    const QString                           mFormatsAudioGroup;
    const QString                           mFormatsVideoGroup;
};

#endif // SETTINGS_H
