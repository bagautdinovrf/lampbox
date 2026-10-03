#include "restyletheme.h"

#include <QAbstractButton>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QEvent>
#include <QFile>
#include <QFontDatabase>
#include <QGraphicsEffect>
#include <QHeaderView>
#include <QIconEngine>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QProxyStyle>
#include <QPushButton>
#include <QRawFont>
#include <QScreen>
#include <QSlider>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTextEdit>
#include <QToolButton>
#include <QXmlStreamReader>
#include <QtMath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cmath>

namespace {
Restyle::Tokens currentTokens;
QString currentAppearance = QStringLiteral("tide-relief");
QString currentTheme = QStringLiteral("denim");
QString chosenFont;
bool fontVerified = false;

QColor alpha(QColor color, int opacity)
{
    color.setAlpha(opacity);
    return color;
}

void loadFont()
{
    if (!chosenFont.isEmpty())
        return;
    QStringList paths;
#ifdef Q_OS_WIN
    const QString root = qEnvironmentVariable("WINDIR", QStringLiteral("C:/Windows"));
    paths << root + "/Fonts/segoeui.ttf" << root + "/Fonts/seguisb.ttf"
          << root + "/Fonts/segoeuib.ttf";
#endif
    // Legal, explicitly supplied local font files are also useful for an
    // isolated offscreen verification machine; fonts are never distributed.
    const QString extra = qEnvironmentVariable("MEDIABOX_FONT_DIRECTORY");
    if (!extra.isEmpty())
        paths << extra + "/segoeui.ttf" << extra + "/seguisb.ttf" << extra + "/segoeuib.ttf";
    for (const QString &path : paths) {
        if (!QFile::exists(path))
            continue;
        const int id = QFontDatabase::addApplicationFont(path);
        if (id >= 0 && chosenFont.isEmpty())
            chosenFont = QFontDatabase::applicationFontFamilies(id).value(0);
    }
    const QStringList families = QFontDatabase::families();
    if (chosenFont.isEmpty() && families.contains("Segoe UI"))
        chosenFont = QStringLiteral("Segoe UI");
    if (chosenFont.isEmpty()) {
        for (const QString &fallback : {QStringLiteral("Noto Sans"), QStringLiteral("DejaVu Sans")}) {
            if (families.contains(fallback)) { chosenFont = fallback; break; }
        }
        if (chosenFont.isEmpty()) chosenFont = QApplication::font().family();
        qWarning("Segoe UI is unavailable; preview uses %s. Reference typography is unverified.",
                 qPrintable(chosenFont));
    }
    QFont check(chosenFont);
    check.setPixelSize(13);
    QRawFont raw = QRawFont::fromFont(check);
    fontVerified = raw.isValid();
    const QString probe = QStringLiteral("АБВГДабвгдЁёЙй0123456789—%");
    for (QChar c : probe)
        fontVerified = fontVerified && raw.supportsCharacter(c);
    if (!fontVerified)
        qWarning("The selected interface font does not cover the required Cyrillic characters.");
}

Restyle::Tokens makeTokens(const QString &appearance, const QString &theme)
{
    Restyle::Tokens t;
    t.relief = appearance == "tide-relief";
    t.dark = theme == "dark";
    static const QHash<QString, QColor> accents = {
        {"denim", QColor("#315ca8")}, {"slate", QColor("#54707b")},
        {"pine", QColor("#2a6a58")}, {"berry", QColor("#a3336a")},
        {"graphite", QColor("#515963")}, {"pearl", QColor("#087e89")},
        {"dark", QColor("#315ca8")}
    };
    t.accent = accents.value(theme, QColor("#315ca8"));
    t.accentText = t.accent;
    t.accentSoft = Restyle::mix(t.accent, .09, Qt::white);
    t.accentLine = Restyle::mix(t.accent, .28, Qt::white);
    t.background = QColor(t.relief ? "#eef1f5" : "#f4f5f7");
    t.surface = QColor(t.relief ? "#f9fafc" : "#ffffff");
    t.surface2 = QColor(t.relief ? "#edf1f5" : "#f6f7f9");
    t.panel = QColor(t.relief ? "#f7f9fc" : "#ffffff");
    t.header = QColor(t.relief ? "#f4f6f9" : "#ffffff");
    t.navigation = QColor(t.relief ? "#f1f4f7" : "#ffffff");
    t.transport = QColor(t.relief ? "#f2f5f8" : "#ffffff");
    t.text = QColor(t.relief ? "#25303c" : "#232830");
    t.secondary = t.muted = QColor(t.relief ? "#647283" : "#66707d");
    t.line = QColor(t.relief ? "#dce2e9" : "#e1e5e9");
    t.field = QColor(t.relief ? "#edf2f6" : "#ffffff");
    t.fieldBorder = QColor(t.relief ? "#e4e9ef" : "#dce2e8");
    t.tableHeader = QColor(t.relief ? "#edf1f6" : "#f7f8fa");
    t.tableLine = QColor(t.relief ? "#e0e6ed" : "#e9edf1");
    t.selection = t.relief ? Restyle::mix(t.accent, .07, QColor("#f6f8fb")) : t.accentSoft;
    t.focus = t.accent;
    t.success = QColor("#236747"); t.successBg = QColor("#edf6f0");
    t.warning = QColor("#885816"); t.warningBg = QColor("#fff5e3");
    t.error = QColor("#ad3740"); t.errorBg = QColor("#fff0f0");
    t.buttonTop = QColor(t.relief ? "#fafbfd" : "#ffffff");
    t.buttonBottom = QColor(t.relief ? "#edf1f5" : "#ffffff");
    if (!t.relief) {
        t.panelRadius = 10; t.buttonRadius = 7; t.fieldRadius = 6;
        t.channelRadius = 7; t.dayRadius = 4; t.trackRadius = 4;
        t.intervalRadius = 3; t.dialogRadius = 12;
    }
    if (t.dark) {
        t.background = QColor("#121923");
        t.surface = t.panel = t.header = t.transport = QColor("#1b2430");
        t.surface2 = t.navigation = t.tableHeader = QColor("#222f3d");
        t.field = QColor("#151f2b");
        t.fieldBorder = QColor("#75879d");
        t.text = QColor("#e7eef8"); t.secondary = QColor("#b9c7d8");
        t.muted = QColor("#9daec3");
        t.line = t.tableLine = QColor("#3b4a5e");
        t.accentText = QColor("#93b4ee"); t.accentSoft = QColor("#1d3552");
        t.accentLine = QColor("#75879d"); t.selection = QColor("#243b57");
        t.focus = QColor("#a7c4f5");
        t.success = QColor("#8cd8b0"); t.successBg = QColor("#16392c");
        t.warning = QColor("#f0c783"); t.warningBg = QColor("#49351d");
        t.error = QColor("#ffadb5"); t.errorBg = QColor("#46252e");
        t.buttonTop = QColor(t.relief ? "#293748" : "#222f3d");
        t.buttonBottom = QColor(t.relief ? "#202c3a" : "#222f3d");
    }
    return t;
}

void rounded(QPainter &p, QRectF rect, qreal radius, const QBrush &fill,
             const QColor &border = Qt::transparent)
{
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(border.alpha() ? QPen(border, 1) : QPen(Qt::NoPen));
    p.setBrush(fill);
    p.drawRoundedRect(rect.adjusted(.5, .5, -.5, -.5), radius, radius);
}

QBrush gradient(const QRectF &rect, const QColor &top, const QColor &bottom)
{
    // CSS 145deg: light from the upper left, independent of widget aspect.
    const qreal length = .574 * rect.width() + .819 * rect.height();
    const QPointF vector(.574 * length / 2, .819 * length / 2);
    QLinearGradient g(rect.center() - vector, rect.center() + vector);
    g.setColorAt(0, top); g.setColorAt(1, bottom);
    return g;
}

void shadow(QPainter &p, const QRectF &rect, qreal radius, QPointF offset,
            qreal blur, QColor color)
{
    // Layered coverage approximates the Gaussian CSS shadow without an opaque
    // bitmap, and scales with the paint device's pixel ratio.
    p.save();
    p.setPen(Qt::NoPen);
    const int spread = qCeil(blur * .85);
    const qreal sigma = qMax<qreal>(1, blur / 2.0);
    qreal previous = 0;
    for (int i = spread; i >= 0; --i) {
        const qreal coverage = .5 * std::erfc(i / (sigma * std::sqrt(2.0)));
        QColor layer = color;
        layer.setAlphaF(qBound(0.0, color.alphaF() * (coverage - previous), 1.0));
        p.setBrush(layer);
        const QRectF r = rect.translated(offset).adjusted(-i, -i, i, i);
        p.drawRoundedRect(r, radius + i, radius + i);
        previous = coverage;
    }
    p.restore();
}

void inset(QPainter &p, QRectF rect, qreal radius)
{
    const auto &t = currentTokens;
    if (!t.relief) return;
    p.save();
    QPainterPath clip; clip.addRoundedRect(rect, radius, radius);
    p.setClipPath(clip, Qt::IntersectClip);
    const QColor dark = t.dark ? QColor(0, 0, 0, 122) : QColor(37, 61, 89, 33);
    const QColor light = t.dark ? QColor(93, 118, 149, 26) : QColor(Qt::white);
    for (int i = 5; i > 0; --i) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(alpha(dark, qMax(1, dark.alpha() / (i * 3))), i * 1.1));
        p.drawRoundedRect(rect.translated(1.4, 1.4).adjusted(-i, -i, i, i), radius, radius);
        p.setPen(QPen(alpha(light, qMax(1, light.alpha() / (i * 5))), i * 1.0));
        p.drawRoundedRect(rect.translated(-1.2, -1.2).adjusted(-i, -i, i, i), radius, radius);
    }
    p.restore();
}

