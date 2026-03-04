#include "alarm_timeout_dialog.h"

#include <QCoreApplication>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <QSvgRenderer>
#include <QtMath>

namespace {
QString resolveAlarmSvgPath()
{
    const QString appDirPath = QCoreApplication::applicationDirPath();
    const QStringList iconCandidates {
        QDir(appDirPath).filePath(QStringLiteral("resource/images/alarm.svg")),
        QDir(appDirPath).filePath(QStringLiteral("../resource/images/alarm.svg"))
    };

    for (const QString &path : iconCandidates) {
        if (QFileInfo::exists(path)) {
            return path;
        }
    }

    return QString();
}

class VibratingAlarmIconWidget : public QWidget
{
public:
    explicit VibratingAlarmIconWidget(QWidget *parent = nullptr)
        : QWidget(parent)
        , m_renderer(new QSvgRenderer(this))
        , m_phase(0)
    {
        setFixedSize(128, 128);

        const QString svgPath = resolveAlarmSvgPath();
        if (!svgPath.isEmpty()) {
            m_renderer->load(svgPath);
        }

        auto *timer = new QTimer(this);
        timer->setInterval(55);
        connect(timer, &QTimer::timeout, this, [this]() {
            m_phase = (m_phase + 1) % 120;
            update();
        });
        timer->start();
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const qreal wave = qSin(static_cast<qreal>(m_phase) * 0.42);
        const qreal offsetX = wave * 6.0;
        const qreal angle = wave * 10.0;

        painter.translate(width() / 2.0 + offsetX, height() / 2.0);
        painter.rotate(angle);

        const QRectF targetRect(-46.0, -46.0, 92.0, 92.0);
        if (m_renderer->isValid()) {
            m_renderer->render(&painter, targetRect);
            return;
        }

        painter.setPen(QPen(palette().color(QPalette::Highlight), 3));
        painter.setBrush(palette().color(QPalette::Base));
        painter.drawEllipse(targetRect);
        painter.drawLine(QPointF(0, -36), QPointF(0, -8));
        painter.setBrush(palette().color(QPalette::Highlight));
        painter.drawEllipse(QPointF(0, -36), 4, 4);
    }

private:
    QSvgRenderer *m_renderer;
    int m_phase;
};
}

AlarmTimeoutDialog::AlarmTimeoutDialog(QWidget *parent)
    : QDialog(parent)
    , m_iconWidget(new VibratingAlarmIconWidget(this))
    , m_messageLabel(new QLabel(QStringLiteral("时间到！"), this))
    , m_okButton(nullptr)
{
    setWindowTitle(QStringLiteral("番茄钟"));
    setModal(true);
    setMinimumWidth(320);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 16, 18, 14);
    layout->setSpacing(10);

    m_messageLabel->setAlignment(Qt::AlignCenter);
    QFont font = m_messageLabel->font();
    font.setPointSize(font.pointSize() + 1);
    font.setBold(true);
    m_messageLabel->setFont(font);

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    m_okButton = buttonBox->button(QDialogButtonBox::Ok);
    if (m_okButton) {
        m_okButton->setText(QStringLiteral("确定"));
    }

    layout->addWidget(m_iconWidget, 0, Qt::AlignHCenter);
    layout->addWidget(m_messageLabel);
    layout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
}
