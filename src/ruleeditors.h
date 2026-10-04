#pragma once

#ifndef RULEEDITORS_H
#define RULEEDITORS_H

#include <QDate>
#include <QDialog>
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
                              bool creating = false);
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
bool applyChannel(QAbstractItemModel *model, int row, const ChannelRuleValues &values);
bool applyAdvert(QAbstractItemModel *model, int row, const AdvertRuleValues &values);
bool editChannel(QAbstractItemModel *model, int row, QWidget *parent = nullptr,
                 const std::function<void()> &deleteAction = {});
bool editAdvert(QAbstractItemModel *model, int row, QWidget *parent = nullptr);

} // namespace RuleEditors

#endif
