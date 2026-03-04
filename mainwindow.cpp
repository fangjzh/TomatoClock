#include "alarm_timeout_dialog.h"
#include "countdown_dial_widget.h"
#include "icon_utils.h"
#include "mainwindow.h"
#include "audio_backend_miniaudio.h"

#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDir>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPalette>
#include <QPushButton>
#include <QSettings>
#include <QSlider>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSvgRenderer>
#include <QStyle>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVariantList>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <QWidget>
#include <QPainter>

#include <functional>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#include <dshow.h>
#endif

namespace {
constexpr const wchar_t *kAlarmAlias = L"TomatoClockAlarm";

class IOSSwitchButton : public QPushButton
{
public:
    explicit IOSSwitchButton(QWidget *parent = nullptr)
        : QPushButton(parent)
        , m_knobPos(0.0)
        , m_darkMode(false)
        , m_anim(new QVariantAnimation(this))
    {
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFixedSize(52, 28);
        setFocusPolicy(Qt::StrongFocus);

        m_anim->setDuration(140);
        m_anim->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_knobPos = value.toReal();
            update();
        });

        connect(this, &QPushButton::toggled, this, [this](bool checked) {
            m_anim->stop();
            m_anim->setStartValue(m_knobPos);
            m_anim->setEndValue(checked ? 1.0 : 0.0);
            m_anim->start();
        });
    }

    void setDarkMode(bool dark)
    {
        if (m_darkMode == dark) {
            return;
        }
        m_darkMode = dark;
        update();
    }

    void syncStateNoAnimation(bool checked)
    {
        m_anim->stop();
        const bool changed = isChecked() != checked;
        QPushButton::setChecked(checked);
        m_knobPos = checked ? 1.0 : 0.0;
        if (changed) {
            update();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF bounds = rect().adjusted(1.0, 1.0, -1.0, -1.0);
        const qreal radius = bounds.height() * 0.5;

        QColor trackOff = m_darkMode ? QColor(99, 111, 128) : QColor(176, 187, 201);
        QColor trackOn = QColor(52, 199, 89);
        if (!isEnabled()) {
            trackOff = m_darkMode ? QColor(72, 82, 96) : QColor(205, 213, 223);
            trackOn = m_darkMode ? QColor(67, 137, 92) : QColor(137, 194, 154);
        }
        const QColor trackColor = isChecked() ? trackOn : trackOff;

        painter.setPen(Qt::NoPen);
        painter.setBrush(trackColor);
        painter.drawRoundedRect(bounds, radius, radius);

        const qreal knobDiameter = bounds.height() - 4.0;
        const qreal knobMinX = bounds.left() + 2.0;
        const qreal knobMaxX = bounds.right() - 2.0 - knobDiameter;
        const qreal knobX = knobMinX + (knobMaxX - knobMinX) * m_knobPos;
        const QRectF knobRect(knobX, bounds.top() + 2.0, knobDiameter, knobDiameter);

        painter.setBrush(QColor(255, 255, 255));
        painter.drawEllipse(knobRect);

        if (hasFocus()) {
            QPen focusPen(m_darkMode ? QColor(133, 184, 255) : QColor(69, 133, 246));
            focusPen.setWidthF(1.2);
            painter.setPen(focusPen);
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(bounds.adjusted(0.8, 0.8, -0.8, -0.8), radius, radius);
        }
    }

private:
    qreal m_knobPos;
    bool m_darkMode;
    QVariantAnimation *m_anim;
};

class DraggablePresetButton : public QPushButton
{
public:
    explicit DraggablePresetButton(int slotIndex, QWidget *parent = nullptr)
        : QPushButton(parent)
        , m_slotIndex(slotIndex)
        , m_dragStartPos()
    {
        setAcceptDrops(true);
    }

    std::function<void(int, int)> onSwapRequested;

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_dragStartPos = event->pos();
        }
        QPushButton::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (!(event->buttons() & Qt::LeftButton)) {
            QPushButton::mouseMoveEvent(event);
            return;
        }

        if ((event->pos() - m_dragStartPos).manhattanLength() < QApplication::startDragDistance()) {
            QPushButton::mouseMoveEvent(event);
            return;
        }

        auto *drag = new QDrag(this);
        auto *mimeData = new QMimeData();
        mimeData->setData("application/x-tomatoclock-preset-index", QByteArray::number(m_slotIndex));
        drag->setMimeData(mimeData);
        drag->exec(Qt::MoveAction);
    }

    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (event->mimeData()->hasFormat("application/x-tomatoclock-preset-index")) {
            event->acceptProposedAction();
            return;
        }
        QPushButton::dragEnterEvent(event);
    }

    void dropEvent(QDropEvent *event) override
    {
        if (!event->mimeData()->hasFormat("application/x-tomatoclock-preset-index")) {
            QPushButton::dropEvent(event);
            return;
        }

        const int fromIndex = event->mimeData()->data("application/x-tomatoclock-preset-index").toInt();
        if (fromIndex != m_slotIndex && onSwapRequested) {
            onSwapRequested(fromIndex, m_slotIndex);
        }

        event->acceptProposedAction();
    }

private:
    int m_slotIndex;
    QPoint m_dragStartPos;
};

QString historyButtonStyle(bool dark)
{
    if (dark) {
        return QStringLiteral("QPushButton{background:#2f3a48;border:1px solid #4a596d;border-radius:8px;padding:6px 8px;color:#e8eef7;}"
                              "QPushButton:hover{background:#3a4859;}"
                              "QPushButton{font-size:14px;font-weight:600;}");
    }

    return QStringLiteral("QPushButton{background:#eef3f8;border:1px solid #ced9e7;border-radius:8px;padding:6px 8px;color:#1f2a35;}"
                          "QPushButton:hover{background:#e6edf6;}"
                          "QPushButton{font-size:14px;font-weight:600;}");
}

QString customButtonStyle(bool dark, int index)
{
    if (dark) {
        const QStringList darkColors {
            QStringLiteral("#4a4236"),
            QStringLiteral("#38483f"),
            QStringLiteral("#433a4b"),
            QStringLiteral("#364953"),
            QStringLiteral("#4c3b43")
        };
        const int safeIndex = qBound(0, index, darkColors.size() - 1);
        return QStringLiteral("QPushButton{background:%1;border:1px solid #5a6470;border-radius:8px;padding:5px 6px;color:#f3f6fb;}"
                              "QPushButton:hover{background:#4b5665;}"
                              "QPushButton{font-size:14px;font-weight:600;}")
            .arg(darkColors.at(safeIndex));
    }

    const QStringList pastelColors {
        QStringLiteral("#f4efdc"),
        QStringLiteral("#eaf2e7"),
        QStringLiteral("#efe9f5"),
        QStringLiteral("#e8f0f6"),
        QStringLiteral("#f3e9ec")
    };
    const int safeIndex = qBound(0, index, pastelColors.size() - 1);
    return QStringLiteral("QPushButton{background:%1;border:1px solid #d9e1ea;border-radius:8px;padding:5px 6px;color:#1f2a35;}"
                          "QPushButton:hover{background:white;}"
                          "QPushButton{font-size:14px;font-weight:600;}")
        .arg(pastelColors.at(safeIndex));
}

QString resolveRuntimeImagePath(const QString &fileName)
{
    const QString appDirPath = QCoreApplication::applicationDirPath();
    const QStringList candidates {
        QDir(appDirPath).filePath(QStringLiteral("resource/images/%1").arg(fileName)),
        QDir(appDirPath).filePath(QStringLiteral("../resource/images/%1").arg(fileName))
    };

    for (const QString &path : candidates) {
        if (QFileInfo::exists(path)) {
            return QDir::toNativeSeparators(path).replace('\\', '/');
        }
    }

    return QString();
}

QIcon renderInlineSvgIcon(const QString &svgMarkup)
{
    const QSize iconSize(16, 16);
    QPixmap pixmap(iconSize);
    pixmap.fill(Qt::transparent);

    QSvgRenderer renderer(svgMarkup.toUtf8());
    QPainter painter(&pixmap);
    renderer.render(&painter, QRectF(0, 0, iconSize.width(), iconSize.height()));

    return QIcon(pixmap);
}

QIcon playStateIcon(bool playing, bool dark)
{
    const QString fillColor = dark ? QStringLiteral("#e6edf5") : QStringLiteral("#1f2a35");
    if (playing) {
        const QString pauseSvg = QStringLiteral(
            "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
            "<rect x='6' y='5' width='4' height='14' rx='1' fill='%1'/>"
            "<rect x='14' y='5' width='4' height='14' rx='1' fill='%1'/>"
            "</svg>")
                                     .arg(fillColor);
        return renderInlineSvgIcon(pauseSvg);
    }

    const QString playSvg = QStringLiteral(
        "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 24 24'>"
        "<path d='M7 5.5v13l11-6.5z' fill='%1'/>"
        "</svg>")
                                  .arg(fillColor);
    return renderInlineSvgIcon(playSvg);
}

#ifdef _WIN32
struct DirectShowPlayer
{
    IGraphBuilder *graph = nullptr;
    IMediaControl *control = nullptr;
    IMediaSeeking *seeking = nullptr;
    bool comInitialized = false;
};

DirectShowPlayer g_directShowPlayer;

void releaseDirectShowPlayer()
{
    if (g_directShowPlayer.control) {
        g_directShowPlayer.control->Stop();
        g_directShowPlayer.control->Release();
        g_directShowPlayer.control = nullptr;
    }
    if (g_directShowPlayer.seeking) {
        g_directShowPlayer.seeking->Release();
        g_directShowPlayer.seeking = nullptr;
    }
    if (g_directShowPlayer.graph) {
        g_directShowPlayer.graph->Release();
        g_directShowPlayer.graph = nullptr;
    }
    if (g_directShowPlayer.comInitialized) {
        CoUninitialize();
        g_directShowPlayer.comInitialized = false;
    }
}

QString hresultToString(HRESULT hr)
{
    return QStringLiteral("HRESULT=%1(0x%2)")
        .arg(static_cast<unsigned int>(hr))
        .arg(QString::number(static_cast<unsigned int>(hr), 16).toUpper());
}

