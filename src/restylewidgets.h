#pragma once

#ifndef RESTYLEWIDGETS_H
#define RESTYLEWIDGETS_H

#include "restyletheme.h"
#include <QLabel>
#include <QWidget>

class RestylePanel : public QWidget
{
public:
    explicit RestylePanel(QWidget *parent = nullptr,
                          const QString &role = QStringLiteral("panel"));
    void setSurfaceRole(const QString &role);
protected:
    void paintEvent(QPaintEvent *event) override;
};

class RestyleLabel : public QLabel
{
public:
    explicit RestyleLabel(const QString &text = {}, int pixels = 13,
                         int weight = QFont::Normal, QWidget *parent = nullptr);
    void setColorRole(const QString &role);
protected:
    void changeEvent(QEvent *event) override;
private:
    void updateColor();
    QString mColorRole;
};

#endif
