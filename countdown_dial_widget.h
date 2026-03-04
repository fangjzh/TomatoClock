#ifndef COUNTDOWN_DIAL_WIDGET_H
#define COUNTDOWN_DIAL_WIDGET_H

#include <QWidget>

class CountdownDialWidget : public QWidget
{
    Q_OBJECT

public:
    enum class VisualState {
        Ready,
        Running,
        Paused,
        Finished
    };

    explicit CountdownDialWidget(QWidget *parent = nullptr);
    void setCountdownState(int totalSeconds, int remainingSeconds);
    void setVisualState(VisualState state);

protected:
    void paintEvent(QPaintEvent *event) override;
    QSize sizeHint() const override;

private:
    int m_totalSeconds;
    int m_remainingSeconds;
    VisualState m_visualState;
};

#endif // COUNTDOWN_DIAL_WIDGET_H
