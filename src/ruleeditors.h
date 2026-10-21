#pragma once

#ifndef RULEEDITORS_H
#define RULEEDITORS_H

#include <QDate>
#include <QDialog>
#include <QIdentityProxyModel>
#include <QStringList>
#include <QTime>
#include <functional>
#include <memory>
#include <optional>

class QAbstractItemModel;

namespace RuleEditors {

struct ChannelRuleValues {
    QString name = QStringLiteral("Новый_канал");
    QTime start{8, 0};
    QTime end{22, 0};
    QString weekdays = QStringLiteral("*");
    QString days = QStringLiteral("*");
    QString months = QStringLiteral("*");
    int volume = 65;
    QString order = QStringLiteral("shuffle_cycle");
    int untilDayOffset = 0;
};

struct AdvertRuleValues {
    QString fileName;
    QString hours = QStringLiteral("*");
    // The existing format distinguishes 00m,30m / frequency 1..5 / * (never).
    QString minutes = QStringLiteral("00m,30m");
    QString weekdays = QStringLiteral("*");
    QDate start = QDate::currentDate();
    QDate end = QDate::currentDate().addMonths(1);
    int volume = 75;
    QString startMode = QStringLiteral("interrupt");
};

class ChannelRuleDialog final : public QDialog {
public:
    explicit ChannelRuleDialog(const ChannelRuleValues &initial,
                               const QStringList &otherChannelNames = {},
                               QWidget *parent = nullptr, bool creating = false);
    ~ChannelRuleDialog() override;
    ChannelRuleValues values() const;
    void enableDelete();
    bool deleteRequested() const;
    void accept() override;
protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
private:
    struct Private;
    std::unique_ptr<Private> d;
};

class AdvertRuleDialog final : public QDialog {
public:
    explicit AdvertRuleDialog(const AdvertRuleValues &initial, QWidget *parent = nullptr,
                              bool creating = false, const QStringList &files = {});
    ~AdvertRuleDialog() override;
    AdvertRuleValues values() const;
    void accept() override;
protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;
private:
    struct Private;
    std::unique_ptr<Private> d;
};

std::optional<ChannelRuleValues> newChannel(QWidget *parent = nullptr,
                                          const QStringList &existingNames = {});
std::optional<AdvertRuleValues> newAdvert(const QString &fileName, QWidget *parent = nullptr);
// Lets the operator pick the clip from the advert library; preselected may be empty.
std::optional<AdvertRuleValues> newAdvert(const QStringList &files, const QString &preselected,
                                          QWidget *parent = nullptr);
bool applyChannel(QAbstractItemModel *model, int row, const ChannelRuleValues &values);
bool applyAdvert(QAbstractItemModel *model, int row, const AdvertRuleValues &values);
bool editChannel(QAbstractItemModel *model, int row, QWidget *parent = nullptr,
                 const std::function<void()> &deleteAction = {});
bool editAdvert(QAbstractItemModel *model, int row, QWidget *parent = nullptr);

// Operator-facing wording of stored rule fields ("*", "1,2,3,4,5", "00m,30m").
// Values that cannot be parsed are returned unchanged.
QString describeWeekdays(const QString &value);
QString describeMonthDays(const QString &value);
QString describeMonths(const QString &value);
QString describeHours(const QString &value);
QString describeAdvertTiming(const QString &minutes);
QString describeCalendar(const QString &weekdays, const QString &days, const QString &months);

// Display-only view of ChannelModel / AdvertModel columns. Edit roles and row
// numbers stay those of the source model.
// Adverts get one extra, read-only "Запуск" column from AdvertModel::StartModeRole.
class RuleTableModel final : public QIdentityProxyModel {
public:
    enum Kind { Channels, Adverts };
    explicit RuleTableModel(Kind kind, QObject *parent = nullptr);
    int columnCount(const QModelIndex &parent = {}) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = {}) const override;
    QModelIndex sibling(int row, int column, const QModelIndex &index) const override;
    QModelIndex mapToSource(const QModelIndex &proxyIndex) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
private:
    int startColumn() const;
    Kind mKind;
};

} // namespace RuleEditors

#endif
