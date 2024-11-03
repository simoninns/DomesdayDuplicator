/************************************************************************

    configurationdialog.h

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
#pragma once
#include <QDialog>
#include <QFileDialog>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QAbstractButton>
#include <vector>
#include <string>
#include <memory>
#include "configuration.h"

namespace Ui {
class ConfigurationDialog;
}

class ConfigurationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigurationDialog(QWidget *parent = nullptr);
    ~ConfigurationDialog();

    void updateDeviceList(const std::vector<std::string>& deviceList);
    void loadConfiguration(const Configuration& configuration);
    void saveConfiguration(Configuration& configuration);

signals:
    void configurationChanged();

private slots:
    void on_captureDirectoryPushButton_clicked();
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

    void on_buttonBox_clicked(QAbstractButton *button);

private:
    std::unique_ptr<Ui::ConfigurationDialog> ui;
};