void paintWidgetShadow(QPainter &painter, QWidget *owner, const QRectF &rect)
{
    const auto &t = currentTokens;
    if (!t.relief || !owner->isEnabled()) return;
    const QString role = owner->property("restyleRole").toString();
    auto *button = qobject_cast<QAbstractButton *>(owner);
    const bool pressed = button && (button->isDown() || button->isChecked());
    const QString surfaceRole = owner->property("restyleSurface").toString();
    const bool panel = surfaceRole == "panel" || surfaceRole == "selection";
    if (!panel && !button && role != "picker") return;
    if (qobject_cast<QCheckBox *>(owner)) return;
    if (pressed || role == "danger" || role == "nav" || role == "day" || role == "flat") return;
    const qreal radius = panel ? (surfaceRole == "selection" ? (t.relief ? 12 : 9) : t.panelRadius) : t.buttonRadius;
    QColor dark = t.dark ? QColor(0, 0, 0, panel ? 97 : 107) : QColor(37, 61, 89, panel ? 32 : 38);
    QColor light = t.dark ? QColor(93, 118, 149, panel ? 28 : 31) : QColor(Qt::white);
    if (role == "primary" || role == "play") dark = t.dark ? QColor(0, 0, 0, 107) : alpha(t.accent, 69);
    const qreal shift = panel ? 5 : 3;
    shadow(painter, rect, radius, QPointF(shift, shift), panel ? 12 : 6, dark);
    shadow(painter, rect, radius, QPointF(-shift, -shift), panel ? 12 : 6, light);
}

// SVG endpoint arcs are converted to cubic Beziers. This keeps the original
// 26 frozen SVG paths intact and does not add an undeclared QtSvg dependency.
void arcTo(QPainterPath &path, qreal rx, qreal ry, qreal rotation,
           bool largeArc, bool sweep, QPointF end)
{
    const QPointF start = path.currentPosition();
    rx = std::abs(rx); ry = std::abs(ry);
    if (qFuzzyIsNull(rx) || qFuzzyIsNull(ry) || start == end) { path.lineTo(end); return; }
    const qreal phi = qDegreesToRadians(rotation);
    const qreal cp = std::cos(phi), sp = std::sin(phi);
    const QPointF d = (start - end) / 2;
    const qreal xp = cp*d.x() + sp*d.y(), yp = -sp*d.x() + cp*d.y();
    const qreal scale = xp*xp/(rx*rx) + yp*yp/(ry*ry);
    if (scale > 1) { rx *= std::sqrt(scale); ry *= std::sqrt(scale); }
    const qreal numerator = qMax<qreal>(0, rx*rx*ry*ry-rx*rx*yp*yp-ry*ry*xp*xp);
    const qreal denominator = rx*rx*yp*yp+ry*ry*xp*xp;
    const qreal coefficient = (largeArc == sweep ? -1 : 1)
            * std::sqrt(numerator / qMax<qreal>(denominator, .0000001));
    const qreal cxp = coefficient*rx*yp/ry, cyp = -coefficient*ry*xp/rx;
    const QPointF center(cp*cxp-sp*cyp+(start.x()+end.x())/2,
                         sp*cxp+cp*cyp+(start.y()+end.y())/2);
    auto angle = [](qreal x1, qreal y1, qreal x2, qreal y2) {
        return std::atan2(x1*y2-y1*x2, x1*x2+y1*y2);
    };
    const qreal ux=(xp-cxp)/rx, uy=(yp-cyp)/ry;
    const qreal vx=(-xp-cxp)/rx, vy=(-yp-cyp)/ry;
    qreal theta=std::atan2(uy,ux), delta=angle(ux,uy,vx,vy);
    if (!sweep && delta>0) delta-=2*M_PI;
    if (sweep && delta<0) delta+=2*M_PI;
    const int pieces=qMax(1,qCeil(std::abs(delta)/(M_PI/2)));
    delta/=pieces;
    auto point = [&](qreal a) {
        return center+QPointF(cp*rx*std::cos(a)-sp*ry*std::sin(a),
                             sp*rx*std::cos(a)+cp*ry*std::sin(a));
    };
    auto tangent = [&](qreal a) {
        return QPointF(-cp*rx*std::sin(a)-sp*ry*std::cos(a),
                       -sp*rx*std::sin(a)+cp*ry*std::cos(a));
    };
    for (int i=0;i<pieces;++i) {
        const qreal factor=4.0/3*std::tan(delta/4);
        path.cubicTo(point(theta)+factor*tangent(theta),
                     point(theta+delta)-factor*tangent(theta+delta),point(theta+delta));
        theta+=delta;
    }
}

