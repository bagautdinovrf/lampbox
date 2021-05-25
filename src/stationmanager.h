#ifndef STATIONMANAGER_H
#define STATIONMANAGER_H

#include <QObject>
#include <QDir>
#include <QMap>

#define LOCAL_ID -1
enum TypeStation{
    STATION_LOCAL = 1,
    STATION_NETWORK
};

class StationManager : public QObject
{
    Q_OBJECT
private:
    StationManager() :
        QObject(0),
        nameStation("NO SET"), stationId(LOCAL_ID),
        typeStation(STATION_LOCAL), pathToStation(""), lastErrorStr("Data not load")
    {
        textType[STATION_LOCAL] = tr("Локальная станиця");
        textType[STATION_NETWORK] = tr("Сетевая станиця");
        update();
    }

    StationManager(const StationManager&);
    StationManager& operator=(const StationManager&);
    QString nameStation;
    int stationId;          // if -1(LOCAL_ID) => local
    TypeStation typeStation;// if stationId != -1(LOCAL_ID) and typeStation = local => nonfree version
    QDir pathToStation;
    QDir pathToMedia;
    QString lastErrorStr;
    QString mCronDir;
    bool alternativeExecScript;
    QMap<TypeStation,QString> textType;

    /// Конфигрурафионный файл
    QString                     mConfigFile;

    /// Триальность
    bool                        mTrial;

public:
    static StationManager& Instance()
    {
        static StationManager singleInstance;
        return singleInstance;
    }

    bool update();
    QString get();
    QString get(QString subdir);
    QString media(QString subdir);
    bool isAlter();
    QString getCronDir();
    TypeStation type();
    QString typeText();
    int id();
    QString lastError();

    /**
     * @brief configFile        - Возвращает файл конфигурации
     * @return      - полный путь до файла конфигурации
     */
    QString configFile();

    /**
     * @brief trial     - Триальный статус
     * @return
     *          false - полная версия
     *          true  - триальная версия
     */
    bool trial();
};

//Путь до скриптов
#define STATIONPATH StationManager::Instance().get()
//Путь до файла
#define STATIONPATHTO(X) StationManager::Instance().get(X)
//Путь до хранилища
#define STATIONMEDIA StationManager::Instance().media("")
#define STATIONMEDIATO(X) StationManager::Instance().media(X)
///
#endif // STATIONMANAGER_H
