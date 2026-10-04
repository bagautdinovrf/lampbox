#include "advertmanager.h"
#include "informer.h"
#include "mediaboxmanagerdata.h"
#include "stationmanager.h"
#include <QUuid>

namespace {
AdvertData dataFor(const ScheduleCore::AdvertRule &rule)
{
    AdvertData data;
    data.setName(rule.name); data.setHours(rule.hours); data.setMinuts(rule.timing);
    data.setDays(rule.weekdays); data.setStartDate(rule.from); data.setEndDate(rule.until);
    data.setVolume(rule.volume);
    data.setStartMode(rule.startMode);
    return data;
}
bool sameTiming(const ScheduleCore::AdvertRule &a, const ScheduleCore::AdvertRule &b)
{
    const auto at = ScheduleCore::parseAdvertTiming(a.timing), bt = ScheduleCore::parseAdvertTiming(b.timing);
    return a.name == b.name
            && ScheduleCore::parseCalendar(a.hours, 0, 23).values == ScheduleCore::parseCalendar(b.hours, 0, 23).values
            && ScheduleCore::parseCalendar(a.weekdays, 0, 6).values == ScheduleCore::parseCalendar(b.weekdays, 0, 6).values
            && a.from == b.from && a.until == b.until && at.kind == bt.kind
            && at.frequency == bt.frequency && at.minutes == bt.minutes;
}
}

AdvertManager::AdvertManager(QObject *parent) : QObject(parent)
{
    const SPathData paths;
    mProjectPaths = {paths.homePathDir, paths.channelDirMusic, paths.channelDirVideo};
    collectAdvert();
}
bool AdvertManager::fail(const QString &error)
{
    mLastError = error;
    Informer::Instance().infoEvent(error, Informer::ERROR);
    return false;
}
int AdvertManager::column() { return 7; }
int AdvertManager::count() { return mAdvertDataList.size(); }

bool AdvertManager::collectAdvert()
{
    QString error;
    mLoadFailed = true;
    ProjectRepository::Project project;
    if (!ProjectRepository::load(mProjectPaths, &project, &error)) return fail(error);
    QList<AdvertData> replacement;
    QList<QList<int>> phases;
    QStringList ids;
    for (const auto &rule : project.advert) {
        replacement.append(dataFor(rule)); phases.append(rule.compiledMinutes); ids.append(rule.stableId);
    }
    emit beginCollect();
    mAdvertDataList = replacement; mCompiledMinutes = phases; mRuleIds = ids;
    emit endCollect();
    mLastError.clear(); mLoadFailed = false;
    return true;
}

QList<ScheduleCore::AdvertRule> AdvertManager::rules() const
{
    QList<ScheduleCore::AdvertRule> result;
    for (int row = 0; row < mAdvertDataList.size(); ++row) {
        const auto &data = mAdvertDataList[row];
        ScheduleCore::AdvertRule rule;
        rule.stableId = mRuleIds.value(row);
        rule.name = data.name(); rule.hours = data.hours(); rule.timing = data.minuts();
        rule.weekdays = data.days(); rule.from = data.startDate(); rule.until = data.endDate();
        rule.volume = data.volume(); rule.compiledMinutes = compiledMinutes(row);
        rule.startMode = data.startMode(); result.append(rule);
    }
    return result;
}

bool AdvertManager::decodeRule(const QVariantList &fields, ScheduleCore::AdvertRule *rule)
{
    if (fields.size() != 7 && fields.size() != 8)
        return fail(QStringLiteral("Ожидаются семь или восемь полей рекламного правила"));
    rule->name = fields[0].toString(); rule->hours = fields[1].toString(); rule->timing = fields[2].toString();
    rule->weekdays = fields[3].toString(); rule->from = fields[4].toDate(); rule->until = fields[5].toDate();
    bool volumeOk = false; rule->volume = fields[6].toInt(&volumeOk);
    if (fields.size() == 8) rule->startMode = fields[7].toString();
    if (!ProjectRepository::validFileName(rule->name, false)) return fail(QStringLiteral("Некорректное имя рекламного файла"));
    if (!volumeOk) return fail(QStringLiteral("Некорректная громкость"));
    const QString error = ScheduleCore::validateAdvert(*rule);
    return error.isEmpty() || fail(error);
}

