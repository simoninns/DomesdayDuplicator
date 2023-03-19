/************************************************************************

    playerremotedialog.h

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2018-2019 Simon Inns

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

#ifndef PLAYERREMOTEDIALOG_H
#define PLAYERREMOTEDIALOG_H

#include <QDialog>
#include <QDebug>

namespace Ui {
class PlayerRemoteDialog;
}

class PlayerRemoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlayerRemoteDialog(QWidget *parent = nullptr);
    ~PlayerRemoteDialog();

    // Enumeration of available remote buttons
    enum RemoteButtons {
        rbReject,
        rbPause,
        rbPlay,
        rbRepeat,
        rbStepRev,
        rbStepFwd,
        rbDisplay,
        rbScanRev,
        rbScanFwd,
        rbAudio,
        rbSpeedDown,
        rbSpeedUp,
        rbClear,
        rbMultiRev,
        rbMultiFwd,
        rbSearch,
        rbChapFrame,
        rbZero,
        rbOne,
        rbTwo,
        rbThree,
        rbFour,
        rbFive,
        rbSix,
        rbSeven,
        rbEight,
        rbNine
    };

    enum PositionMode {
        pmChapter,
        pmFrame,
        pmTime
    };

    enum MultiSpeed {
        multiSm16,   // Slow-mo 1/6
        multiSm14,   // Slow-mo 1/4
        multiSm13,   // Slow-mo 1/3
        multiSm12,   // Slow-mo 1/2
        multiX1,     // x1
        multiX2,     // x2
        multiX3,     // x3
        multiX4      // x4
    };

    enum DisplayMode {
        displayOff,
        displayOn
    };

    void setEnabled(bool flag);

    void setMultiSpeed(MultiSpeed multiSpeedParam);
    void setDisplayMode(DisplayMode displayModeParam);

signals:
    void remoteControlCommand(RemoteButtons button);
    void remoteControlSearch(qint32 position, PlayerRemoteDialog::PositionMode positionMode);

private slots:
    void on_rejectPushButton_clicked();
    void on_pausePushButton_clicked();
    void on_playPushButton_clicked();
    void on_repeatPushButton_clicked();
    void on_stepRevPushButton_clicked();
    void on_stepFwdPushButton_clicked();
    void on_displayPushButton_clicked();
    void on_scanRevPushButton_clicked();
    void on_scanFwdPushButton_clicked();
    void on_audioPushButton_clicked();
    void on_speedDownPushButton_clicked();
    void on_speedUpPushButton_clicked();
    void on_clearPushButton_clicked();
    void on_multiRevPushButton_clicked();
    void on_multiFwdPushButton_clicked();
    void on_sevenPushButton_clicked();
    void on_eightPushButton_clicked();
    void on_ninePushButton_clicked();
    void on_fourPushButton_clicked();
    void on_fivePushButton_clicked();
    void on_sixPushButton_clicked();
    void on_onePushButton_clicked();
    void on_twoPushButton_clicked();
    void on_threePushButton_clicked();
    void on_zeroPushButton_clicked();
    void on_searchPushButton_clicked();
    void on_chapFramePushButton_clicked();

private:
    Ui::PlayerRemoteDialog *ui;

    PositionMode positionMode;
    MultiSpeed multiSpeed;
    DisplayMode displayMode;

    QString position;
    QString display;

    void updateGui();
    void positionAddValue(qint32 value);
};

#endif // PLAYERREMOTEDIALOG_H
