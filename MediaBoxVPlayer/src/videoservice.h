#pragma once

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <functional>
#include <map>
#include <memory>

class QScreen;
class QVideoWidget;

namespace MediaBox {
class AudioBackend;
class VideoWindow;

class VideoService final : public QObject
{
    Q_OBJECT
public:
    using BackendFactory = std::function<AudioBackend *(QVideoWidget *, QObject *)>;
    explicit VideoService(const QString &dataDirectory, BackendFactory factory = {}, QObject *parent = nullptr);
    ~VideoService() override;
    QJsonObject execute(const QJsonObject &request);
    QJsonObject status() const;
    bool restore(QString *error);
    VideoWindow *window(const QString &id) const;

signals:
    void statusChanged();

private:
    struct Record;
    QJsonObject success() const;
    QJsonObject failure(const QString &code, const QString &message) const;
    void updateScreens();
    void reconcileScreens();
    QScreen *resolveScreen(const QString &id) const;
    void present(Record &record);
    Record &createWindow(const QString &id, const QString &name, const QString &screen, bool fullscreen);
    bool save(QString *error) const;
    QJsonObject finishMutation();
    QString m_dataDirectory;
    BackendFactory m_factory;
    std::map<QString, std::unique_ptr<Record>> m_windows;
    QHash<QScreen *, QString> m_screenIds;
    bool m_restoring = false;
    bool m_mutating = false;
    QString m_persistenceError;
};

} // namespace MediaBox