bool startDirectShowPlayback(const QString &path, bool loop, QString *errorMessage)
{
    Q_UNUSED(loop);
    releaseDirectShowPlayer();

    const HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (SUCCEEDED(initHr)) {
        g_directShowPlayer.comInitialized = true;
    } else if (initHr != RPC_E_CHANGED_MODE) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("DirectShow 初始化失败（%1）").arg(hresultToString(initHr));
        }
        return false;
    }

    IGraphBuilder *graph = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FilterGraph,
                                  nullptr,
                                  CLSCTX_INPROC_SERVER,
                                  IID_IGraphBuilder,
                                  reinterpret_cast<void **>(&graph));
    if (FAILED(hr) || !graph) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("DirectShow 创建图形失败（%1）").arg(hresultToString(hr));
        }
        releaseDirectShowPlayer();
        return false;
    }

    const std::wstring pathW = QDir::toNativeSeparators(path).toStdWString();
    hr = graph->RenderFile(pathW.c_str(), nullptr);
    if (FAILED(hr)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("DirectShow 加载音频失败（%1）").arg(hresultToString(hr));
        }
        graph->Release();
        releaseDirectShowPlayer();
        return false;
    }

    IMediaControl *control = nullptr;
    hr = graph->QueryInterface(IID_IMediaControl, reinterpret_cast<void **>(&control));
    if (FAILED(hr) || !control) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("DirectShow 获取控制接口失败（%1）").arg(hresultToString(hr));
        }
        graph->Release();
        releaseDirectShowPlayer();
        return false;
    }

    IMediaSeeking *seeking = nullptr;
    graph->QueryInterface(IID_IMediaSeeking, reinterpret_cast<void **>(&seeking));

    hr = control->Run();
    if (FAILED(hr)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("DirectShow 启动播放失败（%1）").arg(hresultToString(hr));
        }
        if (seeking) {
            seeking->Release();
        }
        control->Release();
        graph->Release();
        releaseDirectShowPlayer();
        return false;
    }

    g_directShowPlayer.graph = graph;
    g_directShowPlayer.control = control;
    g_directShowPlayer.seeking = seeking;
    if (errorMessage) {
        errorMessage->clear();
    }
    return true;
}

bool isDirectShowPlaying()
{
    if (!g_directShowPlayer.control) {
        return false;
    }

    OAFilterState state = State_Stopped;
    const HRESULT hr = g_directShowPlayer.control->GetState(0, &state);
    if (FAILED(hr)) {
        return false;
    }
    return state == State_Running || state == State_Paused;
}

QString mciErrorToString(MCIERROR errorCode)
{
    if (errorCode == 0) {
        return QString();
    }

    const QString codeText = QStringLiteral("错误码=%1(0x%2)")
                                 .arg(static_cast<unsigned int>(errorCode))
                                 .arg(QString::number(static_cast<unsigned int>(errorCode), 16).toUpper());

    constexpr UINT kBufferLength = 512;
    wchar_t buffer[kBufferLength] = { 0 };
    if (mciGetErrorStringW(errorCode, buffer, kBufferLength)) {
        return QStringLiteral("%1（%2）").arg(QString::fromWCharArray(buffer).trimmed(), codeText);
    }
    return QStringLiteral("MCI错误（%1）").arg(codeText);
}

QString toShortWindowsPath(const QString &path)
{
    const std::wstring longPath = path.toStdWString();
    if (longPath.empty()) {
        return QString();
    }

    const DWORD required = GetShortPathNameW(longPath.c_str(), nullptr, 0);
    if (required == 0) {
        return QString();
    }

    std::wstring shortPath(required + 1, L'\0');
    const DWORD result = GetShortPathNameW(longPath.c_str(), shortPath.data(), static_cast<DWORD>(shortPath.size()));
    if (result == 0) {
        return QString();
    }

    shortPath.resize(result);
    return QDir::toNativeSeparators(QString::fromWCharArray(shortPath.c_str()));
}

bool sendMciCommand(const QString &command, QString *errorMessage = nullptr)
{
    const std::wstring commandW = command.toStdWString();
    const MCIERROR errorCode = mciSendStringW(commandW.c_str(), nullptr, 0, nullptr);
    if (errorCode == 0) {
        return true;
    }

    if (errorMessage) {
        *errorMessage = mciErrorToString(errorCode);
    }
    return false;
}

bool tryOpenMediaByMci(const QString &normalizedPath, QString *errorMessage)
{
    struct OpenAttempt
    {
        const wchar_t *deviceType;
    };

    const QString extension = QFileInfo(normalizedPath).suffix().trimmed().toLower();
    QList<OpenAttempt> attempts;
    if (extension == QStringLiteral("mp3")) {
        attempts << OpenAttempt { L"mpegvideo" }
                 << OpenAttempt { L"MPEGVideo" }
                 << OpenAttempt { nullptr };
    } else if (extension == QStringLiteral("wav") || extension == QStringLiteral("wave")) {
        attempts << OpenAttempt { L"waveaudio" }
                 << OpenAttempt { nullptr };
    } else {
        attempts << OpenAttempt { nullptr }
                 << OpenAttempt { L"mpegvideo" }
                 << OpenAttempt { L"waveaudio" };
    }

    QStringList pathCandidates;
    pathCandidates << normalizedPath;

    const QString shortPath = toShortWindowsPath(normalizedPath);
    if (!shortPath.isEmpty() && !pathCandidates.contains(shortPath, Qt::CaseInsensitive)) {
        pathCandidates << shortPath;
    }

    QString lastError;
    QString lastAttempt;

    for (const QString &candidatePath : pathCandidates) {
        const std::wstring pathW = candidatePath.toStdWString();

        for (const OpenAttempt &attempt : attempts) {
            mciSendStringW(L"close TomatoClockAlarm", nullptr, 0, nullptr);

            MCI_OPEN_PARMSW openParams {};
            openParams.lpstrElementName = pathW.c_str();
            openParams.lpstrAlias = kAlarmAlias;
            openParams.lpstrDeviceType = attempt.deviceType;

            DWORD flags = MCI_OPEN_ELEMENT | MCI_OPEN_ALIAS;
            if (attempt.deviceType != nullptr) {
                flags |= MCI_OPEN_TYPE;
            }

            const MCIERROR errorCode = mciSendCommandW(0, MCI_OPEN, flags, reinterpret_cast<DWORD_PTR>(&openParams));
            if (errorCode == 0) {
                if (errorMessage) {
                    errorMessage->clear();
                }
                return true;
            }

            const QString deviceType = attempt.deviceType ? QString::fromWCharArray(attempt.deviceType) : QStringLiteral("auto");
            lastAttempt = QStringLiteral("路径=%1, 类型=%2").arg(candidatePath, deviceType);
            lastError = mciErrorToString(errorCode);
        }
    }

    if (errorMessage) {
        if (!lastError.isEmpty() && !lastAttempt.isEmpty()) {
            *errorMessage = QStringLiteral("%1；%2").arg(lastError, lastAttempt);
        } else {
            *errorMessage = lastError;
        }
    }
    return false;
}
#endif
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_minutesSpinBox(nullptr)
    , m_secondsSpinBox(nullptr)
    , m_timeLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_dialWidget(nullptr)
    , m_presetHintLabel(nullptr)
    , m_historyTimerButton(nullptr)
    , m_soundCheckBox(nullptr)
    , m_loopAlarmCheckBox(nullptr)
    , m_audioSectionToggleButton(nullptr)
    , m_audioSection(nullptr)
    , m_volumeButton(nullptr)
    , m_volumePopup(nullptr)
    , m_volumeSlider(nullptr)
    , m_soundFileNameLabel(nullptr)
    , m_browseSoundButton(nullptr)
    , m_testSoundButton(nullptr)
    , m_exitButton(nullptr)
    , m_startButton(nullptr)
    , m_pauseButton(nullptr)
    , m_resetButton(nullptr)
    , m_followSystemButton(nullptr)
    , m_themeIconButton(nullptr)
    , m_trayIcon(nullptr)
    , m_showAction(nullptr)
    , m_exitAction(nullptr)
    , m_timer(new QTimer(this))
    , m_beepLoopTimer(new QTimer(this))
    , m_previewStateTimer(new QTimer(this))
    , m_totalSeconds(0)
    , m_remainingSeconds(0)
    , m_volumePercent(80)
    , m_soundFilePath()
    , m_isPreviewPlaying(false)
    , m_audioSectionExpanded(false)
    , m_isRunning(false)
    , m_isPaused(false)
    , m_isExiting(false)
    , m_trayHintShown(false)
    , m_trayFallbackHintShown(false)
    , m_trayRecoverHintShown(false)
    , m_trayHealthMissCount(0)
    , m_themeMode(ThemeMode::Auto)
    , m_configFilePath(QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("config/settings.ini")))
{
    buildUi();
    ensureTrayReady();
    startTrayHealthCheck();
    loadConfig();

    m_beepLoopTimer->setInterval(1200);
    connect(m_beepLoopTimer, &QTimer::timeout, this, []() {
        QApplication::beep();
    });

    m_previewStateTimer->setInterval(250);
    connect(m_previewStateTimer, &QTimer::timeout, this, &MainWindow::refreshPreviewPlaybackState);

    connect(m_timer, &QTimer::timeout, this, &MainWindow::onTick);
    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::startCountdown);
    connect(m_pauseButton, &QPushButton::clicked, this, &MainWindow::togglePause);
    connect(m_resetButton, &QPushButton::clicked, this, &MainWindow::resetCountdown);
    connect(m_browseSoundButton, &QPushButton::clicked, this, &MainWindow::chooseSoundFile);
    connect(m_testSoundButton, &QPushButton::clicked, this, &MainWindow::testSoundFile);
    connect(m_exitButton, &QPushButton::clicked, this, &MainWindow::exitApplication);
    connect(m_followSystemButton, &QPushButton::clicked, this, &MainWindow::onFollowSystemButtonClicked);
    connect(m_themeIconButton, &QPushButton::clicked, this, &MainWindow::onThemeIconButtonClicked);
    connect(m_soundCheckBox, &QCheckBox::toggled, this, [this](bool) {
        saveConfig();
    });
    connect(m_loopAlarmCheckBox, &QCheckBox::toggled, this, [this](bool) {
        saveConfig();
    });
    connect(m_minutesSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_isRunning) {
            if (m_dialWidget) {
                m_dialWidget->setVisualState(CountdownDialWidget::VisualState::Ready);
            }
            updateDisplay(inputSeconds());
        }
    });
    connect(m_secondsSpinBox, qOverload<int>(&QSpinBox::valueChanged), this, [this](int) {
        if (!m_isRunning) {
            if (m_dialWidget) {
                m_dialWidget->setVisualState(CountdownDialWidget::VisualState::Ready);
            }
            updateDisplay(inputSeconds());
        }
    });

    m_totalSeconds = inputSeconds();
    if (m_dialWidget) {
        m_dialWidget->setVisualState(CountdownDialWidget::VisualState::Ready);
    }
    updateDisplay(inputSeconds());
}

