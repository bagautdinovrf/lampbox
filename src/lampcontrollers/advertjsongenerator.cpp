#include "advertjsongenerator.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>
#include <QVariantMap>


AdvertJsonGenerator::AdvertJsonGenerator(QString timetable, QObject *parent) :
    QObject(parent)
{
}

void AdvertJsonGenerator::setAdvertTimetable(const QString timetable)
{
    setAdvertTimetable( timetable.split('\n') );
}


void AdvertJsonGenerator::setAdvertTimetable(const QStringList timetable_list)
{
    mTimeTableList.clear();
    mTimeTableList.append(timetable_list);
}

QString AdvertJsonGenerator::json()
{
    QJsonArray jarray;
    foreach (auto advert, mTimeTableList) {
        QStringList advert_data = advert.split(';');
        QVariantMap advertMap;
        int i = 0;
        advertMap.insert("name",     advert_data[i++]);
        advertMap.insert("hours",    advert_data[i++]);
        advertMap.insert("minuts",   advert_data[i++]);
        advertMap.insert("days",     advert_data[i++]);
        advertMap.insert("start",    advert_data[i++]);
        advertMap.insert("end",      advert_data[i++]);
        advertMap.insert("volume",   advert_data[i++]);
        QJsonValue jvalue = QJsonObject::fromVariantMap(advertMap);
        jarray.append(jvalue);
    }

    QJsonDocument jdoc(jarray);
    QString json = jdoc.toJson();
//    qDebug() << json

    return json;
}
