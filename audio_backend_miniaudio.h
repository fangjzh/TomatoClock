#pragma once

#include <QString>

namespace AudioMini {

bool playFile(const QString &filePath, bool loop, int volumePercent, QString *errorMessage);
void setVolumePercent(int volumePercent);
void stop();
bool isPlaying();

} // namespace AudioMini