MainWindow::~MainWindow()
{
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("番茄钟"));
    setWindowIcon(loadRuntimeAppIconFromDisk(QCoreApplication::applicationDirPath(), QApplication::style()));
    setMinimumSize(520, 560);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    auto *mainLayout = new QVBoxLayout(central);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(12, 10, 12, 10);

    auto *inputLayout = new QHBoxLayout();
    inputLayout->setSpacing(8);
    auto *minutesLabel = new QLabel(QStringLiteral("分钟:"), central);
    m_minutesSpinBox = new QSpinBox(central);
    m_minutesSpinBox->setRange(0, 180);
    m_minutesSpinBox->setValue(25);

    auto *secondsLabel = new QLabel(QStringLiteral("秒:"), central);
    m_secondsSpinBox = new QSpinBox(central);
    m_secondsSpinBox->setRange(0, 59);
    m_secondsSpinBox->setValue(0);

    auto *followLabel = new QLabel(QStringLiteral("跟随系统"), central);
    followLabel->setObjectName(QStringLiteral("followSystemLabel"));
    m_followSystemButton = new IOSSwitchButton(central);
    m_followSystemButton->setToolTip(QStringLiteral("跟随系统主题"));

    m_themeIconButton = new QPushButton(central);
    m_themeIconButton->setToolTip(QStringLiteral("切换浅色/深色"));
    m_themeIconButton->setFixedSize(28, 28);
    m_themeIconButton->setCursor(Qt::PointingHandCursor);
    m_themeIconButton->setFlat(true);
    m_themeIconButton->setText(QString());
    m_themeIconButton->setIconSize(QSize(20, 20));

    inputLayout->addWidget(minutesLabel);
    inputLayout->addWidget(m_minutesSpinBox);
    inputLayout->addSpacing(8);
    inputLayout->addWidget(secondsLabel);
    inputLayout->addWidget(m_secondsSpinBox);
    inputLayout->addStretch();
    inputLayout->addWidget(followLabel);
    inputLayout->addWidget(m_followSystemButton);
    inputLayout->addWidget(m_themeIconButton);
    mainLayout->addLayout(inputLayout);

    m_timeLabel = new QLabel(QStringLiteral("00:00"), central);
    m_timeLabel->setAlignment(Qt::AlignCenter);
    QFont timeFont = m_timeLabel->font();
    timeFont.setPointSize(34);
    timeFont.setBold(true);
    m_timeLabel->setFont(timeFont);
    m_timeLabel->setStyleSheet(QStringLiteral("QLabel{padding-top:2px;padding-bottom:0px;letter-spacing:1px;}"));
    mainLayout->addWidget(m_timeLabel);

    setupTimerPresetButtons(mainLayout, central);

    m_dialWidget = new CountdownDialWidget(central);
    m_dialWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    mainLayout->addWidget(m_dialWidget, 0, Qt::AlignHCenter);

    m_statusLabel = new QLabel(QStringLiteral("状态：就绪"), central);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setStyleSheet(QStringLiteral("QLabel{font-size:13px;padding-top:0px;padding-bottom:2px;}"));
    mainLayout->addWidget(m_statusLabel);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(10);
    m_exitButton = new QPushButton(QStringLiteral("退出"), central);
    m_startButton = new QPushButton(QStringLiteral("开始"), central);
    m_pauseButton = new QPushButton(QStringLiteral("暂停"), central);
    m_resetButton = new QPushButton(QStringLiteral("重置"), central);

    m_startButton->setObjectName(QStringLiteral("primaryActionButton"));
    m_pauseButton->setObjectName(QStringLiteral("secondaryActionButton"));
    m_resetButton->setObjectName(QStringLiteral("secondaryActionButton"));
    m_exitButton->setObjectName(QStringLiteral("dangerActionButton"));

    m_pauseButton->setEnabled(false);
    m_resetButton->setEnabled(false);
    m_startButton->setMinimumHeight(34);
    m_pauseButton->setMinimumHeight(34);
    m_resetButton->setMinimumHeight(34);
    m_exitButton->setMinimumHeight(34);

    buttonLayout->addWidget(m_startButton);
    buttonLayout->addWidget(m_pauseButton);
    buttonLayout->addWidget(m_resetButton);
    buttonLayout->addWidget(m_exitButton);
    mainLayout->addLayout(buttonLayout);

    auto *audioToggleLayout = new QHBoxLayout();
    audioToggleLayout->setSpacing(0);
    audioToggleLayout->setContentsMargins(0, 0, 0, 0);
    audioToggleLayout->addStretch();
    m_audioSectionToggleButton = new QPushButton(QStringLiteral("显示音频设置"), central);
    m_audioSectionToggleButton->setObjectName(QStringLiteral("audioToggleButton"));
    m_audioSectionToggleButton->setCursor(Qt::PointingHandCursor);
    m_audioSectionToggleButton->setMinimumHeight(24);
    audioToggleLayout->addWidget(m_audioSectionToggleButton);
    mainLayout->addLayout(audioToggleLayout);

    m_audioSection = new QWidget(central);
    m_audioSection->setObjectName(QStringLiteral("audioSection"));
    auto *soundLayout = new QVBoxLayout(m_audioSection);
    soundLayout->setContentsMargins(0, 6, 0, 0);
    soundLayout->setSpacing(5);

    auto *soundTopLayout = new QHBoxLayout();
    soundTopLayout->setSpacing(8);

    auto *soundBottomLayout = new QHBoxLayout();
    soundBottomLayout->setSpacing(8);

    m_soundCheckBox = new QCheckBox(QStringLiteral("音频提醒"), central);
    m_loopAlarmCheckBox = new QCheckBox(QStringLiteral("循环"), central);
    auto *volumeLabel = new QLabel(QStringLiteral("音量"), central);
    m_volumeButton = new QPushButton(central);
    m_volumeButton->setObjectName(QStringLiteral("volumeValueButton"));
    m_volumeButton->setToolTip(QStringLiteral("点击调节提醒音量"));
    m_volumeButton->setFixedWidth(62);
    m_volumeButton->setCursor(Qt::PointingHandCursor);
    m_soundFileNameLabel = new QLabel(QStringLiteral("未选择音乐"), central);
    m_soundFileNameLabel->setObjectName(QStringLiteral("audioFileNameLabel"));
    m_soundFileNameLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_soundFileNameLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_soundFileNameLabel->setMinimumWidth(110);
    m_browseSoundButton = new QPushButton(QStringLiteral("选择音乐"), central);
    m_browseSoundButton->setObjectName(QStringLiteral("audioActionButton"));
    m_browseSoundButton->setToolTip(QStringLiteral("选择提醒音频（WAV / MP3）"));
    m_browseSoundButton->setFixedWidth(84);
    m_testSoundButton = new QPushButton(central);
    m_testSoundButton->setObjectName(QStringLiteral("audioActionButton"));
    m_testSoundButton->setToolTip(QStringLiteral("测试播放 / 停止"));
    m_testSoundButton->setFixedWidth(72);
    m_testSoundButton->setIconSize(QSize(16, 16));

    m_soundFileNameLabel->setMinimumHeight(30);
    m_browseSoundButton->setMinimumHeight(30);
    m_testSoundButton->setMinimumHeight(30);

    soundTopLayout->addWidget(m_soundCheckBox);
    soundTopLayout->addWidget(m_loopAlarmCheckBox);
    soundTopLayout->addSpacing(10);
    soundTopLayout->addStretch();
    soundTopLayout->addWidget(volumeLabel);
    soundTopLayout->addWidget(m_volumeButton);

    soundBottomLayout->addWidget(m_browseSoundButton);
    soundBottomLayout->addWidget(m_soundFileNameLabel, 1);
    soundBottomLayout->addWidget(m_testSoundButton);

    soundLayout->addLayout(soundTopLayout);
    soundLayout->addLayout(soundBottomLayout);
    mainLayout->addWidget(m_audioSection);
    m_audioSection->setVisible(false);

    m_volumePopup = new QWidget(this, Qt::Popup);
    m_volumePopup->setObjectName(QStringLiteral("volumePopup"));
    m_volumePopup->setAttribute(Qt::WA_TranslucentBackground, true);
    m_volumePopup->setWindowFlag(Qt::FramelessWindowHint, true);
    m_volumePopup->setWindowFlag(Qt::NoDropShadowWindowHint, true);

    auto *volumePopupLayout = new QVBoxLayout(m_volumePopup);
    volumePopupLayout->setContentsMargins(2, 2, 2, 2);
    volumePopupLayout->setSpacing(0);

    m_volumeSlider = new QSlider(Qt::Vertical, m_volumePopup);
    m_volumeSlider->setObjectName(QStringLiteral("volumeSlider"));
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setPageStep(10);
    m_volumeSlider->setSingleStep(2);
    m_volumeSlider->setTracking(true);
    m_volumeSlider->setValue(m_volumePercent);
    m_volumeSlider->setToolTip(QStringLiteral("滚轮或拖动调节音量"));
    m_volumeSlider->setFixedSize(24, 156);
    volumePopupLayout->addWidget(m_volumeSlider, 0, Qt::AlignHCenter);

    updateVolumeDisplay();
    auto *volumeSaveTimer = new QTimer(this);
    volumeSaveTimer->setSingleShot(true);
    volumeSaveTimer->setInterval(220);
    connect(volumeSaveTimer, &QTimer::timeout, this, [this]() {
        saveConfig();
    });

    connect(m_volumeButton, &QPushButton::clicked, this, &MainWindow::showVolumePopup);
    connect(m_volumeSlider, &QSlider::valueChanged, this, [this](int value) {
        m_volumePercent = qBound(0, value, 100);
        updateVolumeDisplay();
        AudioMini::setVolumePercent(m_volumePercent);
    });
    connect(m_volumeSlider, &QSlider::valueChanged, volumeSaveTimer, [volumeSaveTimer](int) {
        volumeSaveTimer->start();
    });
    updateTestSoundButtonState();
    m_startButton->setShortcut(QKeySequence(QStringLiteral("Space")));
    m_pauseButton->setShortcut(QKeySequence(QStringLiteral("Ctrl+Space")));
    m_resetButton->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
    m_browseSoundButton->setShortcut(QKeySequence(QStringLiteral("Ctrl+O")));

    connect(m_audioSectionToggleButton, &QPushButton::clicked, this, [this]() {
        m_audioSectionExpanded = !m_audioSectionExpanded;
        if (m_audioSection) {
            m_audioSection->setVisible(m_audioSectionExpanded);
        }
        if (m_audioSectionToggleButton) {
            m_audioSectionToggleButton->setText(m_audioSectionExpanded
                                                    ? QStringLiteral("收起音频设置")
                                                    : QStringLiteral("显示音频设置"));
        }
    });
}

