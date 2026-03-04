#include "countdown_dial_widget.h"

#include <QFont>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QRadialGradient>
#include <QtGlobal>

CountdownDialWidget::CountdownDialWidget(QWidget *parent)
    : QWidget(parent)
    , m_totalSeconds(1500)
    , m_remainingSeconds(1500)
    , m_visualState(VisualState::Ready)
{
    setMinimumSize(180, 180);
}

void CountdownDialWidget::setCountdownState(int totalSeconds, int remainingSeconds)
{
    const int normalizedTotal = qMax(1, totalSeconds);
    const int normalizedRemaining = qBound(0, remainingSeconds, normalizedTotal);

    if (m_totalSeconds == normalizedTotal && m_remainingSeconds == normalizedRemaining) {
        return;
    }

    m_totalSeconds = normalizedTotal;
    m_remainingSeconds = normalizedRemaining;
    update();
}

void CountdownDialWidget::setVisualState(VisualState state)
{
    if (m_visualState == state) {
        return;
    }

    m_visualState = state;
    update();
}

QSize CountdownDialWidget::sizeHint() const
{
    return QSize(240, 240);
}

void CountdownDialWidget::paintEvent(QPaintEvent *event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF contentBounds = rect().adjusted(10, 10, -10, -10);
    const qreal dialSize = qMin(contentBounds.width(), contentBounds.height());
    const QRectF dialBounds(contentBounds.center().x() - dialSize * 0.5,
                            contentBounds.center().y() - dialSize * 0.5,
                            dialSize,
                            dialSize);
    const QPointF center = dialBounds.center();
    const qreal radius = dialBounds.width() * 0.5;

    const QPalette pal = palette();
    const QColor ringColor = pal.color(QPalette::Dark);
    QColor majorTickColor = pal.color(QPalette::Text);
    const QColor minorTickColor = pal.color(QPalette::Midlight);
    QColor progressColor = pal.color(QPalette::Highlight);
    QColor minuteHandColor = pal.color(QPalette::Text);
    QColor secondHandColor = pal.color(QPalette::Highlight);
    const QColor faceOuter = pal.color(QPalette::Base);
    const QColor faceInner = pal.color(QPalette::AlternateBase);

    if (m_visualState == VisualState::Paused) {
        progressColor = pal.color(QPalette::Mid);
        minuteHandColor = pal.color(QPalette::Mid);
        secondHandColor = pal.color(QPalette::Midlight);
        majorTickColor = pal.color(QPalette::Dark);
    } else if (m_visualState == VisualState::Finished) {
        progressColor = pal.color(QPalette::Link);
        secondHandColor = pal.color(QPalette::Link);
    }

    painter.setPen(QPen(ringColor, 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(center, radius, radius);

    QRadialGradient faceGradient(center, radius * 0.95, QPointF(center.x() - radius * 0.25, center.y() - radius * 0.25));
    faceGradient.setColorAt(0.0, faceInner.lighter(108));
    faceGradient.setColorAt(1.0, faceOuter.darker(103));
    painter.setPen(Qt::NoPen);
    painter.setBrush(faceGradient);
    painter.drawEllipse(center, radius - 4, radius - 4);

    const qreal progressRatio = static_cast<qreal>(m_remainingSeconds) / static_cast<qreal>(m_totalSeconds);
    const int spanAngle16 = static_cast<int>(-360.0 * progressRatio * 16.0);
    const QRectF progressBounds = dialBounds.adjusted(7, 7, -7, -7);
    painter.setPen(QPen(pal.color(QPalette::Mid), 7, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(progressBounds, 90 * 16, -360 * 16);
    painter.setPen(QPen(progressColor, 7, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(progressBounds, 90 * 16, spanAngle16);

    painter.save();
    painter.translate(center);
    for (int i = 0; i < 60; ++i) {
        const bool major = (i % 5 == 0);
        painter.setPen(QPen(major ? majorTickColor : minorTickColor, major ? 2.6 : 1.1));
        const qreal tickOuter = radius - 18;
        const qreal tickInner = major ? radius - 34 : radius - 27;
        painter.drawLine(QPointF(0, -tickOuter), QPointF(0, -tickInner));
        painter.rotate(6.0);
    }
    painter.restore();

    painter.save();
    painter.translate(center);
    const QFont originalFont = painter.font();
    QFont markerFont = originalFont;
    markerFont.setBold(true);
    markerFont.setPointSizeF(originalFont.pointSizeF() > 0 ? originalFont.pointSizeF() * 0.95 : 10.0);
    painter.setFont(markerFont);
    painter.setPen(majorTickColor);
    const qreal markerRadius = radius - 48;
    painter.drawText(QRectF(-10, -markerRadius - 9, 20, 18), Qt::AlignCenter, QStringLiteral("12"));
    painter.drawText(QRectF(markerRadius - 10, -9, 20, 18), Qt::AlignCenter, QStringLiteral("3"));
    painter.drawText(QRectF(-10, markerRadius - 9, 20, 18), Qt::AlignCenter, QStringLiteral("6"));
    painter.drawText(QRectF(-markerRadius - 10, -9, 20, 18), Qt::AlignCenter, QStringLiteral("9"));
    painter.restore();

    const int remainingMinutes = m_remainingSeconds / 60;
    const int remainingSeconds = m_remainingSeconds % 60;

    const qreal minuteAngle = (static_cast<qreal>(remainingMinutes % 60) + static_cast<qreal>(remainingSeconds) / 60.0) * 6.0;
    const qreal secondAngle = static_cast<qreal>(remainingSeconds) * 6.0;

    painter.save();
    painter.translate(center);

    painter.save();
    painter.rotate(minuteAngle);
    painter.setPen(QPen(minuteHandColor, 4.5, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(0, 7), QPointF(0, -radius * 0.50));
    painter.restore();

    painter.save();
    painter.rotate(secondAngle);
    painter.setPen(QPen(secondHandColor, 2, Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(QPointF(0, 13), QPointF(0, -radius * 0.68));
    painter.setBrush(secondHandColor);
    painter.setPen(Qt::NoPen);
    painter.drawEllipse(QPointF(0, radius * 0.22), 2.8, 2.8);
    painter.restore();

    painter.setPen(Qt::NoPen);
    painter.setBrush(minuteHandColor);
    painter.drawEllipse(QPointF(0, 0), 5.2, 5.2);
    painter.setBrush(faceOuter);
    painter.drawEllipse(QPointF(0, 0), 2.3, 2.3);

    painter.restore();

    if (m_visualState == VisualState::Paused || m_visualState == VisualState::Finished) {
        QFont stateFont = painter.font();
        stateFont.setBold(true);
        stateFont.setPointSizeF(stateFont.pointSizeF() > 0 ? stateFont.pointSizeF() * 0.9 : 10.0);
        painter.setFont(stateFont);
        painter.setPen(majorTickColor);
        const QString stateText = (m_visualState == VisualState::Paused)
                                      ? QStringLiteral("PAUSE")
                                      : QStringLiteral("DONE");
        painter.drawText(QRectF(center.x() - 36, center.y() + radius * 0.28, 72, 18), Qt::AlignCenter, stateText);
    }
}
