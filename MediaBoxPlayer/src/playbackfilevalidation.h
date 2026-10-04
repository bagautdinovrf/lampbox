#pragma once

#include <QString>

namespace MediaBox {

// Shared validation for local playback sources; schedule decoding belongs to ScheduleV1.
QString playbackFileError(const QString &path);

} // namespace MediaBox
