#include "androidservice.h"

#include <QtGlobal>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#endif

namespace MediaBox::AndroidService {

void setPlaying(bool playing)
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("org/mediabox/player/MediaBoxService",
                                     "setPlaying", "(Z)V", jboolean(playing));
#else
    Q_UNUSED(playing)
#endif
}

void stop()
{
#ifdef Q_OS_ANDROID
    QJniObject::callStaticMethod<void>("org/mediabox/player/MediaBoxService", "stop", "()V");
#endif
}

} // namespace MediaBox::AndroidService