void MainWindow::startCountdown()
{
    if (m_isRunning) {
        return;
    }

    m_remainingSeconds = inputSeconds();
    if (m_remainingSeconds <= 0) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请输入大于 0 的时间。"));
        return;
    }
    m_totalSeconds = m_remainingSeconds;
    appendTimerHistory(m_remainingSeconds);

    m_isRunning = true;
    m_isPaused = false;
    m_timer->start(1000);

    m_minutesSpinBox->setEnabled(false);
    m_secondsSpinBox->setEnabled(false);
    m_startButton->setEnabled(false);
    m_pauseButton->setEnabled(true);
    m_resetButton->setEnabled(true);
    m_pauseButton->setText(QStringLiteral("暂停"));
    setStatusText(QStringLiteral("状态：计时中"), true);
    if (m_dialWidget) {
        m_dialWidget->setVisualState(CountdownDialWidget::VisualState::Running);
    }
    updateDisplay(m_remainingSeconds);
}

void MainWindow::togglePause()
{
    if (!m_isRunning) {
        return;
    }

    if (m_isPaused) {
        m_timer->start(1000);
        m_isPaused = false;
        m_pauseButton->setText(QStringLiteral("暂停"));
        setStatusText(QStringLiteral("状态：计时中"), true);
        if (m_dialWidget) {
            m_dialWidget->setVisualState(CountdownDialWidget::VisualState::Running);
        }
        return;
    }

    m_timer->stop();
    m_isPaused = true;
    m_pauseButton->setText(QStringLiteral("继续"));
    setStatusText(QStringLiteral("状态：已暂停"), true);
    if (m_dialWidget) {
        m_dialWidget->setVisualState(CountdownDialWidget::VisualState::Paused);
    }
}

void MainWindow::resetCountdown()
{
    m_timer->stop();
    m_isRunning = false;
    m_isPaused = false;
    m_remainingSeconds = inputSeconds();
    m_totalSeconds = m_remainingSeconds;

    m_minutesSpinBox->setEnabled(true);
    m_secondsSpinBox->setEnabled(true);
    m_startButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    m_resetButton->setEnabled(false);
    m_pauseButton->setText(QStringLiteral("暂停"));
    setStatusText(QStringLiteral("状态：就绪"));
    if (m_dialWidget) {
        m_dialWidget->setVisualState(CountdownDialWidget::VisualState::Ready);
    }
    updateDisplay(m_remainingSeconds);
}

void MainWindow::onTick()
{
    if (m_remainingSeconds > 0) {
        --m_remainingSeconds;
    }

    updateDisplay(m_remainingSeconds);

    if (m_remainingSeconds == 0) {
        finishCountdown();
    }
}

void MainWindow::updateDisplay(int totalSeconds)
{
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    m_timeLabel->setText(QStringLiteral("%1:%2")
                             .arg(minutes, 2, 10, QChar('0'))
                             .arg(seconds, 2, 10, QChar('0')));

    if (m_dialWidget) {
        const int dialTotal = (m_isRunning || m_isPaused) ? qMax(m_totalSeconds, 1) : qMax(inputSeconds(), 1);
        m_dialWidget->setCountdownState(dialTotal, qMax(totalSeconds, 0));
    }
}

void MainWindow::setupTimerPresetButtons(QVBoxLayout *mainLayout, QWidget *central)
{
    m_presetHintLabel = new QLabel(QStringLiteral("快捷定时：左键应用  ·  右键保存当前定时  ·  拖拽交换自定义位置"), central);
    m_presetHintLabel->setAlignment(Qt::AlignCenter);
    m_presetHintLabel->setStyleSheet(QStringLiteral("QLabel{font-size:12px;}"));
    mainLayout->addWidget(m_presetHintLabel);

    auto *presetGrid = new QGridLayout();
    presetGrid->setHorizontalSpacing(8);
    presetGrid->setVerticalSpacing(8);

    m_historyTimerButton = new QPushButton(QStringLiteral("历史定时"), central);
    m_historyTimerButton->setToolTip(QStringLiteral("点击查看并应用历史定时"));
    connect(m_historyTimerButton, &QPushButton::clicked, this, &MainWindow::onHistoryTimerClicked);
    presetGrid->addWidget(m_historyTimerButton, 0, 0);

    m_customTimerButtons.clear();
    for (int i = 0; i < 5; ++i) {
        auto *button = new DraggablePresetButton(i, central);
        button->setToolTip(QStringLiteral("左键应用；右键可保存当前定时或设置名称；按住左键拖拽到其他按钮可交换预设"));
        button->setContextMenuPolicy(Qt::CustomContextMenu);
        button->setMinimumHeight(44);
        connect(button, &QPushButton::clicked, this, [this, i]() {
            onCustomTimerClicked(i);
        });
        connect(button, &QPushButton::customContextMenuRequested, this, [this, i, button](const QPoint &) {
            onCustomTimerContextMenuRequested(i, button->mapToGlobal(QPoint(0, button->height())));
        });
        button->onSwapRequested = [this](int fromIndex, int toIndex) {
            swapCustomTimerPresets(fromIndex, toIndex);
        };

        m_customTimerButtons.append(button);
        presetGrid->addWidget(button, i < 2 ? 0 : 1, i < 2 ? (i + 1) : (i - 2));
    }

    mainLayout->addLayout(presetGrid);
    applyTheme();
}

void MainWindow::refreshTimerPresetButtons()
{
    if (m_historyTimerButton) {
        if (m_timerHistorySeconds.isEmpty()) {
            m_historyTimerButton->setText(QStringLiteral("历史定时\n暂无"));
        } else {
            m_historyTimerButton->setText(QStringLiteral("历史定时\n%1").arg(formatSeconds(m_timerHistorySeconds.first())));
        }
    }

    for (int i = 0; i < m_customTimerButtons.size(); ++i) {
        const int seconds = (i < m_customTimerSeconds.size()) ? m_customTimerSeconds.at(i) : 0;
        const QString name = customTimerDisplayName(i);
        if (seconds > 0) {
            m_customTimerButtons.at(i)->setText(QStringLiteral("%1\n%2")
                                                    .arg(name)
                                                    .arg(formatSeconds(seconds)));
        } else {
            m_customTimerButtons.at(i)->setText(QStringLiteral("%1\n未设置").arg(name));
        }
    }
}

void MainWindow::applyTimerValue(int totalSeconds)
{
    if (m_isRunning) {
        QMessageBox::information(this,
                                 QStringLiteral("定时按钮"),
                                 QStringLiteral("请先暂停并重置当前计时，再应用新的定时时间。"));
        return;
    }

    if (totalSeconds <= 0) {
        return;
    }

    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    m_minutesSpinBox->setValue(minutes);
    m_secondsSpinBox->setValue(seconds);

    m_totalSeconds = totalSeconds;
    if (m_dialWidget) {
        m_dialWidget->setVisualState(CountdownDialWidget::VisualState::Ready);
    }
    setStatusText(QStringLiteral("状态：已应用 %1").arg(formatSeconds(totalSeconds)), true);
    QTimer::singleShot(1000, this, [this]() {
        if (!m_isRunning && !m_isPaused && m_statusLabel) {
            setStatusText(QStringLiteral("状态：就绪"));
        }
    });
    updateDisplay(totalSeconds);
}

void MainWindow::appendTimerHistory(int totalSeconds)
{
    if (totalSeconds <= 0) {
        return;
    }

    m_timerHistorySeconds.removeAll(totalSeconds);
    m_timerHistorySeconds.prepend(totalSeconds);

    const int maxHistory = 20;
    if (m_timerHistorySeconds.size() > maxHistory) {
        m_timerHistorySeconds.resize(maxHistory);
    }

    refreshTimerPresetButtons();
    saveConfig();
}

void MainWindow::swapCustomTimerPresets(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= m_customTimerSeconds.size()
        || toIndex < 0 || toIndex >= m_customTimerSeconds.size()
        || fromIndex == toIndex) {
        return;
    }

    qSwap(m_customTimerSeconds[fromIndex], m_customTimerSeconds[toIndex]);
    qSwap(m_customTimerNames[fromIndex], m_customTimerNames[toIndex]);
    refreshTimerPresetButtons();
    saveConfig();
}

QString MainWindow::customTimerDisplayName(int index) const
{
    if (index < 0 || index >= m_customTimerButtons.size()) {
        return QString();
    }

    if (index < m_customTimerNames.size()) {
        const QString trimmedName = m_customTimerNames.at(index).trimmed();
        if (!trimmedName.isEmpty()) {
            return trimmedName;
        }
    }

    return QStringLiteral("自定义%1").arg(index + 1);
}

QString MainWindow::formatSeconds(int totalSeconds) const
{
    const int minutes = totalSeconds / 60;
    const int seconds = totalSeconds % 60;
    return QStringLiteral("%1:%2")
        .arg(minutes, 2, 10, QChar('0'))
        .arg(seconds, 2, 10, QChar('0'));
}

