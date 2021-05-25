

#include "channelmanager.h"
#include "channelmodel.h"
#include "boxlog.h"
#include "stationmanager.h"
#include "trialmessagebox.h"

#include <QDir>
#include <QTextStream>


ChannelManager::ChannelManager(CHANNEL_TYPE type) :
    mManagerType(type),
    mNumCurrentChannel(-1),
    mParent(0),
    mDefaultChannelName("Новый_")
{
    if( MUSIC == mManagerType) {
        mTimeTableFile  = mPath.sheduleFileMusic;
        mChannelDir     = mPath.channelDirMusic;
    } else if( VIDEO == mManagerType ) {
        mTimeTableFile  = mPath.sheduleFileVideo;
        mChannelDir     = mPath.channelDirVideo;
    }

    if( !QDir().exists(mChannelDir) )
        QDir().mkpath(mChannelDir);
}


ChannelManager::~ChannelManager()
{
    //
}

/**
 * @brief ChannelManager::collectChannels
 */
void ChannelManager::collectChannels()
{
    if( mParent )
        mParent->beginCollect();

    mChannelList.clear();
    QFile file( mTimeTableFile );
    if ( !file.open(QIODevice::ReadOnly | QIODevice::Text) ) {
        BoxLog() << "Невозможно открыть файл:" << file.fileName();
    } else {
        QTextStream stream(&file);
        while(!stream.atEnd()) {
            QString line = stream.readLine();
            mChannelList.append( parseLine(line) );
//            qDebug() << "volume=" << mChannelList.last().volume();
        }
        file.close();
    }

    if( StationManager::Instance().trial() || true ) {
        for( int i = mChannelList.size()-1; i > 0; --i ) {
            deleteChannel(i);
        }
    }

    if( mParent )
        mParent->endCollect();
}

/**
 * @brief ChannelManager::createChannel
 * @param name
 * @return
 */
bool ChannelManager::createChannel()
{
    if( StationManager::Instance().trial() ) {
        if( mChannelList.size() > 0 ) {
            TrialMessageBox("Вы можете создать только 1 плейлист.");
            return false;
        }
    }

    uint index = 1;
    QString name = mDefaultChannelName + QString::number(index);
    while( containsChannel(name)  ) {
        name = mDefaultChannelName + QString::number(++index);
    }

    createChannelDir(name);

    QFile file( mTimeTableFile );
    if( !file.open( QFile::Append | QFile::Text ) ) {
        BoxLog() << "Невозможно открыть файл:" << file.fileName();
        return false;
    }

    QString start_time;
    QString end_time = "00:00";

    if( channelCount() )
        start_time = channel(channelCount() - 1).endTime().toString("HH:mm");
    else
        start_time = end_time;

    ChannelData data(mManagerType);
    data.setChannelName( name );
    data.setStartTime( getTime(start_time) );
    data.setEndTime( getTime(end_time) );
    data.setDaysOfWeek("*");
    data.setDays("*");
    data.setMonths("*");
    data.setVolume(100);

    QTextStream stream(&file);
    stream << name << " "
           << start_time << " "
           << end_time << " "
           << data.daysOfWeek() << " "
           << data.days() << " "
           << data.months() << " "
           << QString::number(data.volume())
           << "\n";
    file.close();

    if( mParent )
        mParent->beginCollect();
    mChannelList.append(data);
    if( mParent )
        mParent->endCollect();

return true;
}

/**
 * @brief ChannelManager::saveChannels
 */
void ChannelManager::saveChannels()
{
    QFile file(mTimeTableFile);
    if( !file.open( QFile::WriteOnly | QFile::Text ) ) {
        BoxLog() << "Невозможно открыть файл:" << file.fileName();
        return;
    }

    QTextStream stream(&file);
    int size = mChannelList.size();

    for(int i = 0; i < size; ++i)
    {
       stream << mChannelList[i].channelName() + " " +
                 mChannelList[i].startTime().toString("HH:mm") + " " +
                 mChannelList[i].endTime().toString("HH:mm") << " "
              << mChannelList[i].daysOfWeek() << " "
              << mChannelList[i].days() << " "
              << mChannelList[i].months() << " "
              << QString::number( mChannelList[i].volume() )
              << "\n";
    }
    file.close();
}

/**
 * @brief ChannelManager::columnCount
 * @return
 */
int ChannelManager::columnCount() const
{
    // Плейлист
    // Начало
    // Окончаниче
    // Громкость
    return 7;
}

/**
 * @brief ChannelManager::parseLine
 * @param str
 * @return
 */
ChannelData ChannelManager::parseLine(const QString &str)
{
    QStringList list = str.split(' ');
    ChannelData data(mManagerType);
    data.setChannelName(list[0]);
    data.setStartTime(getTime(list[1]));
    data.setEndTime(getTime(list[2]));
    data.setDaysOfWeek(list[3]);
    data.setDays(list[4]);
    data.setMonths(list[5]);
    data.setVolume( list[6].toInt() );
//    qDebug() << "parse" << list[6].toInt();
return data;
}


QTime ChannelManager::getTime(const QString &str )
{
    QStringList list = str.split(':');
return QTime( list[0].toInt(), list[1].toInt() );
}

/**
 * @brief ChannelManager::getChannelData
 * @param num
 * @return
 */
ChannelData& ChannelManager::channel( int num )
{
    int id;
    if(num <= 0 )
        id = 0;
    else if( num >= mChannelList.size() )
        id = mChannelList.size() - 1;
    else
        id = num;

    return mChannelList[id];
}

/**
 * @brief ChannelManager::channelCount
 * @return
 */
int ChannelManager::channelCount() const
{
    return mChannelList.size();
}

void ChannelManager::setCurrentChannel(int cur)
{
    if(cur >= mChannelList.size() || cur < 0)
        mNumCurrentChannel = -1;
    else
        mNumCurrentChannel = cur;
}


ChannelData& ChannelManager::currentChannel()
{
    if( mNumCurrentChannel == -1 ) {
        static ChannelData channel;
        return channel;
    }
    return mChannelList[mNumCurrentChannel];
}


void ChannelManager::setChannelModel( ChannelModel * model )
{
    mParent = model;
}


bool ChannelManager::containsChannel(const QString &name)
{
    for( QList<ChannelData>::Iterator it = mChannelList.begin(); it != mChannelList.end(); ++it ) {
        if( name == it->channelName() )
            return true;
    }
    return false;
}


bool ChannelManager::deleteCurrentChannel()
{
    if( -1 == mNumCurrentChannel)
        return false;

    return deleteChannel(mNumCurrentChannel);
}


bool ChannelManager::deleteChannel(int num)
{
    if( num < 0 || num >= mChannelList.size() )
        return false;

    // Удаление канала
    if( !channel(num).channelName().isEmpty() ) {
        if( channel(num).mediaManager().deleteChannel() ) {
//            qDebug() << "delete channel ok";
        }
        else
            return false;
    }

    if(!mChannelList.size())
        mNumCurrentChannel = -1;

    if(mParent)
        mParent->beginCollect();
    mChannelList.removeAt(num);
    if(mParent)
        mParent->endCollect();

    saveChannels();

    return true;
}

bool ChannelManager::createChannelDir(const QString &name)
{
    if( name.isEmpty() )
        return false;

    QDir dir;
    dir.setPath(mChannelDir);
    if( !dir.mkdir(name) ) {
        BoxLog() << "Dir" << name << "cannot create!";
        return false;
    }

    return true;
}

int ChannelManager::currentChannelNum()
{
    return mNumCurrentChannel;
}
