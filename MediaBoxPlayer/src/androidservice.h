#pragma once

namespace MediaBox::AndroidService {

// These functions are no-ops on non-Android platforms.
void setPlaying(bool playing);
void stop();

} // namespace MediaBox::AndroidService
