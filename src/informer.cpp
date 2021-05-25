#include "informer.h"


Informer::Informer(QObject *parent) :
    QObject(parent),
    mActive(false)
{
    //
}

Informer& Informer::Instance()
{
    static Informer _informer;
    return _informer;
}


void Informer::infoEvent(const QString& message, INFORMER_STATUS st)
{
    mActive = true;
    mInformerStatus = st;
    emit infoEventSignal(message);
}


void Informer::setInactive()
{
    mActive = false;
}

bool Informer::isActive()
{
    return mActive;
}

Informer::INFORMER_STATUS Informer::status()
{
    return mInformerStatus;
}
