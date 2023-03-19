/************************************************************************

    automaticcapturedialog.h

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

#ifndef AUTOMATICCAPTUREDIALOG_H
#define AUTOMATICCAPTUREDIALOG_H

#include <QDialog>
#include <QMessageBox>
#include <QDebug>

namespace Ui {
class AutomaticCaptureDialog;
}

class AutomaticCaptureDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AutomaticCaptureDialog(QWidget *parent = nullptr);
    ~AutomaticCaptureDialog();

    enum DiscType {
        unknownDiscType,
        CAV,
        CLV
    };

    enum CaptureType {
        wholeDisc,
        partialDisc,
        leadInCapture
    };

    void setEnabled(bool flag);
    void updateStatus(QString statusString);
    void captureComplete();

signals:
    void startAutomaticCapture(CaptureType captureType, qint32 startAddress, qint32 endAddress, DiscType discType);
    void stopAutomaticCapture();

private slots:
    void on_wholeDiscRadioButton_clicked();
    void on_partialDiscRadioButton_clicked();
    void on_leadInCaptureRadioButton_clicked();
    void on_startCavCapturePushButton_clicked();
    void on_startClvCapturePushButton_clicked();

private:
    Ui::AutomaticCaptureDialog *ui;

    bool captureInProgress;
    CaptureType captureType;
};

#endif // AUTOMATICCAPTUREDIALOG_H