QPainterPath parsePath(const QString &source)
{
    const QByteArray data=source.toLatin1();
    const char *s=data.constData();
    QPainterPath path;
    char command=0, previous=0;
    QPointF cubicControl, subpath;
    auto skip=[&]() { while (*s && (std::isspace(static_cast<unsigned char>(*s)) || *s==',')) ++s; };
    auto number=[&]() {
        skip(); char *end=nullptr; const qreal value=std::strtod(s,&end);
        if (end==s) { if (*s) ++s; return qreal(0); }
        s=end; return value;
    };
    while (*s) {
        skip(); if (!*s) break;
        if (std::isalpha(static_cast<unsigned char>(*s))) command=*s++;
        const bool relative=std::islower(static_cast<unsigned char>(command));
        const char kind=static_cast<char>(std::toupper(static_cast<unsigned char>(command)));
        const QPointF origin=relative ? path.currentPosition() : QPointF();
        auto point=[&]() { const qreal x=number(), y=number(); return QPointF(x,y)+origin; };
        switch (kind) {
        case 'M': { const QPointF p=point(); path.moveTo(p); subpath=p; command=relative?'l':'L'; break; }
        case 'L': path.lineTo(point()); break;
        case 'H': path.lineTo(number()+origin.x(),path.currentPosition().y()); break;
        case 'V': path.lineTo(path.currentPosition().x(),number()+origin.y()); break;
        case 'C': { const QPointF a=point(), b=point(), c=point(); path.cubicTo(a,b,c); cubicControl=b; break; }
        case 'S': { const QPointF a=(previous=='C'||previous=='S')?2*path.currentPosition()-cubicControl:path.currentPosition();
                    const QPointF b=point(), c=point(); path.cubicTo(a,b,c); cubicControl=b; break; }
        case 'A': { const qreal rx=number(), ry=number(), rotation=number();
                    const bool large=number()!=0, sweep=number()!=0; const QPointF end=point();
                    arcTo(path,rx,ry,rotation,large,sweep,end); break; }
        case 'Z': path.closeSubpath(); path.moveTo(subpath); command=0; break;
        default: if (*s) ++s; break;
        }
        previous=kind;
    }
    return path;
}

const QPainterPath &iconPath(const QString &name)
{
    static QHash<QString,QPainterPath> paths;
    if (!paths.contains(name)) {
        QFile file(":/restyle/icons/"+name+".svg");
        QPainterPath path;
        if (file.open(QIODevice::ReadOnly)) {
            QXmlStreamReader xml(&file);
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement() && xml.name()==QLatin1String("path"))
                    path.addPath(parsePath(xml.attributes().value("d").toString()));
            }
        }
        paths.insert(name,path);
    }
    return paths[name];
}

class VectorIcon : public QIconEngine
{
public:
    VectorIcon(QString name, QColor color) : name(std::move(name)), color(std::move(color)) {}
    QIconEngine *clone() const override { return new VectorIcon(name,color); }
    void paint(QPainter *p,const QRect &r,QIcon::Mode mode,QIcon::State) override
    {
        QColor paintColor=color.isValid()?color:currentTokens.secondary;
        if (mode==QIcon::Disabled) paintColor=currentTokens.muted;
        else if (!color.isValid() && mode==QIcon::Selected) paintColor=currentTokens.accentText;
        Restyle::paintIcon(*p,r,name,paintColor);
    }
    QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state) override
    {
        QPixmap result(size); result.fill(Qt::transparent);
        QPainter p(&result); paint(&p,QRect(QPoint(),size),mode,state); return result;
    }
    QPixmap scaledPixmap(const QSize &size,QIcon::Mode mode,QIcon::State state,qreal scale) override
    {
        QPixmap result(size*scale); result.fill(Qt::transparent); result.setDevicePixelRatio(scale);
        QPainter p(&result); paint(&p,QRect(QPoint(),size),mode,state); return result;
    }
private:
    QString name;
    QColor color;
};

QString buttonRole(const QWidget *widget)
{
    if (!widget) return {};
    QString role=widget->property("restyleRole").toString();
    if (role.isEmpty()) {
        const auto *push=qobject_cast<const QPushButton *>(widget);
        if (push && push->isDefault()) role=QStringLiteral("primary");
    }
    return role;
}

QImage blurredBackdrop(QImage image)
{
    image = image.convertToFormat(QImage::Format_RGB32);
    QImage scratch(image.size(), QImage::Format_RGB32);
    // CSS blur(3px) is a Gaussian standard deviation of three logical pixels.
    // Two separable box passes of radius three approximate that distribution
    // without a graphics-effect recursion through the native widget tree.
    const int radius = qMax(1, qRound(3 * image.devicePixelRatio()));
    for (int pass = 0; pass < 2; ++pass) {
        for (int y = 0; y < image.height(); ++y) {
            const QRgb *source = reinterpret_cast<const QRgb *>(image.constScanLine(y));
            QRgb *target = reinterpret_cast<QRgb *>(scratch.scanLine(y));
            int r = 0, g = 0, b = 0;
            for (int x = -radius; x <= radius; ++x) {
                const QRgb c = source[qBound(0,x,image.width()-1)];
                r += qRed(c); g += qGreen(c); b += qBlue(c);
            }
            const int count = radius * 2 + 1;
            for (int x = 0; x < image.width(); ++x) {
                target[x] = qRgb(r/count,g/count,b/count);
                const QRgb old = source[qBound(0,x-radius,image.width()-1)];
                const QRgb next = source[qBound(0,x+radius+1,image.width()-1)];
                r += qRed(next)-qRed(old); g += qGreen(next)-qGreen(old); b += qBlue(next)-qBlue(old);
            }
        }
        for (int x = 0; x < image.width(); ++x) {
            int r = 0, g = 0, b = 0;
            for (int y = -radius; y <= radius; ++y) {
                const QRgb c = reinterpret_cast<const QRgb *>(scratch.constScanLine(qBound(0,y,image.height()-1)))[x];
                r += qRed(c); g += qGreen(c); b += qBlue(c);
            }
            const int count = radius * 2 + 1;
            for (int y = 0; y < image.height(); ++y) {
                reinterpret_cast<QRgb *>(image.scanLine(y))[x] = qRgb(r/count,g/count,b/count);
                const QRgb old = reinterpret_cast<const QRgb *>(scratch.constScanLine(qBound(0,y-radius,image.height()-1)))[x];
                const QRgb next = reinterpret_cast<const QRgb *>(scratch.constScanLine(qBound(0,y+radius+1,image.height()-1)))[x];
                r += qRed(next)-qRed(old); g += qGreen(next)-qGreen(old); b += qBlue(next)-qBlue(old);
            }
        }
    }
    return image;
}

class ModalBackdrop final : public QWidget
{
public:
    explicit ModalBackdrop(QWidget *parent) : QWidget(parent)
    {
        setObjectName(QStringLiteral("modalBackdrop"));
        setProperty("restyleOwnPaint", true);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        snapshot = blurredBackdrop(parent->grab().toImage());
        setGeometry(parent->rect());
    }
protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.drawImage(rect(), snapshot);
        painter.fillRect(rect(), currentTokens.dark ? QColor(4,9,16,158) : QColor(38,60,84,79));
    }
private:
    QImage snapshot;
};

void focusRing(QPainter &p, const QRectF &rect, qreal radius)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(currentTokens.focus, 3)); p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect.adjusted(1.5,1.5,-1.5,-1.5),radius,radius);
    p.restore();
}

