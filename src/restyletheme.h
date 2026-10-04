#pragma once

#ifndef RESTYLETHEME_H
#define RESTYLETHEME_H

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPalette>
#include <QRectF>
#include <QStringList>

class QApplication;
class QPainter;
class QWidget;

// All metrics are logical pixels. No global style sheet is used: widgets keep
// their native input, accessibility and model/view behaviour.
namespace Restyle {
struct Tokens {
    bool dark = false;
    bool relief = true;
    QColor background, surface, surface2, panel, header, navigation, transport;
    QColor text, secondary, muted, line, field, fieldBorder;
    QColor accent, accentText, accentSoft, accentLine, selection, focus;
    QColor success, successBg, warning, warningBg, error, errorBg;
    QColor tableHeader, tableLine, buttonTop, buttonBottom;
    int panelRadius = 14, buttonRadius = 9, fieldRadius = 8;
    int channelRadius = 10, dayRadius = 6, trackRadius = 6;
    int intervalRadius = 4, dialogRadius = 17;
};

void install(QApplication &application,
             const QString &appearance = QStringLiteral("tide-relief"),
             const QString &theme = QStringLiteral("denim"));
void apply(const QString &appearance, const QString &theme);
const Tokens &tokens();
QString appearanceId();
QString themeId();
QStringList appearanceIds();
QStringList themeIds();
QString appearanceName(const QString &id);
QString themeName(const QString &id);
QPalette palette();
QColor mix(const QColor &first, qreal amount, const QColor &second);
QFont font(int pixels = 13, int weight = QFont::Normal, qreal letterSpacing = 0);
QString fontFamily();
bool verifiedCyrillicFont();
QIcon icon(const QString &name, const QColor &color = QColor());
void paintIcon(QPainter &painter, const QRectF &rect, const QString &name,
               const QColor &color);
void button(QWidget *widget, const QString &role = QStringLiteral("normal"));
void surface(QWidget *widget, const QString &role = QStringLiteral("panel"));
void paintSurface(QPainter &painter, const QRectF &rect, const QString &role,
                  bool pressed = false);
void paintChildShadows(QPainter &painter, QWidget *parent);
}

#endif
