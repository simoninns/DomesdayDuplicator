/************************************************************************

    dddanalyse.cpp

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

#include "dddanalyse.h"

DddAnalyse::DddAnalyse()
{
    // Set default configuration
    setSourceSampleFileName(""); // Default is stdin
    setTargetSampleFileName(""); // Default is stdout
    testDataMode = true; // Default is test data verification mode
}

// Public methods -------------------------------------------------------------

// Process the sample
//
// Returns:
//      True on success, false on failure
bool DddAnalyse::processSample(void)
{
    // Open the source sample file
    // Do we have a file name for the source video file?
    if (sourceSampleFilename.isEmpty()) {
        // No source video file name was specified, using stdin instead
        qDebug() << "No source video filename was provided, using stdin";
        sourceSampleFileHandle = new QFile;
        if (!sourceSampleFileHandle->open(stdin, QIODevice::ReadOnly)) {
            // Failed to open stdin
            qWarning() << "Could not open stdin as source sample file";
            return false;
        }
        qInfo() << "Reading sample data from stdin";
    } else {
        // Open source sample file for reading
        sourceSampleFileHandle = new QFile(sourceSampleFilename);
        if (!sourceSampleFileHandle->open(QIODevice::ReadOnly)) {
            // Failed to open source sample file
            qDebug() << "Could not open " << sourceSampleFilename << "as source sample file";
            return false;
        }
        qDebug() << "Source sample file is" << sourceSampleFilename;
    }

    // Process the source sample file
    qint64 sampleElementsInBuffer = 0;
    qint64 receivedSampleBytes = 0;
    qint64 bufferStartPositionInFile = 0;

    // Define the sample input buffer as 8192 16-bit words
    QVector<qint16> sampleInputBuffer;
    sampleInputBuffer.resize(8192);

    qint32 currentValue = 0;
    bool firstRun = true;
    bool inError = false;

    std::cout << "dddanalyse - Analysing test data..." << std::endl;

    // Continue reading file until the end of the file
    while (!sourceSampleFileHandle->atEnd() && !inError) {
        qDebug() << "Requesting" << (sampleInputBuffer.size()) <<
                    "elements from sample file to fill sample buffer";

        // Read from the sample input file and store in the sample buffer vector
        // This operation uses bytes, so we multiply the elements by the size of the data-type
        receivedSampleBytes = sourceSampleFileHandle->read(reinterpret_cast<char *>(sampleInputBuffer.data()),
                                                           (sampleInputBuffer.size() * static_cast<qint64>(sizeof(qint16))));

        // If received bytes is -1, the sample read operation failed for some unknown reason
        // If received bytes is 0, it's probably because we are reading from stdin with nothing avaiable
        if (receivedSampleBytes < 0) {
            qCritical() << "read() operation on sample input file returned error - aborting";
            return -1;
        }
        qDebug() << "Received" << (receivedSampleBytes / static_cast<qint64>(sizeof(qint16))) << "elements (" << receivedSampleBytes <<
                    "bytes ) from file read operation";

        // Add the received elements count to the sample elements in buffer count
        sampleElementsInBuffer = static_cast<qint64>(receivedSampleBytes / static_cast<qint64>(sizeof(qint16)));

        for (qint32 pointer = 0; pointer < sampleElementsInBuffer; pointer++) {
            // First run?
            if (firstRun) {
                currentValue = sampleInputBuffer[pointer];
                firstRun = false;
                std::cout << "Initial value at byte " << bufferStartPositionInFile << " is " << currentValue << std::endl;
            } else {
                // Increment the current expected value
                currentValue += 64; // Value is in 10 bit increments scaled to 16 bits

                // Range check the current value
                if (currentValue == 32768) currentValue = -32768;

                // Check the buffer against the expected value
                if (currentValue != sampleInputBuffer[pointer]) {
                    // Test data is not correct!
                    std::cout << "ERROR! Expected: " << currentValue << " got " << sampleInputBuffer[pointer] << " at byte position " << bufferStartPositionInFile + pointer << std::endl;
                    inError = true;
                    break;
                }
            }
        }

        // Buffer processed, add to start position
        bufferStartPositionInFile += receivedSampleBytes;
    }

    // Close the source sample file
    // Is a source video file open?
        if (sourceSampleFileHandle != nullptr) {
            sourceSampleFileHandle->close();
    }

    std::cout << "Test data analysis complete." << std::endl;
    qDebug() << "Processing completed";

    // Exit with success
    return true;
}

//  Get and set methods -------------------------------------------------------

// Set the filename of the source sample file
void DddAnalyse::setSourceSampleFileName(QString filename)
{
    // Set the file name
    sourceSampleFilename = filename;
}

// Set the filename of the target sample file
void DddAnalyse::setTargetSampleFileName(QString filename)
{
    // Set the file name
    targetSampleFilename = filename;
}

void DddAnalyse::setTestDataMode(void)
{
    testDataMode = true;
}

