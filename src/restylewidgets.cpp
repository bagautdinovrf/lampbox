#include "restylewidgets.h"

#include <QEvent>
#include <QPainter>

RestylePanel::RestylePanel(QWidget *parent, const QString &role)
    : QWidget(parent)
{
    setAttribute(Qt::WA_StyledBackground, false);
    setProperty("restyleOwnPaint", true);
    Restyle::surface(this, role);
}

void RestylePanel::setSurfaceRole(const QString &role)
{
    Restyle::surface(this, role);
}

void RestylePanel::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    Restyle::paintSurface(painter, rect(), property("restyleSurface").toString());
    Restyle::paintChildShadows(painter, this);
}

RestyleLabel::RestyleLabel(const QString &text, int pixels, int weight, QWidget *parent)
    : QLabel(text, parent)
{
    setFont(Restyle::font(pixels, weight));
}

void RestyleLabel::setColorRole(const QString &role)
{
    mColorRole = role;
    updateColor();
}

void RestyleLabel::updateColor()
{
    if (mColorRole.isEmpty())
        return;
    const auto &t = Restyle::tokens();
    QColor color = t.text;
    if (mColorRole == "muted") color = t.muted;
    else if (mColorRole == "secondary") color = t.secondary;
    else if (mColorRole == "accent") color = t.accentText;
    else if (mColorRole == "success") color = t.success;
    else if (mColorRole == "warning") color = t.warning;
    else if (mColorRole == "error") color = t.error;
    QPalette p = palette();
    p.setColor(QPalette::WindowText, color);
    setPalette(p);
}

void RestyleLabel::changeEvent(QEvent *event)
{
    QLabel::changeEvent(event);
    if (event->type() == QEvent::ApplicationPaletteChange
            || event->type() == QEvent::PaletteChange
            || event->type() == QEvent::StyleChange)
        updateColor();
}
