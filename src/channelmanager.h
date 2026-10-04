#pragma once
#include "channeldata.h"
#include "projectrepository.h"
#include <QVariant>

using namespace MediaBoxManager;
class ChannelModel;

class ChannelManager
{
public:
    explicit ChannelManager(CHANNEL_TYPE type);
    ~ChannelManager();
    bool collectChannels();
    QString lastError() const { return mLastError; }
    bool scheduleLoaded() const { return !mLoadFailed; }
    bool setRule(int row, const QVariantList &fields);
    bool createChannel(const QVariantList &fields);
    ChannelData &channel(int num);
    int channelCount() const;
    int columnCount() const;
    void setCurrentChannel(int cur);
    ChannelData &currentChannel();
    void setChannelModel(ChannelModel *model);
    bool deleteChannel(int num);
    bool deleteCurrentChannel();
    bool containsChannel(const QString &name);
    int currentChannelNum();

private:
    QList<ScheduleCore::ChannelRule> rules() const;
    bool decodeRule(const QVariantList &fields, ScheduleCore::ChannelRule *rule);
    bool fail(const QString &error);
    const CHANNEL_TYPE mManagerType;
    QList<ChannelData> mChannelList;
    QString mProjectFile;
    ProjectRepository::Paths mProjectPaths;
    int mNumCurrentChannel = -1;
    ChannelModel *mParent = nullptr;
    QString mChannelDir;
    QString mLastError;
    bool mLoadFailed = true;
};
