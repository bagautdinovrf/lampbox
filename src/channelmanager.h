

#ifndef CHANNELMANAGER_H
#define CHANNELMANAGER_H


#include "channeldata.h"
#include "lampdata.h"

using namespace LampBox;

class ChannelModel;


class ChannelManager
{
public:
    ChannelManager(CHANNEL_TYPE type);
    ~ChannelManager();

    /**
     * @brief collectChannels
     */
    void collectChannels();

    /**
     * @brief saveChannels
     */
    void saveChannels();

    /**
     * @brief getChannelData
     * @param num
     * @return
     */
    ChannelData& channel( int num );

    /// Количество каналов
    int channelCount() const;

    /// Количество столбцов
    int columnCount() const;

    void setCurrentChannel(int cur);

    ChannelData& currentChannel();

    bool createChannel();

    void setChannelModel( ChannelModel * model );

    bool deleteChannel( int num );

    bool deleteCurrentChannel();

    bool containsChannel(const QString &name );

    /**
     * @brief currentChannelNum            - Возвращает номер текущего канала
     * @return
     */
    int currentChannelNum();

private:
    /**
     * @brief parseLine
     * @param ba
     */
    ChannelData parseLine(const QString &ba);

    /**
     * @brief getTime
     * @param str
     * @return
     */
    QTime getTime(const QString &str );

    /**
     * @brief createChannelDir
     * @param name
     * @return
     */
    bool createChannelDir(const QString &name);

private:
    /// Тип менеджера
    const CHANNEL_TYPE                          mManagerType;
    ///
    QList<ChannelData>                          mChannelList;
    ///
    SPathData                                   mPath;
    /// Путь к расписанию
    QString                                     mTimeTableFile;
    /// Текущий канал
    int                                         mNumCurrentChannel;
    ///
    ChannelModel                                *mParent;
    ///Название канала по умолчанию
    const QString                               mDefaultChannelName;
    ///
    QString                                     mChannelDir;
};

#endif // CHANNELMANAGER_H