void MainWindow::onHistoryTimerClicked()
{
    if (m_timerHistorySeconds.isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("历史定时"),
                                 QStringLiteral("暂无历史定时记录。"));
        return;
    }

    QMenu menu(this);
    const int displayCount = qMin(10, m_timerHistorySeconds.size());
    for (int i = 0; i < displayCount; ++i) {
        const int seconds = m_timerHistorySeconds.at(i);
        QAction *action = menu.addAction(QStringLiteral("%1").arg(formatSeconds(seconds)));
        connect(action, &QAction::triggered, this, [this, seconds]() {
            applyTimerValue(seconds);
        });
    }

    const QPoint anchor = m_historyTimerButton
                              ? m_historyTimerButton->mapToGlobal(QPoint(0, m_historyTimerButton->height()))
                              : mapToGlobal(QPoint(width() / 2, height() / 2));
    menu.exec(anchor);
}

void MainWindow::onCustomTimerClicked(int index)
{
    if (index < 0 || index >= m_customTimerSeconds.size()) {
        return;
    }

    const int seconds = m_customTimerSeconds.at(index);
    if (seconds <= 0) {
        QMessageBox::information(this,
                                 QStringLiteral("自定义定时"),
                                 QStringLiteral("该按钮还未设置时间。\n请右键按钮保存当前分钟/秒。"));
        return;
    }

    applyTimerValue(seconds);
}

void MainWindow::onCustomTimerContextMenuRequested(int index, const QPoint &globalPos)
{
    if (index < 0 || index >= m_customTimerSeconds.size()) {
        return;
    }

    QMenu menu(this);
    QAction *saveAction = menu.addAction(QStringLiteral("保存当前定时"));
    QAction *renameAction = menu.addAction(QStringLiteral("设置名称"));
    QAction *cancelAction = menu.addAction(QStringLiteral("取消"));

    QAction *selected = menu.exec(globalPos);
    if (selected == saveAction) {
        const int currentSeconds = inputSeconds();
        if (currentSeconds <= 0) {
            QMessageBox::warning(this,
                                 QStringLiteral("自定义定时"),
                                 QStringLiteral("请输入大于 0 的时间后再保存。"));
            return;
        }

        m_customTimerSeconds[index] = currentSeconds;
        refreshTimerPresetButtons();
        saveConfig();
        return;
    }

    if (selected == renameAction) {
        bool ok = false;
        const QString currentName = customTimerDisplayName(index);
        const QString newName = QInputDialog::getText(this,
                                                      QStringLiteral("设置名称"),
                                                      QStringLiteral("请输入按钮名称（最多 8 个字）："),
                                                      QLineEdit::Normal,
                                                      currentName,
                                                      &ok);
        if (!ok) {
            return;
        }

        QString trimmed = newName.trimmed();
        if (trimmed.size() > 8) {
            trimmed = trimmed.left(8);
        }

        if (index >= 0 && index < m_customTimerNames.size()) {
            m_customTimerNames[index] = trimmed;
            refreshTimerPresetButtons();
            saveConfig();
        }
        return;
    }

    if (selected == cancelAction) {
        return;
    }
}

int MainWindow::inputSeconds() const
{
    return m_minutesSpinBox->value() * 60 + m_secondsSpinBox->value();
}

void MainWindow::finishCountdown()
{
    m_timer->stop();
    m_isRunning = false;
    m_isPaused = false;

    m_minutesSpinBox->setEnabled(true);
    m_secondsSpinBox->setEnabled(true);
    m_startButton->setEnabled(true);
    m_pauseButton->setEnabled(false);
    m_resetButton->setEnabled(false);
    m_pauseButton->setText(QStringLiteral("暂停"));
    setStatusText(QStringLiteral("状态：已完成"), true);
    if (m_dialWidget) {
        m_dialWidget->setVisualState(CountdownDialWidget::VisualState::Finished);
    }

    playReminderSound();

    if (isHidden() && m_trayIcon && m_trayIcon->isVisible()) {
        m_trayIcon->showMessage(QStringLiteral("番茄钟"),
                                QStringLiteral("时间到！"),
                                QSystemTrayIcon::Information,
                                5000);
        restoreFromTray();
    }

    AlarmTimeoutDialog dialog(this);
    dialog.exec();
    stopReminderSound();
}

void MainWindow::chooseSoundFile()
{
    QString initialDir = QStringLiteral("C:/Windows/Media");
    const QString currentPath = currentSoundFilePath();
    if (!currentPath.trimmed().isEmpty()) {
        const QFileInfo selectedInfo(currentPath);
        if (selectedInfo.exists() && selectedInfo.dir().exists()) {
            initialDir = selectedInfo.absolutePath();
        }
    }

#ifdef _WIN32
    const QString defaultSoundDir = initialDir;
#else
    const QString defaultSoundDir = initialDir;
#endif

    const QString filePath = QFileDialog::getOpenFileName(this,
                                                          QStringLiteral("选择提醒音频文件"),
                                                          defaultSoundDir,
                                                          QStringLiteral("音频文件 (*.wav *.mp3)"));
    if (filePath.isEmpty()) {
        return;
    }

    setSoundFilePath(filePath);
    m_soundCheckBox->setChecked(true);
    saveConfig();
}

void MainWindow::testSoundFile()
{
    if (m_isPreviewPlaying) {
        stopReminderSound();
        return;
    }

    const QString filePath = currentSoundFilePath();
    if (filePath.trimmed().isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("音频测试"),
                                 QStringLiteral("请先选择一个音频文件（WAV/MP3）。"));
        return;
    }

    if (!QFileInfo::exists(filePath)) {
        QMessageBox::warning(this,
                             QStringLiteral("音频测试"),
                             QStringLiteral("所选文件不存在，请重新选择。"));
        return;
    }

    stopReminderSound();
    if (!playCustomSound(filePath, false)) {
        const QString detail = m_lastAudioError.trimmed();
        QMessageBox::warning(this,
                             QStringLiteral("音频测试"),
                             detail.isEmpty()
                                 ? QStringLiteral("音频播放失败，将使用系统蜂鸣。")
                                 : QStringLiteral("音频播放失败，将使用系统蜂鸣。\n原因：%1").arg(detail));
        QApplication::beep();
        m_isPreviewPlaying = false;
        updateTestSoundButtonState();
        return;
    }

    m_isPreviewPlaying = true;
    updateTestSoundButtonState();
    if (m_previewStateTimer) {
        m_previewStateTimer->start();
    }
}

void MainWindow::restoreFromTray()
{
    showNormal();
    activateWindow();
    raise();
}

void MainWindow::exitFromTray()
{
    m_isExiting = true;
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    qApp->quit();
}

