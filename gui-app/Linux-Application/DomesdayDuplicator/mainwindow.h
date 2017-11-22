#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>
#include "usbdevice.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
    void usbStatusChanged(bool usbStatus);

private slots:
    void on_actionAbout_triggered();
    void on_actionSave_As_triggered();
    void on_actionQuit_triggered();

    void on_transferPushButton_clicked();

private:
    Ui::MainWindow *ui;

    usbDevice *domDupUsbDevice;
    QLabel *status;
    bool captureFlag;
};

#endif // MAINWINDOW_H
