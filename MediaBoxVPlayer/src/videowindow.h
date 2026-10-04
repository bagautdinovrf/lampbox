#pragma once

#include <QMap>
#include <QWidget>

class QScreen;
class QVideoWidget;

namespace MediaBox {

class VideoWindow final : public QWidget
{
    Q_OBJECT
public:
    explicit VideoWindow(QWidget *parent = nullptr);
    QVideoWidget *videoWidget() const { return m_video; }
    bool requestedFullscreen() const { return m_fullscreen; }
    void setFullscreen(bool fullscreen);
    void presentOn(QScreen *screen, const QString &screenId);
    void suspend();

signals:
    void fullscreenChanged(bool fullscreen);
    void closeRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QRect boundedGeometry(QScreen *screen, const QRect &geometry) const;
    QVideoWidget *m_video;
    bool m_fullscreen = false;
    QString m_screenId;
    QMap<QString, QRect> m_windowedGeometries;
};

} // namespace MediaBox
