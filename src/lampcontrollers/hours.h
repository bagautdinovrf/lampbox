#ifndef HOURS_H
#define HOURS_H
#include <vector>
#include <utility>
#include <QString>
#include <QObject>

using HoursList = std::vector<std::pair<QString, int>>;

class Hours : public QObject
{
    Q_OBJECT
public:
    Hours(HoursList);
    //! @fn Возвращает строчку часов запуска
    //! @return Строка часов
    QString GetHoursString();
    //! TODO функция выдает значение дней в виде интервала 1-15 например
    QString GetCronStyleString();
    //! @fn Возвращает максимальную частоту(пока)
    //! @return Максимальная частота
    int GetFrequency() const;
private:
    HoursList mHoursList;
    //! @var  Максимальная частота запуска трека.
    int       mMaxFrequency;
signals:

public slots:

};

#endif // HOURS_H
