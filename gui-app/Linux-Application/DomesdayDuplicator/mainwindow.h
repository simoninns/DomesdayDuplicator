/************************************************************************

    mainwindow.h

    Capture application for the Domesday Duplicator
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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>

#include "aboutdialog.h"
#include "configurationdialog.h"
#include "configuration.h"
#include "usbdevice.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void deviceAttachedSignalHandler(void);
    void deviceDetachedSignalHandler(void);
    void configurationChangedSignalHandler(void);

    void on_actionExit_triggered();
    void on_actionTest_mode_toggled(bool arg1);
    void on_actionAbout_triggered();

    void on_actionPreferences_triggered();

private:
    Configuration *configuration;
    UsbDevice *usbDevice;
    QLabel *usbStatusLabel;

    Ui::MainWindow *ui;
    AboutDialog *aboutDialog;
    ConfigurationDialog *configurationDialog;
};

#endif // MAINWINDOW_H
