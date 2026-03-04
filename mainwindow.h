#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QVector>

class QAction;
class QCheckBox;
class QCloseEvent;
class CountdownDialWidget;
class QEvent;
class QLabel;
class QPushButton;
class QSlider;
class QString;
class QSpinBox;
class QSystemTrayIcon;
class QTimer;
class QVBoxLayout;
class QPoint;
class QWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    enum class ThemeMode {
        Auto,
        Light,
        Dark
    };

    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void startCountdown();
    void togglePause();
    void resetCountdown();
    void onTick();
    void chooseSoundFile();
    void testSoundFile();
    void restoreFromTray();
    void exitFromTray();
    void exitApplication();
    void onHistoryTimerClicked();
    void onCustomTimerClicked(int index);
    void onCustomTimerContextMenuRequested(int index, const QPoint &globalPos);
    void onFollowSystemButtonClicked();
    void onThemeIconButtonClicked();

protected:
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void buildUi();
    void updateDisplay(int totalSeconds);
    int inputSeconds() const;
    void finishCountdown();
    void playReminderSound();
    bool playCustomSound(const QString &filePath, bool loop);
    void stopReminderSound();
    void startFallbackBeepLoop();
    void stopFallbackBeepLoop();
    bool isLoopReminderEnabled() const;
    int reminderVolumePercent() const;
    void updateVolumeDisplay();
    void showVolumePopup();
    void updateTestSoundButtonState();
    void refreshPreviewPlaybackState();
    bool shouldUseCustomSound() const;
    void ensureTrayReady();
    bool isTrayActuallyReady() const;
    void startTrayHealthCheck();
    void setupTimerPresetButtons(QVBoxLayout *mainLayout, QWidget *central);
    void refreshTimerPresetButtons();
    void applyTimerValue(int totalSeconds);
    void appendTimerHistory(int totalSeconds);
    void swapCustomTimerPresets(int fromIndex, int toIndex);
    QString customTimerDisplayName(int index) const;
    QString formatSeconds(int totalSeconds) const;
    QString currentSoundFilePath() const;
    void setSoundFilePath(const QString &filePath);
    bool isSystemDarkTheme() const;
    bool isDarkThemeActive() const;
    void applyTheme();
    void applyStatusLabelStyle(const QString &colorHex);
    void setStatusText(const QString &text, bool highlight = false);
    QString themeModeToConfigValue(ThemeMode mode) const;
    ThemeMode themeModeFromConfigValue(const QString &value) const;
    void updateThemeButtons();
    void loadConfig();
    void saveConfig() const;

    QSpinBox *m_minutesSpinBox;
    QSpinBox *m_secondsSpinBox;
    QLabel *m_timeLabel;
    QLabel *m_statusLabel;
    CountdownDialWidget *m_dialWidget;
    QLabel *m_presetHintLabel;
    QPushButton *m_historyTimerButton;
    QVector<QPushButton *> m_customTimerButtons;
    QCheckBox *m_soundCheckBox;
    QCheckBox *m_loopAlarmCheckBox;
    QPushButton *m_audioSectionToggleButton;
    QWidget *m_audioSection;
    QPushButton *m_volumeButton;
    QWidget *m_volumePopup;
    QSlider *m_volumeSlider;
    QLabel *m_soundFileNameLabel;
    QPushButton *m_browseSoundButton;
    QPushButton *m_testSoundButton;
    QPushButton *m_startButton;
    QPushButton *m_pauseButton;
    QPushButton *m_resetButton;
    QPushButton *m_followSystemButton;
    QPushButton *m_themeIconButton;
    QPushButton *m_exitButton;
    QSystemTrayIcon *m_trayIcon;
    QAction *m_showAction;
    QAction *m_exitAction;
    QTimer *m_timer;
    QTimer *m_beepLoopTimer;
    QTimer *m_previewStateTimer;

    int m_totalSeconds;
    int m_remainingSeconds;
    int m_volumePercent;
    QString m_soundFilePath;
    QString m_lastAudioError;
    bool m_isPreviewPlaying;
    bool m_audioSectionExpanded;
    QVector<QString> m_customTimerNames;
    QVector<int> m_customTimerSeconds;
    QVector<int> m_timerHistorySeconds;
    bool m_isRunning;
    bool m_isPaused;
    bool m_isExiting;
    bool m_trayHintShown;
    bool m_trayFallbackHintShown;
    bool m_trayRecoverHintShown;
    int m_trayHealthMissCount;
    ThemeMode m_themeMode;
    QString m_statusBaseColorHex;
    QString m_configFilePath;
};
#endif // MAINWINDOW_H