void MainWindow::exitApplication()
{
    m_isExiting = true;
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    close();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (m_isExiting) {
        event->accept();
        return;
    }

    ensureTrayReady();
    const bool trayReady = isTrayActuallyReady();

    if (!trayReady) {
        const QMessageBox::StandardButton choice = QMessageBox::question(
            this,
            QStringLiteral("番茄钟"),
            QStringLiteral("系统托盘不可用。\n\n选择“是”直接退出，选择“否”最小化到任务栏。"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (choice == QMessageBox::Yes) {
            m_isExiting = true;
            event->accept();
            return;
        }

        showMinimized();
        event->ignore();
        if (!m_trayFallbackHintShown) {
            QMessageBox::information(this,
                                     QStringLiteral("番茄钟"),
                                     QStringLiteral("系统托盘暂不可用，已最小化到任务栏。"));
            m_trayFallbackHintShown = true;
        }
        return;
    }
    event->ignore();

    m_trayIcon->setIcon(windowIcon());
    m_trayIcon->show();
    if (QSystemTrayIcon::supportsMessages()) {
        m_trayIcon->showMessage(QStringLiteral("番茄钟"),
                                QStringLiteral("应用已最小化到系统托盘。"),
                                QSystemTrayIcon::Information,
                                3000);
    }

    QTimer::singleShot(120, this, [this]() {
        ensureTrayReady();
        const bool trayActuallyReady = isTrayActuallyReady();

        if (trayActuallyReady) {
            hide();
            return;
        }

        showMinimized();
        if (!m_trayFallbackHintShown) {
            QMessageBox::warning(this,
                                 QStringLiteral("番茄钟"),
                                 QStringLiteral("系统托盘暂不可用，已保留在任务栏，避免后台不可见。"));
            m_trayFallbackHintShown = true;
        }
    });
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (m_themeMode == ThemeMode::Auto
        && (event->type() == QEvent::ApplicationPaletteChange
            || event->type() == QEvent::StyleChange
            || event->type() == QEvent::ActivationChange)) {
        applyTheme();
    }

    if (event->type() != QEvent::WindowStateChange) {
        return;
    }

    if (!isMinimized()) {
        m_trayHealthMissCount = 0;
        return;
    }

    ensureTrayReady();
    const bool trayReady = isTrayActuallyReady();
    if (!trayReady) {
        return;
    }

    hide();
    if (!m_trayHintShown) {
        m_trayIcon->showMessage(QStringLiteral("番茄钟"),
                                QStringLiteral("应用已最小化到系统托盘。"),
                                QSystemTrayIcon::Information,
                                3000);
        m_trayHintShown = true;
    }
}

void MainWindow::onFollowSystemButtonClicked()
{
    if (!m_followSystemButton) {
        return;
    }

    if (m_followSystemButton->isChecked()) {
        m_themeMode = ThemeMode::Auto;
    } else {
        m_themeMode = isSystemDarkTheme() ? ThemeMode::Dark : ThemeMode::Light;
    }

    applyTheme();
    saveConfig();
}

void MainWindow::onThemeIconButtonClicked()
{
    if (m_themeMode == ThemeMode::Auto) {
        return;
    }

    const bool currentlyDark = isDarkThemeActive();

    m_themeMode = currentlyDark ? ThemeMode::Light : ThemeMode::Dark;

    applyTheme();
    saveConfig();
}

bool MainWindow::isSystemDarkTheme() const
{
#ifdef _WIN32
    QSettings settings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize"),
                       QSettings::NativeFormat);
    const int appUseLightTheme = settings.value(QStringLiteral("AppsUseLightTheme"), 1).toInt();
    return appUseLightTheme == 0;
#else
    return false;
#endif
}

bool MainWindow::isDarkThemeActive() const
{
    if (m_themeMode == ThemeMode::Dark) {
        return true;
    }
    if (m_themeMode == ThemeMode::Light) {
        return false;
    }
    return isSystemDarkTheme();
}

void MainWindow::applyTheme()
{
    const bool dark = isDarkThemeActive();
    QWidget *central = centralWidget();
    const QString spinUpArrow = resolveRuntimeImagePath(QStringLiteral("spin_up.svg"));
    const QString spinDownArrow = resolveRuntimeImagePath(QStringLiteral("spin_down.svg"));
    const QString volumeTrackSvg = resolveRuntimeImagePath(dark ? QStringLiteral("volume_track_dark.svg")
                                                                : QStringLiteral("volume_track_light.svg"));
    const QString volumeHandleSvg = resolveRuntimeImagePath(dark ? QStringLiteral("volume_handle_dark.svg")
                                                                 : QStringLiteral("volume_handle_light.svg"));

    if (dark) {
        QPalette palette;
        palette.setColor(QPalette::Window, QColor(37, 42, 50));
        palette.setColor(QPalette::WindowText, QColor(233, 238, 245));
        palette.setColor(QPalette::Base, QColor(28, 33, 40));
        palette.setColor(QPalette::AlternateBase, QColor(43, 49, 58));
        palette.setColor(QPalette::ToolTipBase, QColor(52, 58, 68));
        palette.setColor(QPalette::ToolTipText, QColor(240, 243, 248));
        palette.setColor(QPalette::Text, QColor(233, 238, 245));
        palette.setColor(QPalette::Button, QColor(52, 59, 69));
        palette.setColor(QPalette::ButtonText, QColor(236, 240, 247));
        palette.setColor(QPalette::Highlight, QColor(74, 140, 255));
        palette.setColor(QPalette::HighlightedText, QColor(250, 250, 250));
        qApp->setPalette(palette);
    } else {
        qApp->setPalette(qApp->style()->standardPalette());
    }

    if (central) {
        central->setStyleSheet(QStringLiteral(
                                   "QWidget{background:%1;color:%2;}"
                                   "QSpinBox,QLineEdit{background:%3;border:1px solid %4;border-radius:8px;padding:4px 6px;color:%2;}"
                                   "QSpinBox{padding-right:26px;}"
                                   "QSpinBox::up-button,QSpinBox::down-button{width:18px;border-left:1px solid %4;background:%8;}"
                                   "QSpinBox::up-button{subcontrol-origin:border;subcontrol-position:top right;border-top-right-radius:8px;}"
                                   "QSpinBox::down-button{subcontrol-origin:border;subcontrol-position:bottom right;border-bottom-right-radius:8px;}"
                                   "QSpinBox::up-button:hover,QSpinBox::down-button:hover{background:%9;}"
                                   "QSpinBox::up-arrow{image:url(%10);width:10px;height:10px;}"
                                   "QSpinBox::down-arrow{image:url(%11);width:10px;height:10px;}"
                                   "QSpinBox:disabled,QLineEdit:disabled{background:%5;color:%6;}"
                                   "QCheckBox{spacing:6px;}"
                                   "QCheckBox::indicator{width:14px;height:14px;border-radius:3px;border:1px solid %4;background:%3;}"
                                   "QLabel#followSystemLabel{font-size:12px;color:%12;}"
                                   "QCheckBox::indicator:checked{background:%7;border:1px solid %7;}")
                                   .arg(dark ? QStringLiteral("#222831") : QStringLiteral("#f4f6fb"))
                                   .arg(dark ? QStringLiteral("#e9eff7") : QStringLiteral("#1f2a35"))
                                   .arg(dark ? QStringLiteral("#2f3743") : QStringLiteral("#ffffff"))
                                   .arg(dark ? QStringLiteral("#4f5a6a") : QStringLiteral("#cfd8e6"))
                                   .arg(dark ? QStringLiteral("#2a313d") : QStringLiteral("#ebeff6"))
                                   .arg(dark ? QStringLiteral("#9aa8bb") : QStringLiteral("#9ba7b7"))
                                   .arg(dark ? QStringLiteral("#4a8cff") : QStringLiteral("#2f80ed"))
                                   .arg(dark ? QStringLiteral("#374352") : QStringLiteral("#f2f5fa"))
                                   .arg(dark ? QStringLiteral("#425063") : QStringLiteral("#e8edf5"))
                                   .arg(spinUpArrow)
                                   .arg(spinDownArrow)
                                   .arg(dark ? QStringLiteral("#9fb0c4") : QStringLiteral("#66758a")));
    }

    if (m_timeLabel) {
        m_timeLabel->setStyleSheet(QStringLiteral("QLabel{color:%1;padding-top:2px;padding-bottom:0px;letter-spacing:1px;}")
                                       .arg(dark ? QStringLiteral("#f5f8fe") : QStringLiteral("#0f1722")));
    }

    m_statusBaseColorHex = dark ? QStringLiteral("#b2c1d3") : QStringLiteral("#4f5c6d");
    if (m_statusLabel) {
        applyStatusLabelStyle(m_statusBaseColorHex);
    }

    if (m_presetHintLabel) {
        m_presetHintLabel->setStyleSheet(QStringLiteral("QLabel{color:%1;font-size:12px;}")
                                             .arg(dark ? QStringLiteral("#95a8bb") : QStringLiteral("#6f7d8c")));
    }

    if (m_historyTimerButton) {
        m_historyTimerButton->setStyleSheet(historyButtonStyle(dark));
    }

    for (int i = 0; i < m_customTimerButtons.size(); ++i) {
        if (m_customTimerButtons.at(i)) {
            m_customTimerButtons.at(i)->setStyleSheet(customButtonStyle(dark, i));
        }
    }

    if (m_startButton) {
        m_startButton->setStyleSheet(QStringLiteral(
                                         "QPushButton#primaryActionButton{background:%1;border:1px solid %2;border-radius:8px;padding:6px 12px;color:%3;font-weight:600;}"
                                         "QPushButton#primaryActionButton:hover{background:%4;}"
                                         "QPushButton#primaryActionButton:disabled{background:%5;color:%6;border:1px solid %7;}")
                                         .arg(dark ? QStringLiteral("#2d8a63") : QStringLiteral("#31a874"))
                                         .arg(dark ? QStringLiteral("#37a173") : QStringLiteral("#289864"))
                                         .arg(QStringLiteral("#ffffff"))
                                         .arg(dark ? QStringLiteral("#369f72") : QStringLiteral("#38b77f"))
                                         .arg(dark ? QStringLiteral("#3a4452") : QStringLiteral("#dce4ef"))
                                         .arg(dark ? QStringLiteral("#8ea0b6") : QStringLiteral("#98a7b9"))
                                         .arg(dark ? QStringLiteral("#4d5869") : QStringLiteral("#c6d0de")));
    }

    const QString secondaryStyle = QStringLiteral(
                                     "QPushButton#secondaryActionButton{background:%1;border:1px solid %2;border-radius:8px;padding:6px 12px;color:%3;font-weight:600;}"
                                     "QPushButton#secondaryActionButton:hover{background:%4;}"
                                     "QPushButton#secondaryActionButton:disabled{background:%5;color:%6;border:1px solid %7;}")
                                     .arg(dark ? QStringLiteral("#394452") : QStringLiteral("#eef3f9"))
                                     .arg(dark ? QStringLiteral("#556173") : QStringLiteral("#cdd8e7"))
                                     .arg(dark ? QStringLiteral("#e3eaf5") : QStringLiteral("#2a3543"))
                                     .arg(dark ? QStringLiteral("#445061") : QStringLiteral("#e6edf6"))
                                     .arg(dark ? QStringLiteral("#34404f") : QStringLiteral("#e7edf6"))
                                     .arg(dark ? QStringLiteral("#9aa9bf") : QStringLiteral("#7f8fa4"))
                                     .arg(dark ? QStringLiteral("#526074") : QStringLiteral("#becbdd"));

    if (m_pauseButton) {
        m_pauseButton->setStyleSheet(secondaryStyle);
    }
    if (m_resetButton) {
        m_resetButton->setStyleSheet(secondaryStyle);
    }

    if (m_exitButton) {
        m_exitButton->setStyleSheet(QStringLiteral(
                                        "QPushButton#dangerActionButton{background:%1;border:1px solid %2;border-radius:8px;padding:6px 12px;color:%3;font-weight:600;}"
                                        "QPushButton#dangerActionButton:hover{background:%4;}")
                                        .arg(dark ? QStringLiteral("#6d3a45") : QStringLiteral("#f8e9ec"))
                                        .arg(dark ? QStringLiteral("#8a4b59") : QStringLiteral("#e7bcc5"))
                                        .arg(dark ? QStringLiteral("#ffe9ee") : QStringLiteral("#8c2e44"))
                                        .arg(dark ? QStringLiteral("#7f4352") : QStringLiteral("#f5dde3")));
    }

    if (m_volumeButton) {
        m_volumeButton->setStyleSheet(QStringLiteral(
                                         "QPushButton#volumeValueButton{background:%1;border:1px solid %2;border-radius:8px;padding:4px 8px;color:%3;font-weight:600;}"
                                         "QPushButton#volumeValueButton:hover{background:%4;}"
                                         "QPushButton#volumeValueButton:pressed{background:%5;}")
                                         .arg(dark ? QStringLiteral("#384453") : QStringLiteral("#edf3fa"))
                                         .arg(dark ? QStringLiteral("#546274") : QStringLiteral("#cdd8e7"))
                                         .arg(dark ? QStringLiteral("#e4ecf7") : QStringLiteral("#2a3543"))
                                         .arg(dark ? QStringLiteral("#445364") : QStringLiteral("#e4edf7"))
                                         .arg(dark ? QStringLiteral("#304052") : QStringLiteral("#d9e6f3")));
    }

    if (m_audioSectionToggleButton) {
        m_audioSectionToggleButton->setStyleSheet(QStringLiteral(
                                                     "QPushButton#audioToggleButton{background:transparent;border:none;padding:2px 2px;color:%1;font-size:12px;font-weight:600;}"
                                                     "QPushButton#audioToggleButton:hover{color:%2;}"
                                                     "QPushButton#audioToggleButton:pressed{color:%3;}")
                                                     .arg(dark ? QStringLiteral("#9fb0c4") : QStringLiteral("#5f7087"))
                                                     .arg(dark ? QStringLiteral("#c3d0e2") : QStringLiteral("#3d516b"))
                                                     .arg(dark ? QStringLiteral("#89a0be") : QStringLiteral("#33455d")));
    }

    if (auto *audioSection = findChild<QWidget *>(QStringLiteral("audioSection"))) {
        audioSection->setStyleSheet(QStringLiteral(
                                        "QWidget#audioSection{background:transparent;border:none;border-top:1px solid %1;}"
                                        "QLabel#audioFileNameLabel{background:transparent;border:none;padding:3px 4px;color:%2;}"
                                        "QPushButton#audioActionButton{background:transparent;border:1px solid %3;border-radius:8px;padding:4px 8px;color:%2;font-weight:600;}"
                                        "QPushButton#audioActionButton:hover{background:%4;}"
                                        "QPushButton#audioActionButton:pressed{background:%5;}"
                                        "QPushButton#audioActionButton:disabled{color:%6;border-color:%6;}")
                                        .arg(dark ? QStringLiteral("#465466") : QStringLiteral("#d3ddea"))
                                        .arg(dark ? QStringLiteral("#d7e1ee") : QStringLiteral("#2a3543"))
                                        .arg(dark ? QStringLiteral("rgba(73,91,112,0.45)") : QStringLiteral("#cdd8e7"))
                                        .arg(dark ? QStringLiteral("rgba(73,91,112,0.35)") : QStringLiteral("rgba(75,110,150,0.10)"))
                                        .arg(dark ? QStringLiteral("rgba(73,91,112,0.55)") : QStringLiteral("rgba(75,110,150,0.18)"))
                                        .arg(dark ? QStringLiteral("#7f90a6") : QStringLiteral("#95a2b3")));
    }

    if (m_volumePopup) {
        m_volumePopup->setStyleSheet(QStringLiteral(
                                        "QWidget#volumePopup{background:transparent;border:none;}"
                                        "QSlider#volumeSlider{background:transparent;}"
                                        "QSlider#volumeSlider::groove:vertical{border-image:url(%1) 0 0 0 0 stretch stretch;width:10px;margin:0 7px;}"
                                        "QSlider#volumeSlider::sub-page:vertical{background:%2;border-radius:5px;margin:0 7px;}"
                                        "QSlider#volumeSlider::add-page:vertical{background:transparent;margin:0 7px;}"
                                        "QSlider#volumeSlider::handle:vertical{image:url(%3);width:18px;height:18px;margin:0 -4px;}")
                                        .arg(volumeTrackSvg)
                                        .arg(dark ? QStringLiteral("rgba(80,148,255,235)") : QStringLiteral("rgba(47,128,237,230)"))
                                        .arg(volumeHandleSvg));
    }

    updateTestSoundButtonState();

    updateThemeButtons();
    update();
}

void MainWindow::applyStatusLabelStyle(const QString &colorHex)
{
    if (!m_statusLabel) {
        return;
    }

    m_statusLabel->setStyleSheet(QStringLiteral("QLabel{color:%1;font-size:13px;padding-top:0px;padding-bottom:2px;}")
                                     .arg(colorHex));
}

void MainWindow::setStatusText(const QString &text, bool highlight)
{
    if (!m_statusLabel) {
        return;
    }

    m_statusLabel->setText(text);
    if (!highlight || m_statusBaseColorHex.isEmpty()) {
        applyStatusLabelStyle(m_statusBaseColorHex.isEmpty() ? QStringLiteral("#4f5c6d") : m_statusBaseColorHex);
        return;
    }

    const QString highlightColorHex = isDarkThemeActive() ? QStringLiteral("#8fc0ff") : QStringLiteral("#2f80ed");
    auto *anim = new QVariantAnimation(this);
    anim->setDuration(220);
    anim->setStartValue(QColor(highlightColorHex));
    anim->setEndValue(QColor(m_statusBaseColorHex));
    connect(anim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        applyStatusLabelStyle(value.value<QColor>().name());
    });
    connect(anim, &QVariantAnimation::finished, anim, &QObject::deleteLater);
    anim->start();
}

