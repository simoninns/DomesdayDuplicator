/************************************************************************

    playerremotedialog.h

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

#ifndef PLAYERREMOTEDIALOG_H
#define PLAYERREMOTEDIALOG_H

#include <QDialog>

namespace Ui {
class PlayerRemoteDialog;
}

class PlayerRemoteDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PlayerRemoteDialog(QWidget *parent = nullptr);
    ~PlayerRemoteDialog();

private:
    Ui::PlayerRemoteDialog *ui;
};

#endif // PLAYERREMOTEDIALOG_H
