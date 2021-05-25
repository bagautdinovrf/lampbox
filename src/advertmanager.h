#ifndef ADVERTMANAGER_H
#define ADVERTMANAGER_H


#include "advertdata.h"
#include "lampdata.h"


#include <QObject>

class AdvertModel;

class AdvertManager : public QObject
{
    Q_OBJECT
public:
    AdvertManager(QObject *parent = 0);

    /// Возвращает число стоблцов для модели
    int column();

    /// Количество записей
    int count();

    /// Возвращает запрашиваемы данные по рекламе
    AdvertData &advert(int num);

    /// Возвращает список рекламных треков
    const QList<AdvertData>& advertDataList();

    /// Добавить рекламу
    bool addAdvert(const QString &name);

    /// Удалить рекламу
    bool delAdvert(int num);

private:
    ///
    bool saveAdvert();

    /// Формирование списка рекламы
    void collectAdvert();

    /// Парсинг рекламы
    AdvertData parseAdvert( const QString &line );

    bool generateAdvert();

signals:
    /// Начало изменения списка
    void beginCollect();

    /// Конец изменения списка
    void endCollect();

private:
    /// Формат даты
    const QString                               mDateFormat;

    /// Директории
    SPathData                                   mPathData;

    /// Cron файл с расписанием рекламы
    QString                                     mFileAdvertTask;

    /// Файл с расписанием рекламы для таблицы с рекламой
    QString                                     mFileAdverTable;
    QString                                     mFileAdvertView;

    /// Список рекламных роликов
    QList<AdvertData>                           mAdvertDataList;

    /// Ддобавим модель в друзья
    friend class AdvertModel;
};

#endif // ADVERTMANAGER_H
