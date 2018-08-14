/************************************************************************

    dddanalyse.h

    LaserDisc RF sample analysis utility
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

#ifndef DDDANALYSE_H
#define DDDANALYSE_H

#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <iostream>

class DddAnalyse
{
public:
    DddAnalyse();

    bool processSample(void);
    void setSourceSampleFileName(QString filename);
    void setTargetSampleFileName(QString filename);
    void setTestDataMode(void);

private:
    // Filenames for the source and target sample files
    QString sourceSampleFilename;
    QString targetSampleFilename;

    // Test data mode flag
    bool testDataMode;

    // Source file handle
    QFile *sourceSampleFileHandle;
};

#endif // DDDANALYSE_H
