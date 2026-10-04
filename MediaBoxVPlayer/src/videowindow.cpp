#include "videowindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMouseEvent>
#include <QScreen>
#include <QShortcut>
#include <QVBoxLayout>
#include <QVideoWidget>
#include <QWindow>

namespace MediaBox {

VideoWindow::VideoWindow(QWidget *parent)
    : QWidget(parent), m_video(new QVideoWidget(this))
{
    setWindowTitle(QStringLiteral("MediaBoxVPlayer"));
    setMinimumSize(160, 90);
    resize(960, 540);
    QPalette colors = palette();
    colors.setColor(QPalette::Window, Qt::black);
    setPalette(colors);
    setAutoFillBackground(true);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_video);
    m_video->setAspectRatioMode(Qt::KeepAspectRatio);
    m_video->setAccessibleName(tr("Видео"));
    // The platform video surface is a child of QVideoWidget. Filtering the
    // application covers that surface too, including children created later.
    qApp->installEventFilter(this);
    auto *toggle = new QShortcut(QKeySequence(Qt::Key_F11), this);
    connect(toggle, &QShortcut::activated, this, [this] { setFullscreen(!m_fullscreen); });
    auto *escape = new QShortcut(QKeySequence(Qt::Key_Escape), this);
    connect(escape, &QShortcut::activated, this, [this] { setFullscreen(false); });
}

QRect VideoWindow::boundedGeometry(QScreen *screen, const QRect &geometry) const
{
    const QRect available = screen->availableGeometry();
    QSize size = geometry.isValid() ? geometry.size() : QSize(960, 540);
    size = size.boundedTo(available.size());
    QRect result(geometry.isValid() ? geometry.topLeft() : available.center() - QPoint(size.width() / 2, size.height() / 2), size);
    result.moveLeft(qBound(available.left(), result.left(), available.right() - size.width() + 1));
    result.moveTop(qBound(available.top(), result.top(), available.bottom() - size.height() + 1));
    return result;
}

void VideoWindow::presentOn(QScreen *target, const QString &screenId)
{
    if (!target)
        return;
    if (!m_fullscreen && isVisible() && !m_screenId.isEmpty())
        m_windowedGeometries.insert(m_screenId, geometry());
    const bool changedScreen = m_screenId != screenId;
    const bool needsPlacement = changedScreen || !isVisible()
        || (windowHandle() && windowHandle()->screen() != target);
    m_screenId = screenId;
    // Force a native handle before selecting the destination monitor.
    winId();
    if (needsPlacement && isFullScreen())
        showNormal();
    windowHandle()->setScreen(target);
    if (m_fullscreen) {
        if (needsPlacement || !isFullScreen())
            setGeometry(target->geometry());
        showFullScreen();
    } else {
        setGeometry(boundedGeometry(target, m_windowedGeometries.value(screenId)));
        showNormal();
    }
}

void VideoWindow::suspend()
{
    if (!m_fullscreen && isVisible() && !m_screenId.isEmpty())
        m_windowedGeometries.insert(m_screenId, geometry());
    hide();
}

void VideoWindow::setFullscreen(bool fullscreen)
{
    if (m_fullscreen == fullscreen)
        return;
    if (fullscreen && isVisible())
        m_windowedGeometries.insert(m_screenId, geometry());
    m_fullscreen = fullscreen;
    if (isVisible()) {
        if (fullscreen) {
            showFullScreen();
        } else {
            showNormal();
            if (screen())
                setGeometry(boundedGeometry(screen(), m_windowedGeometries.value(m_screenId)));
        }
    }
    emit fullscreenChanged(fullscreen);
}

bool VideoWindow::eventFilter(QObject *watched, QEvent *event)
{
    const auto *widget = qobject_cast<QWidget *>(watched);
    if (event->type() == QEvent::MouseButtonDblClick && widget
        && (widget == this || isAncestorOf(widget))
        && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton) {
        setFullscreen(!m_fullscreen);
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void VideoWindow::closeEvent(QCloseEvent *event)
{
    // Accepting lets QApplication finish an explicit quit/SIGTERM. The main
    // application disables quitOnLastWindowClosed, so an operator closing a
    // single window only hides it and leaves the TCP service available.
    event->accept();
    emit closeRequested();
}

} // namespace MediaBox
