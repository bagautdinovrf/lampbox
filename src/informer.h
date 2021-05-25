#ifndef INFORMER_H
#define INFORMER_H

#include <QObject>


class Informer : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(Informer)

    explicit Informer(QObject *parent = 0);

public:
    enum INFORMER_STATUS { INFO, WARNING, ERROR };

    /**
     * @brief Instance
     * @return
     */
    static Informer& Instance();

    /**
     * @brief infoEvent
     * @param message
     */
    void infoEvent(const QString& message, INFORMER_STATUS st = Informer::INFO );

    bool isActive();

    void setInactive();

    INFORMER_STATUS status();

signals:
    void infoEventSignal(const QString&);

public:
    bool                                    mActive;
    INFORMER_STATUS                         mInformerStatus;
};

#endif // INFORMER_H