void paintButton(QPainter &p, const QRectF &rect, const QString &role,
                 QStyle::State state)
{
    const auto &t=currentTokens;
    const bool enabled=state & QStyle::State_Enabled;
    const bool pressed=state & QStyle::State_Sunken;
    const bool checked=state & QStyle::State_On;
    const bool hovered=state & QStyle::State_MouseOver;
    const bool primary=role=="primary" || role=="play";
    const bool plain=role=="nav" || role=="day" || role=="flat";
    const int radius=role=="day"?t.dayRadius:(role=="play"?11:t.buttonRadius);
    QBrush fill=gradient(rect,t.buttonTop,t.buttonBottom);
    QColor border=t.dark?t.fieldBorder:(t.relief?QColor(255,255,255,179):QColor("#dce1e6"));
    if (plain) { fill=Qt::NoBrush; border=Qt::transparent; }
    if (role=="quiet") {
        fill=t.dark?t.surface2:(t.relief?QColor("#f4f7fa"):QColor(Qt::transparent));
        if (!t.relief) border=Qt::transparent;
    }
    if (role=="danger") {
        fill=t.dark?t.errorBg:QColor("#fff5f5");
        border=t.dark?t.error:QColor("#eedbdd");
    }
    if (primary) {
        const QColor start=t.dark?QColor("#416ab3"):Restyle::mix(t.accent,.78,Qt::white);
        fill=t.relief?gradient(rect,start,t.accent):QBrush(t.accent);
        border=t.dark?t.fieldBorder:Restyle::mix(t.accent,.68,Qt::white);
        if (!t.relief) border=t.accent;
        if (hovered && !t.relief) fill=t.dark?QColor("#416ab3"):t.accent.lighter(107);
    } else if (hovered && !pressed && !checked && role!="danger") {
        fill=t.dark?QBrush(QColor("#293748")):QBrush(Restyle::mix(t.accent,.03,t.surface));
    }
    if (checked) {
        fill=t.selection;
        if (role=="nav" && t.relief && !t.dark) fill=QColor("#eef2f6");
        if (role=="day" && !t.dark)
            fill=t.relief?gradient(rect,Qt::white,QColor("#f1f4f7")):QBrush(Qt::white);
        border=t.dark?t.fieldBorder:(t.relief?QColor(255,255,255,166):t.accentLine);
    }
    if (pressed) fill=primary?QBrush(t.dark?QColor("#284e92"):t.accent.darker(110)):QBrush(t.field);
    if (!enabled) { fill=t.field; border=t.line; }
    QRectF r=rect;
    if (pressed && t.relief) r.translate(0,1);
    rounded(p,r,radius,fill,border);
    if (enabled && t.relief && (pressed || (checked && role!="day"))) inset(p,r,radius);
    if (enabled && t.relief && primary && !pressed) {
        p.save();
        p.setPen(QPen(t.dark?QColor(150,177,210,31):QColor(255,255,255,122),1));
        p.drawLine(r.topLeft()+QPointF(radius,1.5),r.topRight()+QPointF(-radius,1.5));
        p.restore();
    }
    if ((state & QStyle::State_HasFocus) && (state & QStyle::State_KeyboardFocusChange))
        focusRing(p,r,radius);
}

