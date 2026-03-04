#include "icon_utils.h"
#include "mainwindow.h"

#include <QApplication>
#include <QCoreApplication>
#include <QIcon>
#include <QMessageBox>
#include <QSharedMemory>
#include <QStyle>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setWindowIcon(loadRuntimeAppIconFromDisk(QCoreApplication::applicationDirPath(), a.style()));

    QSharedMemory singleInstanceGuard(QStringLiteral("TomatoClock.SingleInstance"));
    if (!singleInstanceGuard.create(1)) {
        auto *messageBox = new QMessageBox(QMessageBox::Warning,
                                           QStringLiteral("番茄钟"),
                                           QString(),
                                           QMessageBox::NoButton);
        messageBox->setAttribute(Qt::WA_DeleteOnClose);
        messageBox->setWindowTitle(QStringLiteral("番茄钟"));

        int remainingSeconds = 5;
        messageBox->setText(QStringLiteral("已经有任务在运行，程序将在 %1 秒后退出。")
                                .arg(remainingSeconds));

        auto *countdownTimer = new QTimer(messageBox);
        QObject::connect(countdownTimer, &QTimer::timeout, messageBox, [messageBox, &a, &remainingSeconds]() {
            --remainingSeconds;
            if (remainingSeconds <= 0) {
                messageBox->close();
                a.quit();
                return;
            }

            messageBox->setText(QStringLiteral("已经有任务在运行，程序将在 %1 秒后退出。")
                                    .arg(remainingSeconds));
        });

        countdownTimer->start(1000);
        messageBox->show();
        return a.exec();
    }

    MainWindow w;
    w.show();
    return a.exec();
}
