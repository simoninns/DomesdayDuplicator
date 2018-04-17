/************************************************************************

    serialportselectdialog.h

    QT GUI Capture application for Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2018 Simon Inns

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

#ifndef SERIALPORTSELECTDIALOG_H
#define SERIALPORTSELECTDIALOG_H

#include <QDialog>
#include <QDebug>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>

namespace Ui {
class serialPortSelectDialog;
}

class serialPortSelectDialog : public QDialog
{
    Q_OBJECT

public:
    explicit serialPortSelectDialog(QWidget *parent = 0);
    ~serialPortSelectDialog();
    virtual void showEvent(QShowEvent *);

    qint16 getBaudRate(void);
    QString getPortName(void);
    bool isConfigured(void);

signals:
    void serialPortChanged(void);

private slots:
    void on_buttonBox_accepted();

private:
    Ui::serialPortSelectDialog *ui;

    struct Settings {
        QString name;
        qint16 baudRate;
        bool configured;
    };

    Settings currentSettings;

    void fillPortsInfo();
    void updateSettings();

};

#endif // SERIALPORTSELECTDIALOG_H