class ManagerStyle final : public QProxyStyle
{
public:
    ManagerStyle() : QProxyStyle(QStyleFactory::create("Fusion")) {}
    void polish(QPalette &p) override { p=Restyle::palette(); }
    void polish(QWidget *widget) override
    {
        QProxyStyle::polish(widget);
        widget->setAttribute(Qt::WA_Hover);
        widget->installEventFilter(this);
        if (auto *button=qobject_cast<QAbstractButton *>(widget)) {
            if (!qobject_cast<QCheckBox *>(button)) {
                if (!button->testAttribute(Qt::WA_SetFont))
                    button->setFont(Restyle::font(11,QFont::DemiBold));
            }
        }
        if (auto *line=qobject_cast<QLineEdit *>(widget)) {
            const bool nested=qobject_cast<QAbstractSpinBox *>(line->parentWidget())
                    || qobject_cast<QComboBox *>(line->parentWidget());
            // The outer complex control already reserves its field padding
            // and arrow area. Repeating margins here clips compact HH:mm and
            // date controls, and overriding the inherited font enlarges them.
            line->setTextMargins(nested?0:8,0,nested?0:8,0);
            if (!nested && line->maximumHeight()>=30)
                line->setMinimumHeight(qMax(30,line->minimumHeight()));
            if (!line->testAttribute(Qt::WA_SetFont) && !nested)
                line->setFont(Restyle::font(11));
        }
        if (auto *combo=qobject_cast<QComboBox *>(widget)) {
            if (combo->maximumHeight()>=30) combo->setMinimumHeight(qMax(30,combo->minimumHeight()));
            if (!combo->testAttribute(Qt::WA_SetFont)) combo->setFont(Restyle::font(11));
            if (widget->objectName()=="appearancePicker" || widget->objectName()=="themePicker") {
                combo->setFont(Restyle::font(11,QFont::DemiBold));
                combo->setProperty("restyleRole","picker");

            }
        }
        if (auto *spin=qobject_cast<QAbstractSpinBox *>(widget)) {
            if (spin->maximumHeight()>=30) spin->setMinimumHeight(qMax(30,spin->minimumHeight()));
            if (!spin->testAttribute(Qt::WA_SetFont)) spin->setFont(Restyle::font(11));
        }
        if (auto *header=qobject_cast<QHeaderView *>(widget)) {
            if (!header->testAttribute(Qt::WA_SetFont)) header->setFont(Restyle::font(10));
            header->setDefaultAlignment(Qt::AlignLeft|Qt::AlignVCenter);
            header->setMinimumSectionSize(28);
        }
        if (auto *view=qobject_cast<QAbstractItemView *>(widget)) {
            view->setFrameShape(QFrame::NoFrame);
            view->setAttribute(Qt::WA_Hover);
            view->viewport()->setAttribute(Qt::WA_Hover);
        }
        if (qobject_cast<QDialog *>(widget) && widget->isWindow()) {
            widget->setAutoFillBackground(true);
            if (widget->property("restyleSurface").toString().isEmpty())
                widget->setProperty("restyleSurface","dialog");
        }
        if (qobject_cast<QMenu *>(widget)) {
            widget->setFont(Restyle::font(12));
            widget->setAutoFillBackground(true);
        }
    }
    void unpolish(QWidget *widget) override
    {
        widget->removeEventFilter(this);
        QProxyStyle::unpolish(widget);
    }
    int pixelMetric(PixelMetric metric,const QStyleOption *option=nullptr,
                    const QWidget *widget=nullptr) const override
    {
        switch (metric) {
        case PM_DefaultFrameWidth: return 1;
        case PM_ButtonMargin: return 12;
        case PM_ButtonDefaultIndicator: return 0;
        case PM_ButtonShiftHorizontal: return 0;
        case PM_ButtonShiftVertical: return currentTokens.relief?1:0;
        case PM_SmallIconSize: return 16;
        case PM_ButtonIconSize: return 15;
        case PM_IndicatorWidth: case PM_IndicatorHeight:
        case PM_ExclusiveIndicatorWidth: case PM_ExclusiveIndicatorHeight: return 15;
        case PM_CheckBoxLabelSpacing: case PM_RadioButtonLabelSpacing: return 7;
        case PM_ScrollBarExtent: return 10;
        case PM_SliderThickness: return 20;
        case PM_SliderControlThickness: return 16;
        case PM_SliderLength: return 16;
        case PM_HeaderMargin: return 8;
        case PM_ToolTipLabelFrameWidth: return 7;
        case PM_MenuPanelWidth: return 1;
        case PM_MenuHMargin: case PM_MenuVMargin: return 5;
        case PM_LayoutHorizontalSpacing: return 8;
        case PM_LayoutVerticalSpacing: return 8;
        default: return QProxyStyle::pixelMetric(metric,option,widget);
        }
    }
    int styleHint(StyleHint hint,const QStyleOption *option=nullptr,
                  const QWidget *widget=nullptr,QStyleHintReturn *ret=nullptr) const override
    {
        switch (hint) {
        case SH_ComboBox_Popup: return false;
        case SH_ItemView_ShowDecorationSelected: return true;
        case SH_UnderlineShortcut: return false;
        case SH_ScrollBar_Transient: return false;
        case SH_EtchDisabledText: return false;
        case SH_DialogButtonBox_ButtonsHaveIcons: return false;
        default:return QProxyStyle::styleHint(hint,option,widget,ret);
        }
    }
    QSize sizeFromContents(ContentsType type,const QStyleOption *option,const QSize &size,
                           const QWidget *widget=nullptr) const override
    {
        QSize result=QProxyStyle::sizeFromContents(type,option,size,widget);
        if (type==CT_PushButton) {
            result.setHeight(qMax(32,size.height()+12));
            const QString role=buttonRole(widget);
            if (const auto *button=qstyleoption_cast<const QStyleOptionButton *>(option)) {
                const QFont f=role=="nav"?Restyle::font(12,QFont::DemiBold):
                            (widget?widget->font():Restyle::font(11,QFont::DemiBold));
                const int textWidth=QFontMetrics(f).size(Qt::TextShowMnemonic,button->text).width();
                const int gap=button->icon.isNull() || button->text.isEmpty()?0:(role=="nav"?9:6);
                const int iconWidth=button->icon.isNull()?0:button->iconSize.width();
                const int padding=role=="day"?8:22;
                result.setWidth(textWidth+iconWidth+gap+padding);
            } else result.setWidth(qMax(result.width(),size.width()+22));
            if (role=="icon") result=QSize(29,29);
            if (role=="play") result=QSize(34,34);
            if (role=="stop") result=QSize(30,30);
        } else if (type==CT_LineEdit || type==CT_ComboBox || type==CT_SpinBox) {
            result.setHeight(qMax(30,result.height()));
        } else if (type==CT_MenuItem) {
            result.setHeight(qMax(30,result.height()+6));
        } else if (type==CT_HeaderSection) {
            result.setHeight(qMax(30,result.height()));
        }
        return result;
    }
    QRect subElementRect(SubElement element,const QStyleOption *option,
                         const QWidget *widget=nullptr) const override
    {
        if (element==SE_LineEditContents) return option->rect.adjusted(2,2,-2,-2);
        if (element==SE_PushButtonContents) {
            const QString role=buttonRole(widget);
            const int padding=role=="day"?3:(role=="icon" || role=="play" || role=="stop"?2:10);
            return option->rect.adjusted(padding,3,-padding,-3);
        }
        return QProxyStyle::subElementRect(element,option,widget);
    }
    QRect subControlRect(ComplexControl control,const QStyleOptionComplex *option,
                         SubControl sub,const QWidget *widget=nullptr) const override
    {
        if (control==CC_ComboBox) {
            if (sub==SC_ComboBoxArrow) return QRect(option->rect.right()-25,option->rect.top(),24,option->rect.height());
            if (sub==SC_ComboBoxEditField) return option->rect.adjusted(9,2,-27,-2);
        }
        if (control==CC_SpinBox) {
            const QRect r=option->rect;
            if (sub==SC_SpinBoxUp) return QRect(r.right()-21,r.top()+2,19,(r.height()-4)/2);
            if (sub==SC_SpinBoxDown) return QRect(r.right()-21,r.center().y(),19,(r.height()-4)/2);
            if (sub==SC_SpinBoxEditField) return r.adjusted(4,2,-22,-2);
        }
        return QProxyStyle::subControlRect(control,option,sub,widget);
    }
    void drawPrimitive(PrimitiveElement element,const QStyleOption *option,QPainter *p,
                       const QWidget *widget=nullptr) const override
    {
        const auto &t=currentTokens;
        p->save();
        switch (element) {
        case PE_PanelButtonCommand: case PE_PanelButtonTool:
            paintButton(*p,option->rect,buttonRole(widget),option->state); break;
        case PE_PanelLineEdit: case PE_FrameLineEdit:
            if (widget && (qobject_cast<const QAbstractSpinBox *>(widget->parentWidget())
                    || qobject_cast<const QComboBox *>(widget->parentWidget()))) break;
            rounded(*p,option->rect,t.fieldRadius,t.field,t.fieldBorder);
            inset(*p,option->rect,t.fieldRadius);
            if (option->state&State_HasFocus) focusRing(*p,option->rect,t.fieldRadius);
            break;
        case PE_FrameFocusRect:
            if (option->state&State_KeyboardFocusChange) focusRing(*p,option->rect,t.buttonRadius);
            break;
        case PE_IndicatorCheckBox: case PE_IndicatorRadioButton: {
            const bool checked=option->state&(State_On|State_NoChange);
            const bool enabled=option->state&State_Enabled;
            const QRectF r=option->rect.adjusted(1,1,-1,-1);
            rounded(*p,r,element==PE_IndicatorRadioButton?7:3,
                    checked&&enabled?t.accent:t.field,enabled?t.fieldBorder:t.line);
            if (checked) {
                if (element==PE_IndicatorRadioButton) {
                    p->setPen(Qt::NoPen); p->setBrush(Qt::white); p->drawEllipse(r.adjusted(4,4,-4,-4));
                } else if (option->state&State_NoChange) {
                    p->setPen(QPen(enabled?QColor(Qt::white):t.muted,2));
                    p->drawLine(r.left()+3,r.center().y(),r.right()-3,r.center().y());
                } else Restyle::paintIcon(*p,r.adjusted(1,1,-1,-1),"check",enabled?QColor(Qt::white):t.muted);
            }
            break;
        }
        case PE_IndicatorArrowDown: case PE_IndicatorArrowUp:
        case PE_IndicatorArrowLeft: case PE_IndicatorArrowRight: {
            p->setRenderHint(QPainter::Antialiasing);
            p->translate(option->rect.center());
            if (element==PE_IndicatorArrowDown) p->rotate(90);
            if (element==PE_IndicatorArrowUp) p->rotate(-90);
            if (element==PE_IndicatorArrowLeft) p->rotate(180);
            QPainterPath arrow; arrow.moveTo(-2.5,-4); arrow.lineTo(1.5,0); arrow.lineTo(-2.5,4);
            p->setPen(QPen(option->state&State_Enabled?t.secondary:t.muted,1.5,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
            p->setBrush(Qt::NoBrush); p->drawPath(arrow); break;
        }
        case PE_PanelItemViewItem: {
            const auto *item=qstyleoption_cast<const QStyleOptionViewItem *>(option);
            if (option->state&State_Selected) p->fillRect(option->rect,t.selection);
            else if (option->state&State_MouseOver) p->fillRect(option->rect,t.surface2);
            else if (item && item->backgroundBrush.style()!=Qt::NoBrush) p->fillRect(option->rect,item->backgroundBrush);
            break;
        }
        case PE_PanelTipLabel:
            rounded(*p,option->rect,7,t.panel,t.fieldBorder); break;
        case PE_PanelMenu: case PE_FrameMenu:
            rounded(*p,option->rect,t.buttonRadius,t.panel,t.line); break;
        case PE_Frame:
            if (qobject_cast<const QAbstractItemView *>(widget)) break;
            rounded(*p,option->rect,t.fieldRadius,t.surface,t.line); break;
        default:
            p->restore(); QProxyStyle::drawPrimitive(element,option,p,widget); return;
        }
        p->restore();
    }
    void drawControl(ControlElement element,const QStyleOption *option,QPainter *p,
                     const QWidget *widget=nullptr) const override
    {
        const auto &t=currentTokens;
        p->save();
        switch (element) {
        case CE_PushButton: {
            const auto *button=qstyleoption_cast<const QStyleOptionButton *>(option);
            if (!button) break;
            const QString role=buttonRole(widget);
            paintButton(*p,option->rect,role,option->state);
            QStyleOptionButton label=*button;
            label.rect=subElementRect(SE_PushButtonContents,option,widget);
            drawControl(CE_PushButtonLabel,&label,p,widget); break;
        }
        case CE_PushButtonLabel: case CE_ToolButtonLabel: {
            QString text; QIcon icon; QSize iconSize(15,15);
            if (const auto *button=qstyleoption_cast<const QStyleOptionButton *>(option)) {
                text=button->text; icon=button->icon; iconSize=button->iconSize;
            } else if (const auto *button=qstyleoption_cast<const QStyleOptionToolButton *>(option)) {
                text=button->text; icon=button->icon; iconSize=button->iconSize;
                if (button->toolButtonStyle==Qt::ToolButtonIconOnly) text.clear();
                if (button->toolButtonStyle==Qt::ToolButtonTextOnly) icon=QIcon();
            }
            const QString role=buttonRole(widget);
            const bool enabled=option->state&State_Enabled;
            QColor color=t.text;
            if (role=="primary" || role=="play") color=Qt::white;
            if (role=="quiet" || role=="flat") color=t.secondary;
            if (role=="danger") color=t.dark?t.error:QColor("#a63a43");
            if (option->state&State_On) color=t.accentText;
            if (!enabled) color=t.muted;
            QRect rect=option->rect;
            if ((option->state&State_Sunken) && t.relief) rect.translate(0,1);
            QFont f=widget?widget->font():Restyle::font(11,QFont::DemiBold);
            if (role=="nav") f=Restyle::font(12,(option->state&State_On)?QFont::DemiBold:QFont::Normal);
            p->setFont(f); p->setPen(color);
            const QFontMetrics fm(f);
            const int gap=icon.isNull() || text.isEmpty()?0:(role=="nav"?9:6);
            const int iconWidth=icon.isNull()?0:iconSize.width();
            const int textWidth=qMin(qMax(0,rect.width()-iconWidth-gap),fm.horizontalAdvance(text));
            const int width=textWidth+(icon.isNull()?0:iconSize.width()+gap);
            int left=rect.left()+(rect.width()-width)/2;
            if (!icon.isNull()) {
                const QRect ir(left,rect.center().y()-iconSize.height()/2,iconSize.width(),iconSize.height());
                // A monochrome mask lets shared source vectors follow primary,
                // checked and disabled text without reloading every QAction.
                QPixmap source=icon.pixmap(iconSize,p->device()->devicePixelRatioF(),enabled?QIcon::Normal:QIcon::Disabled);
                QPainter tint(&source); tint.setCompositionMode(QPainter::CompositionMode_SourceIn); tint.fillRect(source.rect(),color); tint.end();
                p->drawPixmap(ir,source);
                left+=iconSize.width()+gap;
            }
            p->drawText(QRect(left,rect.top(),textWidth+1,rect.height()),
                        Qt::AlignLeft|Qt::AlignVCenter|Qt::TextShowMnemonic,
                        fm.elidedText(text,Qt::ElideRight,textWidth+1));
            break;
        }
        case CE_HeaderSection:
            p->fillRect(option->rect,t.tableHeader);
            p->setPen(t.line); p->drawLine(option->rect.bottomLeft(),option->rect.bottomRight()); break;
        case CE_HeaderLabel: {
            const auto *header=qstyleoption_cast<const QStyleOptionHeader *>(option);
            if (header) {
                p->setFont(Restyle::font(10)); p->setPen(t.muted);
                p->drawText(option->rect.adjusted(8,0,-8,0),header->textAlignment,
                            p->fontMetrics().elidedText(header->text,Qt::ElideRight,option->rect.width()-16));
            }
            break;
        }
        case CE_MenuItem: {
            const auto *menu=qstyleoption_cast<const QStyleOptionMenuItem *>(option);
            if (!menu) break;
            if (menu->menuItemType==QStyleOptionMenuItem::Separator) {
                p->setPen(t.line); p->drawLine(option->rect.left()+8,option->rect.center().y(),option->rect.right()-8,option->rect.center().y()); break;
            }
            if (option->state&State_Selected) rounded(*p,option->rect,5,t.selection);
            p->setFont(widget?widget->font():Restyle::font(12));
            p->setPen(option->state&State_Enabled?t.text:t.muted);
            QRect r=option->rect.adjusted(28,0,-18,0);
            const QStringList parts=menu->text.split('\t');
            p->drawText(r,Qt::AlignLeft|Qt::AlignVCenter,parts.first());
            if (parts.size()>1) p->drawText(r,Qt::AlignRight|Qt::AlignVCenter,parts.last());
            if (!menu->icon.isNull()) menu->icon.paint(p,QRect(option->rect.left()+6,option->rect.center().y()-8,16,16));
            else if (menu->checked) Restyle::paintIcon(*p,QRect(option->rect.left()+6,option->rect.center().y()-8,16,16),"check",t.accentText);
            if (menu->menuItemType==QStyleOptionMenuItem::SubMenu)
                Restyle::paintIcon(*p,QRect(option->rect.right()-17,option->rect.center().y()-6,12,12),"chevron",t.secondary);
            break;
        }
        case CE_ProgressBarGroove:
            rounded(*p,option->rect,6,t.field,t.line); break;
        case CE_ProgressBarContents: {
            const auto *progress=qstyleoption_cast<const QStyleOptionProgressBar *>(option);
            if (progress) {
                const qreal value=progress->maximum>progress->minimum?
                            qBound(0.0,qreal(progress->progress-progress->minimum)/(progress->maximum-progress->minimum),1.0):.35;
                QRectF r=option->rect; r.setWidth(r.width()*value); rounded(*p,r,6,t.accent);
            }
            break;
        }
        case CE_ScrollBarAddLine: case CE_ScrollBarSubLine: break;
        case CE_ScrollBarAddPage: case CE_ScrollBarSubPage:
            p->fillRect(option->rect,t.surface); break;
        case CE_ScrollBarSlider:
            rounded(*p,QRectF(option->rect).adjusted(2,2,-2,-2),3,t.dark?QColor("#75879d"):QColor("#b7c1cc")); break;
        default:
            p->restore(); QProxyStyle::drawControl(element,option,p,widget); return;
        }
        p->restore();
    }
    void drawComplexControl(ComplexControl control,const QStyleOptionComplex *option,
                            QPainter *p,const QWidget *widget=nullptr) const override
    {
        const auto &t=currentTokens;
        p->save();
        if (control==CC_ComboBox || control==CC_SpinBox) {
            if (widget && widget->property("restyleRole").toString()=="picker")
                paintButton(*p,option->rect,"normal",option->state);
            else {
                rounded(*p,option->rect,t.fieldRadius,t.field,t.fieldBorder);
                inset(*p,option->rect,t.fieldRadius);
            }
            if (option->state&State_HasFocus) focusRing(*p,option->rect,t.fieldRadius);
            QStyleOption arrow; arrow.state=option->state;
            if (control==CC_ComboBox) {
                arrow.rect=subControlRect(control,option,SC_ComboBoxArrow,widget);
                drawPrimitive(PE_IndicatorArrowDown,&arrow,p,widget);
            } else {
                if (option->subControls&SC_SpinBoxUp) {
                    arrow.rect=subControlRect(control,option,SC_SpinBoxUp,widget).adjusted(4,1,-4,-1);
                    drawPrimitive(PE_IndicatorArrowUp,&arrow,p,widget);
                }
                if (option->subControls&SC_SpinBoxDown) {
                    arrow.rect=subControlRect(control,option,SC_SpinBoxDown,widget).adjusted(4,1,-4,-1);
                    drawPrimitive(PE_IndicatorArrowDown,&arrow,p,widget);
                }
            }
        } else if (control==CC_Slider) {
            const auto *slider=qstyleoption_cast<const QStyleOptionSlider *>(option);
            if (slider) {
                const bool horizontal=slider->orientation==Qt::Horizontal;
                QRect groove=subControlRect(CC_Slider,option,SC_SliderGroove,widget);
                QRect handle=subControlRect(CC_Slider,option,SC_SliderHandle,widget);
                const int thickness=t.relief?7:16;
                if (horizontal) {
                    groove.setTop(option->rect.center().y()-thickness/2); groove.setHeight(thickness);
                    handle.setTop(option->rect.center().y()-8); handle.setHeight(16);
                } else {
                    groove.setLeft(option->rect.center().x()-thickness/2); groove.setWidth(thickness);
                    handle.setLeft(option->rect.center().x()-8); handle.setWidth(16);
                }
                rounded(*p,groove,8,t.dark?t.field:QColor("#e7edf3"));
                inset(*p,groove,8);
                QRect active=groove;
                if (horizontal) {
                    if (slider->upsideDown) active.setLeft(handle.center().x());
                    else active.setRight(handle.center().x());
                } else {
                    if (slider->upsideDown) active.setTop(handle.center().y());
                    else active.setBottom(handle.center().y());
                }
                rounded(*p,active,8,option->state&State_Enabled?t.accent:t.muted);
                const QRectF knob=QRectF(handle).adjusted(1,1,-1,-1);
                rounded(*p,knob,7,option->state&State_Enabled?(t.dark?t.accentText:t.accent):t.muted);
                if ((option->state&State_HasFocus) && (option->state&State_KeyboardFocusChange))
                    focusRing(*p,handle,8);
            }
        } else if (control==CC_ToolButton) {
            const auto *button=qstyleoption_cast<const QStyleOptionToolButton *>(option);
            if (button) {
                paintButton(*p,option->rect,buttonRole(widget),option->state);
                QStyleOptionToolButton label=*button;
                label.rect=option->rect.adjusted(3,2,-3,-2);
                drawControl(CE_ToolButtonLabel,&label,p,widget);
            }
        } else {
            p->restore(); QProxyStyle::drawComplexControl(control,option,p,widget); return;
        }
        p->restore();
    }
protected:
    bool eventFilter(QObject *object,QEvent *event) override
    {
        auto *widget=qobject_cast<QWidget *>(object);
        if (widget && event->type()==QEvent::Paint) {
            const QString role=widget->property("restyleSurface").toString();
            if (!widget->property("restyleOwnPaint").toBool()) {
                QPainter painter(widget);
                if (!role.isEmpty()) Restyle::paintSurface(painter,widget->rect(),role);
                Restyle::paintChildShadows(painter,widget);
            }
        }
        if (widget && widget->parentWidget()
                && (event->type()==QEvent::EnabledChange || event->type()==QEvent::MouseButtonPress
                    || event->type()==QEvent::MouseButtonRelease || event->type()==QEvent::FocusIn
                    || event->type()==QEvent::FocusOut))
            widget->parentWidget()->update(widget->geometry().adjusted(-16,-16,16,16));
        if (widget && (event->type()==QEvent::Hide || event->type()==QEvent::Close)) {
            const QPointer<ModalBackdrop> backdrop=backdrops.take(widget);
            if (backdrop) delete backdrop;
        }
        if (widget && widget->isWindow() && event->type()==QEvent::Show && qobject_cast<QDialog *>(widget)) {
            // Keep native message/trial/progress windows inside the real work
            // area as well as the explicitly laid-out editor dialogs.
            QScreen *screen=widget->screen();
            if (screen) {
                const QRect available=screen->availableGeometry();
                const QSize frame=widget->frameGeometry().size()-widget->size();
                const QSize limit=available.size()-frame-QSize(12,12);
                if (widget->width()>limit.width() || widget->height()>limit.height())
                    widget->resize(widget->size().boundedTo(limit));
                QPoint top=widget->frameGeometry().topLeft();
                if (widget->parentWidget()) {
                    QWidget *parentWindow=widget->parentWidget()->window();
                    top=parentWindow->mapToGlobal(parentWindow->rect().center())
                            -QPoint(widget->width()/2,widget->height()/2);
                }
                top.setX(qBound(available.left(),top.x(),qMax(available.left(),available.right()-widget->frameGeometry().width())));
                top.setY(qBound(available.top(),top.y(),qMax(available.top(),available.bottom()-widget->frameGeometry().height())));
                widget->move(top);
            }
            auto *dialog=qobject_cast<QDialog *>(widget);
            if (dialog->isModal() && dialog->parentWidget() && !backdrops.value(widget)) {
                auto *backdrop=new ModalBackdrop(dialog->parentWidget()->window());
                backdrops.insert(widget,backdrop);
                backdrop->show();
                backdrop->raise();
                dialog->raise();
                connect(dialog,&QObject::destroyed,this,[this,widget] {
                    const QPointer<ModalBackdrop> backdrop=backdrops.take(widget);
                    if (backdrop) delete backdrop;
                });
            }
        }
        return QProxyStyle::eventFilter(object,event);
    }
private:
    QHash<QWidget *,QPointer<ModalBackdrop>> backdrops;
};
}

namespace Restyle {
QColor mix(const QColor &first,qreal amount,const QColor &second)
{
    const qreal a=qBound(0.0,amount,1.0);
    return QColor(qRound(first.red()*a+second.red()*(1-a)),
                  qRound(first.green()*a+second.green()*(1-a)),
                  qRound(first.blue()*a+second.blue()*(1-a)),
                  qRound(first.alpha()*a+second.alpha()*(1-a)));
}

const Tokens &tokens()
{
    if (!currentTokens.background.isValid()) currentTokens=makeTokens(currentAppearance,currentTheme);
    return currentTokens;
}
QString appearanceId() { return currentAppearance; }
QString themeId() { return currentTheme; }
QStringList appearanceIds() { return {"tide","tide-relief"}; }
QStringList themeIds() { return {"denim","slate","pine","berry","graphite","pearl","dark"}; }
QString appearanceName(const QString &id)
{ return id=="tide"?QStringLiteral("Оригинал"):QStringLiteral("Рельеф"); }
QString themeName(const QString &id)
{
    static const QHash<QString,QString> names={{"denim",QStringLiteral("Деним")},
        {"slate",QStringLiteral("Сланец")},{"pine",QStringLiteral("Хвоя")},
        {"berry",QStringLiteral("Ягодная")},{"graphite",QStringLiteral("Графит")},
        {"pearl",QStringLiteral("Жемчуг")},{"dark",QStringLiteral("Тёмная")}};
    return names.value(id,QStringLiteral("Деним"));
}
QString fontFamily() { loadFont(); return chosenFont; }
bool verifiedCyrillicFont() { loadFont(); return fontVerified; }
QFont font(int pixels,int weight,qreal letterSpacing)
{
    loadFont();
    QFont f(chosenFont); f.setPixelSize(pixels); f.setWeight(QFont::Weight(weight));
    f.setLetterSpacing(QFont::AbsoluteSpacing,letterSpacing);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}

QPalette palette()
{
    const auto &t=tokens();
    QPalette p;
    for (QPalette::ColorGroup group:{QPalette::Active,QPalette::Inactive,QPalette::Disabled}) {
        const bool disabled=group==QPalette::Disabled;
        p.setColor(group,QPalette::Window,t.background);
        p.setColor(group,QPalette::WindowText,disabled?t.muted:t.text);
        p.setColor(group,QPalette::Base,t.panel);
        p.setColor(group,QPalette::AlternateBase,t.surface2);
        p.setColor(group,QPalette::Text,disabled?t.muted:t.text);
        p.setColor(group,QPalette::PlaceholderText,t.muted);
        p.setColor(group,QPalette::Button,disabled?t.field:t.buttonBottom);
        p.setColor(group,QPalette::ButtonText,disabled?t.muted:t.text);
        p.setColor(group,QPalette::Highlight,t.selection);
        p.setColor(group,QPalette::HighlightedText,disabled?t.muted:t.text);
        p.setColor(group,QPalette::Link,t.accentText);
        p.setColor(group,QPalette::LinkVisited,t.accentText);
        p.setColor(group,QPalette::BrightText,Qt::white);
        p.setColor(group,QPalette::ToolTipBase,t.panel);
        p.setColor(group,QPalette::ToolTipText,t.text);
        p.setColor(group,QPalette::Light,t.dark?QColor("#75879d"):QColor(Qt::white));
        p.setColor(group,QPalette::Midlight,t.surface2);
        p.setColor(group,QPalette::Mid,t.line);
        p.setColor(group,QPalette::Dark,t.fieldBorder);
        p.setColor(group,QPalette::Shadow,t.dark?QColor(Qt::black):QColor("#253d59"));
        p.setColor(group,QPalette::Accent,t.accent);
    }
    return p;
}

void install(QApplication &application,const QString &appearance,const QString &theme)
{
    currentAppearance=appearanceIds().contains(appearance)?appearance:QStringLiteral("tide-relief");
    currentTheme=themeIds().contains(theme)?theme:QStringLiteral("denim");
    currentTokens=makeTokens(currentAppearance,currentTheme);
    loadFont();
    application.setStyle(new ManagerStyle);
    application.setFont(font());
    application.setPalette(palette());
}

void apply(const QString &appearance,const QString &theme)
{
    currentAppearance=appearanceIds().contains(appearance)?appearance:QStringLiteral("tide-relief");
    currentTheme=themeIds().contains(theme)?theme:QStringLiteral("denim");
    currentTokens=makeTokens(currentAppearance,currentTheme);
    const QPalette p=palette();
    QApplication::setPalette(p);
    // Explicit per-widget palettes from legacy dialogs cannot pin old light
    // colors. Reset them without rebuilding widgets or their model indexes.
    const auto widgets=QApplication::allWidgets();
    for (QWidget *widget:widgets) {
        widget->setPalette(p);
        if (widget->graphicsEffect()) widget->graphicsEffect()->update();
        QEvent changed(QEvent::ApplicationPaletteChange);
        QApplication::sendEvent(widget,&changed);
        widget->updateGeometry();
        widget->update();
    }
}

void paintIcon(QPainter &p,const QRectF &rect,const QString &name,const QColor &color)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const qreal scale=qMin(rect.width(),rect.height())/24;
    p.translate(rect.center()); p.scale(scale,scale); p.translate(-12,-12);
    p.setPen(QPen(color,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    p.setBrush(Qt::NoBrush); p.drawPath(iconPath(name)); p.restore();
}
QIcon icon(const QString &name,const QColor &color) { return QIcon(new VectorIcon(name,color)); }

void button(QWidget *widget,const QString &role)
{
    if (!widget) return;
    widget->setProperty("restyleRole",role);
    widget->setFont(font(role=="nav"?12:role=="day"?10:11,
                         role=="nav" || role=="day"?QFont::Normal:QFont::DemiBold));
    widget->setCursor(Qt::PointingHandCursor);
    if (auto *b=qobject_cast<QAbstractButton *>(widget)) b->setIconSize(QSize(role=="nav"?17:15,role=="nav"?17:15));
    if (role=="icon") widget->setFixedSize(29,29);
    else if (role=="play") widget->setFixedSize(34,34);
    else if (role=="stop") widget->setFixedSize(30,30);
    else if (role=="day") widget->setFixedHeight(26);
    else widget->setMinimumHeight(32);
    widget->update();
}

void surface(QWidget *widget,const QString &role)
{
    if (!widget) return;
    widget->setProperty("restyleSurface",role);
    widget->update();
}

void paintChildShadows(QPainter &p, QWidget *parent)
{
    if (!tokens().relief) return;
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    for (QObject *object : parent->children()) {
        auto *child = qobject_cast<QWidget *>(object);
        if (child && !child->isWindow() && child->isVisible())
            paintWidgetShadow(p, child, child->geometry());
    }
    p.restore();
}

void paintSurface(QPainter &p,const QRectF &rect,const QString &role,bool pressed)
{
    const auto &t=tokens();
    p.save();
    QColor color=t.panel, border=t.dark?t.line:(t.relief?QColor(255,255,255,207):t.line);
    qreal radius=t.panelRadius;
    if (role=="header" || role=="nav" || role=="transport" || role=="background") {
        color=role=="header"?t.header:role=="nav"?t.navigation:role=="transport"?t.transport:t.background;
        p.fillRect(rect,color);
        if (role!="background") {
            p.setPen(t.dark?t.line:(t.relief?QColor(Qt::white):t.line));
            if (role=="transport") p.drawLine(rect.topLeft(),rect.topRight());
            else p.drawLine(rect.bottomLeft(),rect.bottomRight());
        }
        p.restore(); return;
    }
    if (role=="selection") radius=t.relief?12:9;
    if (role=="dialog") { color=t.dark?t.panel:(t.relief?QColor("#f5f8fb"):QColor(Qt::white)); radius=t.dialogRadius; }
    if (role=="inset" || role=="track") { color=t.field; border=t.fieldBorder; radius=role=="track"?t.trackRadius:t.fieldRadius; }
    if (role=="selected") {
        color=t.dark?t.selection:(t.relief?mix(t.accent,.05,QColor("#f4f7fa")):t.accentSoft);
        border=t.dark?t.fieldBorder:(t.relief?QColor(255,255,255,179):t.accentLine);
        radius=t.channelRadius;
    }
    if (role=="thumbnail") {
        color=t.dark?t.surface2:QColor(t.relief?"#f2f5f8":"#f8f9fb");
        radius=t.relief?6:4; border=t.dark?t.line:(t.relief?QColor(Qt::transparent):QColor("#e5e9ed"));
        if (t.relief) {
            shadow(p,rect,radius,QPointF(2,2),4,t.dark?QColor(0,0,0,97):QColor(37,61,89,26));
            shadow(p,rect,radius,QPointF(-2,-2),4,t.dark?QColor(93,118,149,28):QColor(Qt::white));
        }
    }
    rounded(p,rect,radius,color,border);
    if (role=="inset" || role=="track" || role=="selected" || pressed) inset(p,rect,radius);
    p.restore();
}
}
