#pragma once
#include "advertdata.h"
#include "projectrepository.h"
#include <QObject>
#include <QVariant>

class AdvertModel;
class AdvertManager : public QObject
{
    Q_OBJECT
public:
    explicit AdvertManager(QObject *parent = nullptr);
    int column();
    int count();
    AdvertData &advert(int num);
    bool addAdvert(const QVariantList &fields);
    bool delAdvert(int num);
    bool setRule(int row, const QVariantList &fields);
    bool collectAdvert();
    QString lastError() const { return mLastError; }
    bool scheduleLoaded() const { return !mLoadFailed; }
    QList<int> compiledMinutes(int row) const;
    QString ruleId(int row) const { return mRuleIds.value(row); }

signals:
    void beginCollect();
    void endCollect();

private:
    QList<ScheduleCore::AdvertRule> rules() const;
    bool decodeRule(const QVariantList &fields, ScheduleCore::AdvertRule *rule);
    bool persist(const QList<ScheduleCore::AdvertRule> &rules);
    bool fail(const QString &error);
    ProjectRepository::Paths mProjectPaths;
    QList<AdvertData> mAdvertDataList;
    QList<QList<int>> mCompiledMinutes;
    QStringList mRuleIds;
    QString mLastError;
    bool mLoadFailed = true;
};