bool AdvertManager::persist(const QList<ScheduleCore::AdvertRule> &snapshot)
{
    if (mLoadFailed) return fail(QStringLiteral("Сохранение запрещено после ошибки загрузки. ") + mLastError);
    auto prepared = snapshot;
    QList<QList<int>> phases;
    for (auto &rule : prepared) {
        const QString error = ScheduleCore::validateAdvert(rule);
        if (!error.isEmpty()) return fail(error);
        if (!ProjectRepository::validFileName(rule.name, false)) return fail(QStringLiteral("Некорректное имя рекламного файла"));
        if (ScheduleCore::parseAdvertTiming(rule.timing).kind == ScheduleCore::AdvertTiming::Kind::Frequency)
            rule.compiledMinutes = ScheduleCore::compileAdvertMinutes(rule);
        else rule.compiledMinutes.clear();
        phases.append(rule.compiledMinutes);
    }
    QString error;
    if (!ProjectRepository::replaceAdverts(mProjectPaths, prepared, &error)) return fail(error);
    mCompiledMinutes = phases;
    mLastError.clear(); return true;
}

bool AdvertManager::setRule(int row, const QVariantList &fields)
{
    if (StationManager::Instance().type() == STATION_NETWORK)
        return fail(QStringLiteral("Рекламой сетевой станции управляют централизованно"));
    if (row < 0 || row >= mAdvertDataList.size()) return fail(QStringLiteral("Рекламное правило не найдено"));
    ScheduleCore::AdvertRule rule;
    rule.startMode = mAdvertDataList[row].startMode();
    if (!decodeRule(fields, &rule)) return false;
    if (rule.name != mAdvertDataList[row].name()) return fail(QStringLiteral("Нельзя изменить файл рекламного правила"));
    auto snapshot = rules();
    rule.stableId = snapshot[row].stableId;
    if (sameTiming(rule, snapshot[row])) rule.compiledMinutes = snapshot[row].compiledMinutes;
    snapshot[row] = rule;
    if (!persist(snapshot)) return false;
    mAdvertDataList[row] = dataFor(rule);
    return true;
}

bool AdvertManager::addAdvert(const QVariantList &fields)
{
    if (StationManager::Instance().type() == STATION_NETWORK)
        return fail(QStringLiteral("Рекламой сетевой станции управляют централизованно"));
    if (StationManager::Instance().trial() && mAdvertDataList.size() > 1)
        return fail(QStringLiteral("Достигнут предел рекламных правил пробной версии"));
    ScheduleCore::AdvertRule rule;
    if (!decodeRule(fields, &rule)) return false;
    rule.stableId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    auto snapshot = rules(); snapshot.append(rule);
    if (!persist(snapshot)) return false;
    emit beginCollect();
    mAdvertDataList.append(dataFor(rule)); mRuleIds.append(rule.stableId);
    emit endCollect();
    return true;
}
bool AdvertManager::delAdvert(int row)
{
    if (StationManager::Instance().type() == STATION_NETWORK)
        return fail(QStringLiteral("Рекламой сетевой станции управляют централизованно"));
    if (row < 0 || row >= mAdvertDataList.size()) return fail(QStringLiteral("Рекламное правило не найдено"));
    auto snapshot = rules(); snapshot.removeAt(row);
    if (!persist(snapshot)) return false;
    emit beginCollect();
    mAdvertDataList.removeAt(row); mRuleIds.removeAt(row);
    emit endCollect();
    return true;
}
AdvertData &AdvertManager::advert(int row)
{
    if (row < 0 || row >= mAdvertDataList.size()) { static AdvertData invalid; return invalid; }
    return mAdvertDataList[row];
}

QList<int> AdvertManager::compiledMinutes(int row) const
{
    if (row < 0 || row >= mCompiledMinutes.size() || row >= mAdvertDataList.size()
            || ScheduleCore::parseAdvertTiming(mAdvertDataList[row].minuts()).kind != ScheduleCore::AdvertTiming::Kind::Frequency)
        return {};
    return mCompiledMinutes[row];
}
