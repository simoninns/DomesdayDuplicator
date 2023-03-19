/************************************************************************

    sampledetails.cpp

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

#include "sampledetails.h"

SampleDetails::SampleDetails()
{
    // Set default object values
    sizeOnDisc = 0;
    numberOfSamples = 0;
    isInputFileTenBit = true;
}

// Public methods -----------------------------------------------------------------------------------------------------

// Get the details of the input sample file
// Returns 'true' on success
bool SampleDetails::getInputSampleDetails(QString inputFilename, bool isTenBit)
{
    QFile inputSampleFileHandle(inputFilename);

    if (!inputSampleFileHandle.open(QIODevice::ReadOnly)) {
        // Failed to open input sample file
        qDebug() << "SampleDetails::getInputSampleDetails(): Could not open " << inputFilename << "as input sample file";
        return false;
    }

    // Determine the size on disc and number of samples
    sizeOnDisc = inputSampleFileHandle.size();
    if (isTenBit) numberOfSamples = (sizeOnDisc / 5) * 4; // 10-bit packed
    else numberOfSamples = sizeOnDisc / 2; // 16-bit scaled

    // Set the input sample data format
    isInputFileTenBit = isTenBit;

    // Close the input sample file
    inputSampleFileHandle.close();

    qDebug() << "SampleDetails::getInputSampleDetails(): Sample file is" << inputFilename <<
                "containing" << numberOfSamples << "samples and is" << sizeOnDisc << "bytes";


    // Return with success
    return true;
}

// Public get and set methods -----------------------------------------------------------------------------------------

// Get the size of the sample file on disc and return as a
// readable string
QString SampleDetails::getSizeOnDisc()
{
    QString sizeText;

    if (sizeOnDisc < 102400) {
        sizeText = QString::number(sizeOnDisc) + " Bytes";
    } else if (sizeOnDisc < 1024 * 1024) {
        sizeText = QString::number(sizeOnDisc / 1024) + " KBytes";
    } else {
        sizeText = QString::number(sizeOnDisc / 1024 / 1024) + " MBytes";
    }

    return sizeText;
}

// Get the number of samples contained in the sample file
qint64 SampleDetails::getNumberOfSamples()
{
    return numberOfSamples;
}

// Get the approximate duration of the sample file and
// return it as the number of seconds
qint32 SampleDetails::getDurationSeconds()
{
    return static_cast<qint32>(numberOfSamples / 40000000);
}

// Get the approximate duration of the sample file and
// return it as a time string "hh:mm:ss"
QString SampleDetails::getDurationString()
{
    // Number of samples / 40 MSPS sampling rate
    qint64 duration = numberOfSamples / 40000000;

    // Return a QString in the format hh:mm:ss
    return QDateTime::fromMSecsSinceEpoch(duration * 1000).toUTC().toString("hh:mm:ss");
}

// Get the input file format, returns true if 10-bit
// and false if 16-bit
bool SampleDetails::getInputFileFormat()
{
    return isInputFileTenBit;
}
