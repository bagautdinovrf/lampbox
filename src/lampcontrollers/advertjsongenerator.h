#ifndef ADVERTJSONGENERATOR_H
#define ADVERTJSONGENERATOR_H

#include <QObject>
#include <QStringList>


class AdvertJsonGenerator : public QObject
{
    Q_OBJECT
public:
    explicit AdvertJsonGenerator(QString timetable = QString::null, QObject *parent = 0);

    /**
     * @brief setAdvertTimetable                    - Установка расписания
     * @param timetable     - расписание в формате LampBox в csv
     * @return
     *                      true    - успешно
     *                      false   - ошибка
     */
    void setAdvertTimetable(const QString timetable);
    void setAdvertTimetable(const QStringList timetable_list);

    /**
     * @brief generate
     * @return
     */
    QString json();


private:
    QStringList                 mTimeTableList;

};

#endif // ADVERTJSONGENERATOR_H
