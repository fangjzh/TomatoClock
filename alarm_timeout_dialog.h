#ifndef ALARM_TIMEOUT_DIALOG_H
#define ALARM_TIMEOUT_DIALOG_H

#include <QDialog>

class QLabel;
class QPushButton;
class QWidget;

class AlarmTimeoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AlarmTimeoutDialog(QWidget *parent = nullptr);

private:
    QWidget *m_iconWidget;
    QLabel *m_messageLabel;
    QPushButton *m_okButton;
};

#endif // ALARM_TIMEOUT_DIALOG_H
