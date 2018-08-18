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
    void setTest10bitDataMode(void);
    void setConvertToInt16Mode(void);

private:
    // Filenames for the source and target sample files
    QString sourceSampleFilename;
    QString targetSampleFilename;

    // Mode flags
    bool test10bitDataMode;
    bool convertToInt16Mode;

    // Source file handle
    QFile *sourceSampleFileHandle;
    QFile *targetSampleFileHandle;

    // Variables for tracking the source sample file
    qint64 totalNumberOfBytesRead;
    qint64 locationOfBufferStart;
    bool dataAvailable;

    // Create a buffer for the source sample data (bytes)
    QVector<quint8> sourceSampleBuffer_byte;
    qint32 sourceSampleBufferMaximumSize;

    // Create a buffer for the target sample data (int16)
    QVector<qint16> targetSampleBuffer_int16;

    // Private methods
    void verify10bitTestData(void);
    void convert10BitToInt16(void);
    bool openSourceSampleFile(void);
    void closeSourceSampleFile(void);
    bool openTargetSampleFile(void);
    void closeTargetSampleFile(void);
    void appendSourceSampleData(void);
    void saveTargetSampleData(void);
};

#endif // DDDANALYSE_H
