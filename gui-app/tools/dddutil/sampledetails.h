/************************************************************************

    sampledetails.h

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

#ifndef SAMPLEDETAILS_H
#define SAMPLEDETAILS_H

#include <QObject>
#include <QFile>
#include <QDebug>
#include <QDateTime>

class SampleDetails
{

public:
    SampleDetails();
    bool getInputSampleDetails(QString inputFilename, bool isTenBit);

    QString getSizeOnDisc();
    qint64 getNumberOfSamples();
    qint32 getDurationSeconds();
    QString getDurationString();
    bool getInputFileFormat();

signals:

public slots:

private:
    qint64 sizeOnDisc;
    qint64 numberOfSamples;
    bool isInputFileTenBit;

    qint32 samplesToTenBitBytes(qint32 numberOfSamples);
    qint32 tenBitBytesToSamples(qint32 numberOfBytes);
    qint32 samplesToSixteenBitBytes(qint32 numberOfSamples);
    qint32 sixteenBitBytesToSamples(qint32 numberOfBytes);
};

#endif // RFSAMPLE_H