QString MainWindow::themeModeToConfigValue(ThemeMode mode) const
{
    switch (mode) {
    case ThemeMode::Light:
        return QStringLiteral("light");
    case ThemeMode::Dark:
        return QStringLiteral("dark");
    case ThemeMode::Auto:
    default:
        return QStringLiteral("auto");
    }
}

MainWindow::ThemeMode MainWindow::themeModeFromConfigValue(const QString &value) const
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("light")) {
        return ThemeMode::Light;
    }
    if (normalized == QStringLiteral("dark")) {
        return ThemeMode::Dark;
    }
    return ThemeMode::Auto;
}

void MainWindow::updateThemeButtons()
{
    if (!m_followSystemButton || !m_themeIconButton) {
        return;
    }

    const bool autoMode = (m_themeMode == ThemeMode::Auto);
    const bool dark = isDarkThemeActive();

    if (auto *switchButton = dynamic_cast<IOSSwitchButton *>(m_followSystemButton)) {
        switchButton->setDarkMode(dark);
        switchButton->syncStateNoAnimation(autoMode);
    } else {
        m_followSystemButton->setChecked(autoMode);
    }

    m_themeIconButton->setText(dark ? QStringLiteral("🌙") : QStringLiteral("☀️"));
    m_themeIconButton->setEnabled(!autoMode);
    m_themeIconButton->setText(QString());
    const QString iconPath = resolveRuntimeImagePath(dark ? QStringLiteral("theme_dark.svg")
                                                          : QStringLiteral("theme_light.svg"));
    if (!iconPath.isEmpty()) {
        m_themeIconButton->setIcon(QIcon(iconPath));
    }
    m_themeIconButton->setStyleSheet(QStringLiteral(
                                        "QPushButton{border:none;background:transparent;padding:2px;}"
                                        "QPushButton:hover{background:%1;border-radius:6px;}"
                                        "QPushButton:pressed{background:%2;border-radius:6px;}"
                                        "QPushButton:disabled{background:transparent;}")
                                        .arg(dark ? QStringLiteral("#3f4b5c") : QStringLiteral("#eaf0f8"))
                                        .arg(dark ? QStringLiteral("#334052") : QStringLiteral("#dce6f2")));

    if (autoMode) {
        m_themeIconButton->setToolTip(QStringLiteral("跟随系统已开启，手动切换已禁用"));
    } else {
        m_themeIconButton->setToolTip(QStringLiteral("切换浅色/深色"));
    }

    const QString followTip = autoMode
                                  ? QStringLiteral("跟随系统：已开启（当前%1）\n关闭后可手动切换主题")
                                        .arg(dark ? QStringLiteral("深色") : QStringLiteral("浅色"))
                                  : QStringLiteral("跟随系统：已关闭\n当前为手动%1主题")
                                        .arg(dark ? QStringLiteral("深色") : QStringLiteral("浅色"));
    m_followSystemButton->setToolTip(followTip);
}

void MainWindow::playReminderSound()
{
    stopReminderSound();

    if (shouldUseCustomSound() && playCustomSound(currentSoundFilePath(), isLoopReminderEnabled())) {
        return;
    }

    QApplication::beep();
    if (isLoopReminderEnabled()) {
        startFallbackBeepLoop();
    }
}

bool MainWindow::playCustomSound(const QString &filePath, bool loop)
{
    m_lastAudioError.clear();
    const QString trimmedPath = filePath.trimmed();
    if (trimmedPath.isEmpty() || !QFileInfo::exists(trimmedPath)) {
        m_lastAudioError = QStringLiteral("音频文件不存在或路径为空");
        return false;
    }

#ifdef _WIN32
    QString miniError;
    if (AudioMini::playFile(trimmedPath, loop, reminderVolumePercent(), &miniError)) {
        return true;
    }

    const QString normalizedPath = QDir::toNativeSeparators(trimmedPath);
    const QString alias = QString::fromWCharArray(kAlarmAlias);
    const QString extension = QFileInfo(trimmedPath).suffix().trimmed().toLower();
    const int volumeValue = qBound(0, reminderVolumePercent(), 100) * 10;

    sendMciCommand(QStringLiteral("stop %1").arg(alias), nullptr);
    sendMciCommand(QStringLiteral("close %1").arg(alias), nullptr);

    QString openError;
    if (!tryOpenMediaByMci(normalizedPath, &openError)) {
        if (extension == QStringLiteral("mp3")) {
            QString directShowError;
            if (startDirectShowPlayback(trimmedPath, loop, &directShowError)) {
                return true;
            }

            const QString miniText = miniError.isEmpty() ? QStringLiteral("miniaudio 播放失败") : miniError;
            const QString mciText = openError.isEmpty() ? QStringLiteral("MCI 打开失败") : openError;
            const QString dsText = directShowError.isEmpty() ? QStringLiteral("DirectShow 回退失败") : directShowError;
            m_lastAudioError = QStringLiteral("%1；%2；%3").arg(miniText, mciText, dsText);
            return false;
        }

        const QString miniText = miniError.isEmpty() ? QStringLiteral("miniaudio 播放失败") : miniError;
        const QString mciText = openError.isEmpty() ? QStringLiteral("无法打开音频文件") : openError;
        m_lastAudioError = QStringLiteral("%1；%2").arg(miniText, mciText);
        return false;
    }

    QString volumeError;
    sendMciCommand(QStringLiteral("setaudio %1 volume to %2").arg(alias).arg(volumeValue), &volumeError);

    QString playError;
    const QString playCmd = loop
                                ? QStringLiteral("play %1 repeat").arg(alias)
                                : QStringLiteral("play %1").arg(alias);
    if (!sendMciCommand(playCmd, &playError)) {
        sendMciCommand(QStringLiteral("close %1").arg(alias), nullptr);
        m_lastAudioError = playError.isEmpty() ? QStringLiteral("无法播放音频") : playError;
        return false;
    }

    if (!volumeError.isEmpty()) {
        m_lastAudioError = volumeError;
    }
    return true;
#else
    Q_UNUSED(loop);
    Q_UNUSED(trimmedPath);
    m_lastAudioError = QStringLiteral("当前平台暂不支持自定义音频播放");
    return false;
#endif
}

void MainWindow::stopReminderSound()
{
#ifdef _WIN32
    AudioMini::stop();
    PlaySoundW(nullptr, nullptr, 0);
    mciSendStringW(L"stop TomatoClockAlarm", nullptr, 0, nullptr);
    mciSendStringW(L"close TomatoClockAlarm", nullptr, 0, nullptr);
    releaseDirectShowPlayer();
#endif
    stopFallbackBeepLoop();
    m_isPreviewPlaying = false;
    if (m_previewStateTimer) {
        m_previewStateTimer->stop();
    }
    updateTestSoundButtonState();
}

