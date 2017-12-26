/************************************************************************

    mainwindow.h

    QT GUI Capture application for Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2017 Simon Inns

    This file is part of Domesday Duplicator.

    Domesday Duplicator is free software: you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

    Email: simon.inns@gmail.com

************************************************************************/


#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QFileDialog>
#include <QtSerialPort/QSerialPort>

#include "usbdevice.h"
#include "serialportselectdialog.h"
#include "playercontroldialog.h"
#include "aboutdialog.h"
#include "lvdpcontrol.h"

namespace Ui {
class MainWindow;
}

class playerControlDialog;
class serialPortSelectDialog;

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

    void startTransfer(void);
    void stopTransfer(void);
    void updateCaptureInfo(void);

    void on_testModeCheckBox_toggled(bool checked);
    void on_actionSelect_player_COM_port_triggered();
    void on_actionShow_player_control_triggered();

    void serialPortStatusChange(void);
    void updatePlayerControlInfo(void);

    void handlePlayerControlEvent(playerControlDialog::PlayerControlEvents, quint32);

    void on_cavLeadInCheckBox_toggled(bool checked);
    void on_clvLeadInCheckBox_toggled(bool checked);

    void cavPicPoll(void);
    void clvPicPoll(void);

    void on_cavCapturePushButton_clicked();
    void on_clvCapturePushButton_clicked();

private:
    Ui::MainWindow *ui;

    usbDevice *domDupUsbDevice;
    QLabel *status;
    bool captureFlag;

    QTimer* captureTimer;
    QString fileName;

    aboutDialog *aboutDomDup;
    serialPortSelectDialog *lvdpSerialPortSelect;
    playerControlDialog *lvdpPlayerControl;

    QTimer* updateTimer;
    lvdpControl *playerControl;

    // CAV PIC capture state-machine
    // Define the possible state-machine states
    enum CavPicStates {
        cavState_idle,
        cavState_startPlayer,
        cavState_waitForPlay,
        cavState_determineDiscLength,
        cavState_waitForDetermineDiscLength,
        cavState_stopPlayer,
        cavState_seekToFrame,
        cavState_waitForSeek,
        cavState_startCapture,
        cavState_waitForStartCapture,
        cavState_waitForEndFrame,
        cavState_stopCapture,
        cavState_error
    };

    CavPicStates cavPicCurrentState;
    CavPicStates cavPicNextState;
    bool cavPicCaptureActive;
    bool cavPicCaptureAbort;
    QTimer* cavPicPollTimer;

    // CLV PIC capture state-machine
    // Define the possible state-machine states
    enum ClvPicStates {
        clvState_idle,
        clvState_startPlayer,
        clvState_waitForPlay,
        clvState_determineDiscLength,
        clvState_waitForDetermineDiscLength,
        clvState_stopPlayer,
        clvState_seekToFrame,
        clvState_waitForSeek,
        clvState_startCapture,
        clvState_waitForStartCapture,
        clvState_waitForEndFrame,
        clvState_stopCapture,
        clvState_error
    };

    ClvPicStates clvPicCurrentState;
    ClvPicStates clvPicNextState;
    bool clvPicCaptureActive;
    bool clvPicCaptureAbort;
    QTimer* clvPicPollTimer;
};

#endif // MAINWINDOW_H
