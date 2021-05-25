

#ifndef MEDIACONTROLLER_H
#define MEDIACONTROLLER_H


#include <QObject>


class MediaController : public QObject
{
    Q_OBJECT

public:
    MediaController( QObject *parent = 0);
    ~MediaController();

    /**
     * @brief play              - Запуск плеера
     * @return
     */
    bool play();

    /**
     * @brief stop              - Остановка плеера
     * @return
     */
    bool stop();

    /**
     * @brief isPlaying         - Состояние плеера
     * @return
     */
    bool isPlaying();

    /**
     * @brief showPlayer        - Показ интерфейса плеера
     */
    bool showPlayer();

    /**
     * @brief hide              - Cкрытие интерфейса плеера
     */
    bool hidePlayer();

    void playTrack(const QString &track );


public slots:
    /**
     * @brief refreshPlayer     - Плеер перечитывает расписание
     */
    void refreshPlayer();

    /**
     * @brief slot_playerHide   - Cкрыть интерфейс плеера
     */
    void slot_playerHide();

    /**
     * @brief slot_playerShow   - Показать интерфейс плеера
     */
    void slot_playerShow();

private slots:
//    void ready();

    /**
     * @brief sendPlayingState  - Отправка текущего состояния плеера
     */
    void sendPlayingState();

private:

    /**
     * @brief startProcess      - Запуск процесса
     * @param arg           - строка для запуска
     * @return      true    - успешно
     *              false   - не удалось запустить
     */
    bool startProcess(const QString arg );

signals:
    /**
     * @brief playingState          - Текущее состояние плеера
     */
    void playingState( bool );
};

#endif // MEDIACONTROLLER_H
