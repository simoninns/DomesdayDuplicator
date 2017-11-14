/************************************************************************

    domesdayduplicator.h

    QT/Linux RF Capture application main window header
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

#ifndef DOMESDAYDUPLICATOR_H
#define DOMESDAYDUPLICATOR_H

#include <QMainWindow>
#include <QDebug>
#include <QLabel>

#include "streamer.h"

class QLabel;

namespace Ui {
class domesdayDuplicator;
}

class domesdayDuplicator : public QMainWindow
{
    Q_OBJECT

public:
    explicit domesdayDuplicator(QWidget *parent = 0);
    ~domesdayDuplicator();

private slots:
    void on_transferButton_clicked();

private:
    Ui::domesdayDuplicator *ui;

    QLabel *status;
    streamer *dataStreamer;
};

#endif // DOMESDAYDUPLICATOR_H
