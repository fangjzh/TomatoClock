#include "audio_backend_miniaudio.h"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <QDir>
#include <QMutex>
#include <QMutexLocker>

namespace {
QMutex g_mutex;
ma_engine g_engine;
bool g_engineInitialized = false;
ma_sound g_sound;
bool g_soundInitialized = false;

QString maErrorToString(ma_result result)
{
    return QStringLiteral("miniaudio 错误码: %1").arg(static_cast<int>(result));
}

bool ensureEngine(QString *errorMessage)
{
    if (g_engineInitialized) {
        return true;
    }

    ma_engine_config config = ma_engine_config_init();
    const ma_result result = ma_engine_init(&config, &g_engine);
    if (result != MA_SUCCESS) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("初始化音频引擎失败（%1）").arg(maErrorToString(result));
        }
        return false;
    }

    g_engineInitialized = true;
    return true;
}
}

namespace AudioMini {

bool playFile(const QString &filePath, bool loop, int volumePercent, QString *errorMessage)
{
    QMutexLocker locker(&g_mutex);

    if (!ensureEngine(errorMessage)) {
        return false;
    }

    if (g_soundInitialized) {
        ma_sound_stop(&g_sound);
        ma_sound_uninit(&g_sound);
        g_soundInitialized = false;
    }

    const std::wstring pathW = QDir::toNativeSeparators(filePath).toStdWString();
    ma_uint32 flags = MA_SOUND_FLAG_STREAM;
    if (loop) {
        flags |= MA_SOUND_FLAG_LOOPING;
    }

    const ma_result initResult = ma_sound_init_from_file_w(&g_engine,
                                                            pathW.c_str(),
                                                            flags,
                                                            nullptr,
                                                            nullptr,
                                                            &g_sound);
    if (initResult != MA_SUCCESS) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("加载音频失败（%1）").arg(maErrorToString(initResult));
        }
        return false;
    }
    g_soundInitialized = true;

    const float volume = qBound(0, volumePercent, 100) / 100.0f;
    ma_sound_set_volume(&g_sound, volume);

    const ma_result startResult = ma_sound_start(&g_sound);
    if (startResult != MA_SUCCESS) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("启动播放失败（%1）").arg(maErrorToString(startResult));
        }
        ma_sound_uninit(&g_sound);
        g_soundInitialized = false;
        return false;
    }

    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

void setVolumePercent(int volumePercent)
{
    QMutexLocker locker(&g_mutex);

    if (!g_soundInitialized) {
        return;
    }

    const float volume = qBound(0, volumePercent, 100) / 100.0f;
    ma_sound_set_volume(&g_sound, volume);
}

void stop()
{
    QMutexLocker locker(&g_mutex);

    if (g_soundInitialized) {
        ma_sound_stop(&g_sound);
        ma_sound_uninit(&g_sound);
        g_soundInitialized = false;
    }
}

bool isPlaying()
{
    QMutexLocker locker(&g_mutex);

    if (!g_soundInitialized) {
        return false;
    }

    return ma_sound_is_playing(&g_sound) == MA_TRUE;
}

} // namespace AudioMini
