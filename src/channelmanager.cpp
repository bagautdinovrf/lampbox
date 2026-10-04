#include "channelmanager.h"
#include "channelmodel.h"
#include "informer.h"
#include "stationmanager.h"
#include <QDir>
#include <QFileInfo>
#include <QSet>
#include <QUuid>

namespace {
void apply(ChannelData &data, const ScheduleCore::ChannelRule &rule)
{
    data.setChannelName(rule.name);
    data.setRuleId(rule.stableId);
    data.setStartTime(rule.start); data.setEndTime(rule.end);
    data.setDaysOfWeek(rule.weekdays); data.setDays(rule.days); data.setMonths(rule.months);
    data.setVolume(rule.volume);
    data.setPlaybackOrder(rule.order);
}
}

ChannelManager::ChannelManager(CHANNEL_TYPE type) : mManagerType(type)
{
    const SPathData paths;
    mProjectPaths = {paths.homePathDir, paths.channelDirMusic, paths.channelDirVideo};
    mProjectFile = ProjectRepository::filePath(mProjectPaths);
    mChannelDir = type == MUSIC ? paths.channelDirMusic : paths.channelDirVideo;
}
ChannelManager::~ChannelManager() = default;

bool ChannelManager::fail(const QString &error)
{
    mLastError = error;
    Informer::Instance().infoEvent(error, Informer::ERROR);
    return false;
}

bool ChannelManager::collectChannels()
{
    ProjectRepository::Project project;
    QString error;
    mLoadFailed = true;
    if (!ProjectRepository::load(mProjectPaths, &project, &error)) return fail(error);
    const auto &loaded = mManagerType == VIDEO ? project.video : project.music;
    // Parse everything before reset; loading never scans or removes media.
    if (mParent) mParent->beginCollect();
    mChannelList.clear();
    mChannelList.reserve(loaded.size());
    for (const auto &rule : loaded) {
        mChannelList.emplaceBack(mManagerType);
        apply(mChannelList.last(), rule);
    }
    mNumCurrentChannel = -1;
    if (mParent) mParent->endCollect();
    mLastError.clear();
    mLoadFailed = false;
    return true;
}

QList<ScheduleCore::ChannelRule> ChannelManager::rules() const
{
    QList<ScheduleCore::ChannelRule> result;
    for (const ChannelData &data : mChannelList) {
        ScheduleCore::ChannelRule rule;
        rule.name = data.channelName(); rule.start = data.startTime(); rule.end = data.endTime();
        rule.stableId = data.ruleId();
        rule.weekdays = data.daysOfWeek(); rule.days = data.days(); rule.months = data.months();
        rule.volume = data.volume(); rule.order = data.playbackOrder(); result.append(rule);
    }
    return result;
}

bool ChannelManager::decodeRule(const QVariantList &fields, ScheduleCore::ChannelRule *rule)
{
    if (fields.size() != 7 && fields.size() != 8)
        return fail(QStringLiteral("Ожидаются семь или восемь полей правила канала"));
    rule->name = fields[0].toString(); rule->start = fields[1].toTime(); rule->end = fields[2].toTime();
    rule->weekdays = fields[3].toString(); rule->days = fields[4].toString(); rule->months = fields[5].toString();
    bool volumeOk = false;
    rule->volume = fields[6].toInt(&volumeOk);
    if (fields.size() == 8) rule->order = fields[7].toString();
    if (!ProjectRepository::validFileName(rule->name, true))
        return fail(QStringLiteral("Имя канала должно быть безопасным именем каталога"));
    if (!volumeOk) return fail(QStringLiteral("Некорректная громкость"));
    const QString error = ScheduleCore::validateChannel(*rule);
    return error.isEmpty() || fail(error);
}

bool ChannelManager::setRule(int row, const QVariantList &fields)
{
    if (mLoadFailed) return fail(QStringLiteral("Сохранение запрещено после ошибки загрузки. ") + mLastError);
    if (row < 0 || row >= mChannelList.size()) return fail(QStringLiteral("Канал не найден"));
    ScheduleCore::ChannelRule rule;
    if (!decodeRule(fields, &rule)) return false;
    auto snapshot = rules();
    rule.stableId = snapshot[row].stableId;
    if (fields.size() == 7) rule.order = snapshot[row].order;
    for (int i = 0; i < snapshot.size(); ++i)
        if (i != row && snapshot[i].name.compare(rule.name, Qt::CaseInsensitive) == 0)
            return fail(QStringLiteral("Канал с таким именем уже существует"));
    const QString oldName = snapshot[row].name;
    const bool renamed = oldName != rule.name;
    snapshot[row] = rule;
    QString error;
    if (!ProjectRepository::replaceChannels(mProjectPaths, mManagerType == VIDEO, snapshot,
                renamed ? oldName : QString(), renamed ? rule.name : QString(), &error)) {
        return fail(error);
    }
    apply(mChannelList[row], rule);
    mLastError.clear();
    return true;
}

