/************************************************************************

    main.cpp

    Utilities for Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2019 Simon Inns

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

#include "mainwindow.h"
#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Named and versioned like the other two tools, so every shipped binary can be asked
    // which commit it was built from (D21). --version is the only command-line option this
    // one has; everything else it does is driven from the window.
    QCoreApplication::setApplicationName("dddutil");
    QCoreApplication::setApplicationVersion("1.0 (" DDD_VERSION ")");
    QCoreApplication::setOrganizationDomain("domesday86.com");
    QCoreApplication::setOrganizationName("Domesday86");

    QCommandLineParser parser;
    parser.setApplicationDescription(
                "Domesday Duplicator analysis utilities\n"
                "\n"
                "(c)2019-2022 Simon Inns\n"
                "GPLv3 Open-Source - https://www.domesday86.com");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(a);

    MainWindow w;
    w.show();

    return a.exec();
}