void MainWindow::startFallbackBeepLoop()
{
    if (!m_beepLoopTimer || m_beepLoopTimer->isActive()) {
        return;
    }
    m_beepLoopTimer->start();
}

void MainWindow::stopFallbackBeepLoop()
{
    if (m_beepLoopTimer) {
        m_beepLoopTimer->stop();
    }
}

bool MainWindow::isLoopReminderEnabled() const
{
    return m_loopAlarmCheckBox && m_loopAlarmCheckBox->isChecked();
}

int MainWindow::reminderVolumePercent() const
{
    return qBound(0, m_volumePercent, 100);
}

void MainWindow::updateVolumeDisplay()
{
    if (!m_volumeButton) {
        return;
    }

    m_volumeButton->setText(QStringLiteral("%1%").arg(reminderVolumePercent()));
}

void MainWindow::showVolumePopup()
{
    if (!m_volumePopup || !m_volumeSlider || !m_volumeButton) {
        return;
    }

    m_volumeSlider->setValue(reminderVolumePercent());
    m_volumePopup->adjustSize();
    const QPoint anchor = m_volumeButton->mapToGlobal(QPoint(m_volumeButton->width() / 2, 0));
    const int popupX = anchor.x() - m_volumePopup->width() / 2;
    const int popupY = anchor.y() - m_volumePopup->height() - 6;
    m_volumePopup->move(popupX, popupY);
    m_volumePopup->show();
    m_volumePopup->raise();
    m_volumeSlider->setFocus();
}

void MainWindow::updateTestSoundButtonState()
{
    if (!m_testSoundButton) {
        return;
    }

    m_testSoundButton->setIcon(playStateIcon(m_isPreviewPlaying, isDarkThemeActive()));
    m_testSoundButton->setText(m_isPreviewPlaying ? QStringLiteral("暂停") : QStringLiteral("播放"));
}

void MainWindow::refreshPreviewPlaybackState()
{
    if (!m_isPreviewPlaying) {
        if (m_previewStateTimer) {
            m_previewStateTimer->stop();
        }
        return;
    }

#ifdef _WIN32
    if (AudioMini::isPlaying()) {
        return;
    }

    if (isDirectShowPlaying()) {
        return;
    }

    wchar_t buffer[64] = {0};
    const std::wstring statusCmd = QStringLiteral("status %1 mode")
                                       .arg(QString::fromWCharArray(kAlarmAlias))
                                       .toStdWString();
    if (mciSendStringW(statusCmd.c_str(), buffer, 64, nullptr) != 0) {
        m_isPreviewPlaying = false;
    } else {
        const QString mode = QString::fromWCharArray(buffer).trimmed().toLower();
        if (mode != QStringLiteral("playing")) {
            m_isPreviewPlaying = false;
        }
    }
#else
    m_isPreviewPlaying = false;
#endif

    if (!m_isPreviewPlaying && m_previewStateTimer) {
        m_previewStateTimer->stop();
    }
    updateTestSoundButtonState();
}

bool MainWindow::shouldUseCustomSound() const
{
    return m_soundCheckBox
           && m_soundCheckBox->isChecked()
           && !currentSoundFilePath().trimmed().isEmpty();
}

QString MainWindow::currentSoundFilePath() const
{
    return m_soundFilePath.trimmed();
}

void MainWindow::setSoundFilePath(const QString &filePath)
{
    if (!m_soundFileNameLabel) {
        return;
    }

    const QString trimmedPath = filePath.trimmed();
    m_soundFilePath = trimmedPath;

    if (trimmedPath.isEmpty()) {
        m_soundFileNameLabel->setText(QStringLiteral("未选择音乐"));
        m_soundFileNameLabel->setToolTip(QString());
        return;
    }

    const QString fileName = QFileInfo(trimmedPath).fileName();
    const QFontMetrics metrics(m_soundFileNameLabel->font());
    const QString elided = metrics.elidedText(fileName, Qt::ElideMiddle, 150);
    m_soundFileNameLabel->setText(elided);
    m_soundFileNameLabel->setToolTip(trimmedPath);
}

void MainWindow::ensureTrayReady()
{
    QIcon preferredIcon = windowIcon();
    if (preferredIcon.isNull()) {
        preferredIcon = loadRuntimeAppIconFromDisk(QCoreApplication::applicationDirPath(), style());
    }

    if (m_trayIcon) {
        if (QSystemTrayIcon::isSystemTrayAvailable() && !m_trayIcon->isVisible()) {
            m_trayIcon->setIcon(preferredIcon);
            m_trayIcon->show();
        }
        return;
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }

    m_trayIcon = new QSystemTrayIcon(preferredIcon, this);

    auto *trayMenu = new QMenu(this);
    m_showAction = trayMenu->addAction(QStringLiteral("打开"));
    m_exitAction = trayMenu->addAction(QStringLiteral("退出"));
    m_trayIcon->setContextMenu(trayMenu);

    connect(m_showAction, &QAction::triggered, this, &MainWindow::restoreFromTray);
    connect(m_exitAction, &QAction::triggered, this, &MainWindow::exitFromTray);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick || reason == QSystemTrayIcon::Trigger) {
            restoreFromTray();
        }
    });

    m_trayIcon->show();
}

bool MainWindow::isTrayActuallyReady() const
{
    if (!m_trayIcon) {
        return false;
    }

    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return false;
    }

    if (!m_trayIcon->isVisible()) {
        return false;
    }

    return true;
}

void MainWindow::startTrayHealthCheck()
{
    auto *trayHealthTimer = new QTimer(this);
    trayHealthTimer->setInterval(3000);

    connect(trayHealthTimer, &QTimer::timeout, this, [this]() {
        if (m_isExiting) {
            return;
        }

        ensureTrayReady();

        const bool trayReady = isTrayActuallyReady();

        if (trayReady) {
            m_trayHealthMissCount = 0;
            return;
        }

        if (!isHidden()) {
            return;
        }

        ++m_trayHealthMissCount;
        if (m_trayHealthMissCount < 2) {
            return;
        }

        showNormal();
        activateWindow();
        raise();
        m_trayHealthMissCount = 0;

        if (!m_trayRecoverHintShown) {
            QMessageBox::warning(this,
                                 QStringLiteral("番茄钟"),
                                 QStringLiteral("检测到托盘图标不可见，已自动恢复窗口，避免进程隐藏在后台。"));
            m_trayRecoverHintShown = true;
        }
    });

    trayHealthTimer->start();
}

void MainWindow::loadConfig()
{
    const QFileInfo configInfo(m_configFilePath);
    if (!configInfo.dir().exists()) {
        QDir().mkpath(configInfo.dir().absolutePath());
    }

    QSettings settings(m_configFilePath, QSettings::IniFormat);
    const QString lastAudioPath = settings.value(QStringLiteral("audio/lastFile"), QString()).toString();
    const bool enabled = settings.value(QStringLiteral("audio/enabled"), false).toBool();
    const bool loopEnabled = settings.value(QStringLiteral("audio/loopEnabled"), false).toBool();
    const int volumePercent = settings.value(QStringLiteral("audio/volumePercent"), 80).toInt();
    m_themeMode = themeModeFromConfigValue(settings.value(QStringLiteral("ui/themeMode"), QStringLiteral("auto")).toString());

    if (!lastAudioPath.trimmed().isEmpty()) {
        setSoundFilePath(lastAudioPath);
    }
    m_soundCheckBox->setChecked(enabled);
    m_loopAlarmCheckBox->setChecked(loopEnabled);
    m_volumePercent = qBound(0, volumePercent, 100);
    if (m_volumeSlider) {
        m_volumeSlider->setValue(m_volumePercent);
    }
    updateVolumeDisplay();

    const QVector<int> defaultCustomSeconds {
        25 * 60,
        5 * 60,
        10 * 60,
        15 * 60,
        45 * 60
    };
    m_customTimerSeconds.clear();
    m_customTimerNames.clear();
    for (int i = 0; i < 5; ++i) {
        const int seconds = settings.value(QStringLiteral("timer/custom%1").arg(i + 1), defaultCustomSeconds.at(i)).toInt();
        m_customTimerSeconds.append(qMax(0, seconds));
        const QString name = settings.value(QStringLiteral("timer/customName%1").arg(i + 1), QString()).toString().trimmed();
        m_customTimerNames.append(name);
    }

    m_timerHistorySeconds.clear();
    const QVariantList historyValues = settings.value(QStringLiteral("timer/history"), QVariantList()).toList();
    for (const QVariant &value : historyValues) {
        const int seconds = value.toInt();
        if (seconds <= 0 || m_timerHistorySeconds.contains(seconds)) {
            continue;
        }
        m_timerHistorySeconds.append(seconds);
        if (m_timerHistorySeconds.size() >= 20) {
            break;
        }
    }

    refreshTimerPresetButtons();
    applyTheme();
}

void MainWindow::saveConfig() const
{
    const QFileInfo configInfo(m_configFilePath);
    if (!configInfo.dir().exists()) {
        QDir().mkpath(configInfo.dir().absolutePath());
    }

    QSettings settings(m_configFilePath, QSettings::IniFormat);
    settings.setValue(QStringLiteral("audio/lastFile"), currentSoundFilePath());
    settings.setValue(QStringLiteral("audio/enabled"), m_soundCheckBox ? m_soundCheckBox->isChecked() : false);
    settings.setValue(QStringLiteral("audio/loopEnabled"), m_loopAlarmCheckBox ? m_loopAlarmCheckBox->isChecked() : false);
    settings.setValue(QStringLiteral("audio/volumePercent"), reminderVolumePercent());
    settings.setValue(QStringLiteral("ui/themeMode"), themeModeToConfigValue(m_themeMode));

    for (int i = 0; i < m_customTimerSeconds.size(); ++i) {
        settings.setValue(QStringLiteral("timer/custom%1").arg(i + 1), m_customTimerSeconds.at(i));
        const QString name = (i < m_customTimerNames.size()) ? m_customTimerNames.at(i).trimmed() : QString();
        settings.setValue(QStringLiteral("timer/customName%1").arg(i + 1), name);
    }

    QVariantList historyValues;
    for (int seconds : m_timerHistorySeconds) {
        historyValues.append(seconds);
    }
    settings.setValue(QStringLiteral("timer/history"), historyValues);

    settings.sync();
}