bool ChannelManager::createChannel(const QVariantList &fields)
{
    if (mLoadFailed) return fail(QStringLiteral("Создание запрещено после ошибки загрузки. ") + mLastError);
    if (StationManager::Instance().trial() && !mChannelList.isEmpty())
        return fail(QStringLiteral("Пробная версия позволяет создать только один плейлист"));
    ScheduleCore::ChannelRule rule;
    if (!decodeRule(fields, &rule)) return false;
    rule.stableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    if (containsChannel(rule.name)) return fail(QStringLiteral("Канал с таким именем уже существует"));
    QDir root(mChannelDir);
    if (root.exists(rule.name) || !QDir().mkpath(root.absolutePath()) || !root.mkdir(rule.name))
        return fail(QStringLiteral("Не удалось создать каталог канала %1").arg(rule.name));
    auto snapshot = rules(); snapshot.append(rule);
    QString error;
    if (!ProjectRepository::replaceChannels(mProjectPaths, mManagerType == VIDEO, snapshot, {}, {}, &error)) {
        root.rmdir(rule.name); // Only our newly created empty directory.
        return fail(error);
    }
    if (mParent) mParent->beginCollect();
    mChannelList.emplaceBack(mManagerType);
    apply(mChannelList.last(), rule);
    if (mParent) mParent->endCollect();
    mLastError.clear();
    return true;
}

bool ChannelManager::deleteChannel(int row)
{
    if (mLoadFailed) return fail(QStringLiteral("Удаление запрещено после ошибки загрузки. ") + mLastError);
    if (row < 0 || row >= mChannelList.size()) return fail(QStringLiteral("Канал не найден"));
    auto snapshot = rules();
    const QString name = snapshot[row].name;
    if (!ProjectRepository::validFileName(name, true)) return fail(QStringLiteral("Небезопасное имя каталога"));
    QDir root(mChannelDir);
    const QString tombstone = QStringLiteral(".deleted-") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    const bool directoryExists = root.exists(name);
    snapshot.removeAt(row);
    QString error;
    if (!ProjectRepository::replaceChannels(mProjectPaths, mManagerType == VIDEO, snapshot,
                directoryExists ? name : QString(), directoryExists ? tombstone : QString(), &error)) {
        return fail(error);
    }
    if (mParent) mParent->beginCollect();
    mChannelList.removeAt(row);
    mNumCurrentChannel = -1;
    if (mParent) mParent->endCollect();
    mLastError.clear();
    // Commit schedule before deleting bytes; failed cleanup leaves an orphan.
    if (directoryExists && !QFileInfo::exists(mProjectFile + QStringLiteral(".pending"))
            && !QDir(root.filePath(tombstone)).removeRecursively())
        Informer::Instance().infoEvent(QStringLiteral("Канал удалён из расписания; оставшиеся файлы: %1").arg(root.filePath(tombstone)), Informer::WARNING);
    return true;
}

ChannelData &ChannelManager::channel(int num)
{
    if (num < 0 || num >= mChannelList.size()) {
        static ChannelData invalid;
        return invalid;
    }
    return mChannelList[num];
}
int ChannelManager::channelCount() const { return mChannelList.size(); }
int ChannelManager::columnCount() const { return 7; }
void ChannelManager::setCurrentChannel(int cur) { mNumCurrentChannel = cur >= 0 && cur < mChannelList.size() ? cur : -1; }
ChannelData &ChannelManager::currentChannel() { return channel(mNumCurrentChannel); }
void ChannelManager::setChannelModel(ChannelModel *model) { mParent = model; }
bool ChannelManager::containsChannel(const QString &name)
{
    for (const auto &data : mChannelList)
        if (name.compare(data.channelName(), Qt::CaseInsensitive) == 0) return true;
    return false;
}
bool ChannelManager::deleteCurrentChannel() { return deleteChannel(mNumCurrentChannel); }
int ChannelManager::currentChannelNum() { return mNumCurrentChannel; }
