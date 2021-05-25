#ifndef ADVERTCSVGENERATOR_H
#define ADVERTCSVGENERATOR_H

#include "trackfullinfo.h"

#include <QObject>
#include <QTemporaryFile>


class AdvertCsvGenerator : public QObject
{
    Q_OBJECT
public:
    explicit AdvertCsvGenerator(QObject *parent = 0);

    /**
     * @brief MakeCsv                Генерация рекламы в формате CSV
     * @brief makeAdvert
     * @param tracks
     * @return
     */
    bool makeAdvert(TracksFullInfo &tracks );

    /**
     * @brief file
     * @return
     */
    QTemporaryFile& file();

private:
    bool saveFile(const QString& text);

private:
    QTemporaryFile mTempFile;

};

#endif // ADVERTCSVGENERATOR_H
