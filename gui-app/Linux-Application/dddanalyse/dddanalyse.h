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

    // Public methods
    bool processSample(void);
    void setSourceSampleFileName(QString filename);
    void setTargetSampleFileName(QString filename);
    void setTest16bitDataMode(void);
    void setTest10bitDataMode(void);

private:
    // Filenames for the source and target sample files
    QString sourceSampleFilename;
    QString targetSampleFilename;

    // Test data mode flags
    bool test16bitDataMode;
    bool test10bitDataMode;

    // Source file handle
    QFile *sourceSampleFileHandle;

    // Variables for tracking the source sample file
    qint32 totalNumberOfWordsRead;
    qint32 locationOfBufferStart;
    bool dataAvailable;

    // Create a buffer for the source sample data (16-bit signed words)
    QVector<qint16> sourceSampleBuffer_int;
    QVector<quint16> sourceSampleBuffer_uint;
    qint32 sourceSampleBufferMaximumSize;

    // Private methods
    void verify16bitTestData(void);
    void verify10bitTestData(void);
    bool openSourceSampleFile(void);
    void closeSourceSampleFile(void);
    qint32 appendSourceSampleData(void);
};

#endif // DDDANALYSE_H
