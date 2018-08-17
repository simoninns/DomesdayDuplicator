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
    test16bitDataMode = false;
    test10bitDataMode = false;

    // Initialise source sample tracking
    totalNumberOfWordsRead = 0;
    locationOfBufferStart = 0;
    dataAvailable = true;

    // Set the maximum buffer sizes (in words)
    sourceSampleBufferMaximumSize = 8192;
}

// Public methods -------------------------------------------------------------

// Process the sample
//
// Returns:
//      True on success, false on failure
bool DddAnalyse::processSample(void)
{
    // Open the source sample file
    if (!openSourceSampleFile()) {
        // Opening source sample file failed
        return false;
    }

    if (test10bitDataMode) {
        // Verify the 10-bit source sample
        verify10bitTestData();
    } else if (test16bitDataMode) {
        // Verify the 16-bit signed source sample
        verify16bitTestData();
    }

    // Close the source sample file
    closeSourceSampleFile();

    qDebug() << "Processing completed";

    // Exit with success
    return true;
}

void DddAnalyse::verify16bitTestData(void)
{
    // Show some information to the user
    std::cout << "dddanalyse - Analysing 16-bit test data sample..." << std::endl;

    // Process the source sample file
    qint32 currentValue = 0;
    bool firstRun = true;
    bool inError = false;

    while (dataAvailable && !inError) {
        // Read data from the source sample file into the buffer
        appendSourceSampleData();

        // Process the buffer

        for (qint32 pointer = 0; pointer < sourceSampleBuffer_int.size(); pointer++) {
            // First run?
            if (firstRun) {
                currentValue = sourceSampleBuffer_int[pointer];
                firstRun = false;
                std::cout << "Initial value at byte " << locationOfBufferStart + pointer << " is " << currentValue << std::endl;
            } else {
                // Increment the current expected value
                currentValue += 64; // Value is in 10 bit increments scaled to 16 bits

                // Range check the current value
                if (currentValue == 32768) currentValue = -32768;

                // Check the buffer against the expected value
                if (currentValue != sourceSampleBuffer_int[pointer]) {
                    // Test data is not correct!
                    std::cout << "ERROR! Expected: " << currentValue << " got " << sourceSampleBuffer_int[pointer] << " at byte position " << locationOfBufferStart + pointer << std::endl;
                    inError = true;
                    break;
                }
            }
        }

        // Remove the processed words from the buffer
        sourceSampleBuffer_int.resize(0);
    }

    std::cout << "Test data analysis complete." << std::endl;
}

void DddAnalyse::verify10bitTestData(void)
{
    // Show some information to the user
    std::cout << "dddanalyse - Analysing 10-bit test data sample..." << std::endl;

    // Process the source sample file
    quint32 currentValue = 0;
    bool firstRun = true;
    bool inError = false;

    while (dataAvailable && !inError) {
        // Read data from the source sample file into the buffer
        appendSourceSampleData();

        // Process the buffer

        // For 10-bit data we have to process 5 words at a time
        for (qint32 pointer = 0; pointer < sourceSampleBuffer_uint.size(); pointer += 5) {
            // Unpack the 5 16-bit words into 8 10-bit values
            QVector<quint16> tenBitBuffer;
            tenBitBuffer.resize(8);

            // Unpacking algorithm is as follows:
            //
            //              111111                                  111111
            //              54321098 76543210                       54321098 76543210
            //
            // (value0 & 0b 11111111 11000000) >> 06
            // (value0 & 0b 00000000 00111111) << 04 + (value1 & 0b 11110000 00000000) >> 12
            // (value1 & 0b 00001111 11111100) >> 02
            // (value1 & 0b 00000000 00000011) << 08 + (value2 & 0b 11111111 00000000) >> 08
            // (value2 & 0b 00000000 11111111) << 02 + (value3 & 0b 11000000 00000000) >> 14
            // (value3 & 0b 00111111 11110000) >> 04
            // (value3 & 0b 00000000 00001111) << 06 + (value4 & 0b 11111100 00000000) >> 10
            // (value4 & 0b 00000011 11111111)
            //
            // ...or in hex:
            //
            // ((value0 & 0xFFC0) >> 06)
            // ((value0 & 0x003F) << 04) + ((value1 & 0xF000) >> 12)
            //  (value1 & 0x0FFC) >> 02
            // ((value1 & 0x0003) << 08) + ((value2 & 0xFF00) >> 08)
            // ((value2 & 0x00FF) << 02) + ((value3 & 0xC000) >> 14)
            //  (value3 & 0x3FF0) >> 04
            // ((value3 & 0x000F) << 06) + ((value4 & 0xFC00) >> 10)
            //  (value4 & 0x03FF)

            // and now in C++...
            tenBitBuffer[0] = ((sourceSampleBuffer_uint[0 + pointer] & 0xFFC0) >> 6);
            tenBitBuffer[1] = static_cast<quint16>(((sourceSampleBuffer_uint[0 + pointer] & 0x003F) << 4))
                    + ((sourceSampleBuffer_uint[1 + pointer] & 0xF000) >> 12);
            tenBitBuffer[2] =  (sourceSampleBuffer_uint[1 + pointer] & 0x0FFC) >> 2;
            tenBitBuffer[3] = static_cast<quint16>(((sourceSampleBuffer_uint[1 + pointer] & 0x0003) << 8))
                    + ((sourceSampleBuffer_uint[2 + pointer] & 0xFF00) >>  8);
            tenBitBuffer[4] = static_cast<quint16>(((sourceSampleBuffer_uint[2 + pointer] & 0x00FF) << 2))
                    + ((sourceSampleBuffer_uint[3 + pointer] & 0xC000) >> 14);
            tenBitBuffer[5] =  (sourceSampleBuffer_uint[3 + pointer] & 0x3FF0) >> 4;
            tenBitBuffer[6] = static_cast<quint16>(((sourceSampleBuffer_uint[3 + pointer] & 0x000F) << 6))
                    + ((sourceSampleBuffer_uint[4 + pointer] & 0xFC00) >> 10);
            tenBitBuffer[7] =  (sourceSampleBuffer_uint[4 + pointer] & 0x03FF);

            for (qint32 tenBitPointer = 0; tenBitPointer < 8; tenBitPointer++) {
                // First run?
                if (firstRun) {
                    currentValue = tenBitBuffer[tenBitPointer];
                    firstRun = false;
                    std::cout << "Initial value at byte " << locationOfBufferStart + pointer << " is " << currentValue << std::endl;
                } else {
                    // Increment the current expected value
                    currentValue++;

                    // Range check the current value
                    if (currentValue == 1024) currentValue = 0;

                    // Check the buffer against the expected value
                    if (currentValue != tenBitBuffer[tenBitPointer]) {
                        // Test data is not correct!

                        // This next line isn't correct
                        std::cout << "ERROR! Expected: " << currentValue << " got " << tenBitBuffer[tenBitPointer] << " at byte position " << locationOfBufferStart + pointer << std::endl;
                        inError = true;
                        break;
                    }
                }
            }

            if (inError) break;
        }

        // Remove the processed words from the buffer
        sourceSampleBuffer_int.resize(0);
    }

    std::cout << "Test data analysis complete." << std::endl;
}

// Open the source sample file
bool DddAnalyse::openSourceSampleFile(void)
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

    // Initialise source sample tracking
    totalNumberOfWordsRead = 0;
    locationOfBufferStart = 0;
    dataAvailable = true;

    // Exit with success
    return true;
}

// Close the source sample file
void DddAnalyse::closeSourceSampleFile(void)
{
    // Close the source sample file
    // Is a source video file open?
    if (sourceSampleFileHandle != nullptr) {
            sourceSampleFileHandle->close();
    }

    // Clear the file handle pointer
    sourceSampleFileHandle = nullptr;

    // Flag that data is not available
    dataAvailable = false;
}

// Read data from the source sample - amount of data specified in words
// returns the number of words read
qint32 DddAnalyse::appendSourceSampleData(void)
{
    // Store the original size of the buffer
    qint32 originalBufferSize = sourceSampleBuffer_int.size();

    // How many buffer elements do we need to make it full?
    qint64 requiredWords = sourceSampleBufferMaximumSize - originalBufferSize;

    // Range check...
    if (requiredWords == 0) {
        qDebug() << "Append source sample data was called, but the buffer was already full";
        return 0;
    }

    // Resize the buffer to the maximum size allowed
    if (test16bitDataMode) sourceSampleBuffer_int.resize(sourceSampleBufferMaximumSize);
    else sourceSampleBuffer_uint.resize(sourceSampleBufferMaximumSize);

    // Attempt to read enough words to fill the buffer
    qint32 numberOfWordsReceived = 0;
    qint64 receivedBytes = 0;

    qDebug() << "Requesting" << requiredWords << "words from source sample file";
    do {
        if (test16bitDataMode) {
            receivedBytes = sourceSampleFileHandle->read(reinterpret_cast<char *>(sourceSampleBuffer_int.data()) +
                                                     (originalBufferSize * static_cast<qint64>(sizeof(quint16))),
                                                     (requiredWords * static_cast<qint64>(sizeof(quint16))));
        } else {
            receivedBytes = sourceSampleFileHandle->read(reinterpret_cast<char *>(sourceSampleBuffer_uint.data()) +
                                                     (originalBufferSize * static_cast<qint64>(sizeof(quint16))),
                                                     (requiredWords * static_cast<qint64>(sizeof(quint16))));
        }

        // Calculate the received number of words
        numberOfWordsReceived += (receivedBytes / static_cast<qint32>(sizeof(quint16)));
    } while (receivedBytes != 0 && numberOfWordsReceived < requiredWords);
    qDebug() << "Received" << numberOfWordsReceived << "elements from source video file";

    totalNumberOfWordsRead += numberOfWordsReceived;
    if (test16bitDataMode) locationOfBufferStart = totalNumberOfWordsRead - sourceSampleBuffer_int.size();
    else locationOfBufferStart = totalNumberOfWordsRead - sourceSampleBuffer_uint.size();

    // Did we run out of source data?
    if (numberOfWordsReceived < requiredWords) {
        qDebug() << "Reached the end of the source sample file before receiving the required number of words";

        // Resize the buffer correctly
        if (test16bitDataMode) sourceSampleBuffer_int.resize(originalBufferSize + numberOfWordsReceived);
        else sourceSampleBuffer_uint.resize(originalBufferSize + numberOfWordsReceived);
        dataAvailable = false;
        return numberOfWordsReceived;
    }

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

// Select 16-bit test data mode
void DddAnalyse::setTest16bitDataMode(void)
{
    test16bitDataMode = true;
    test10bitDataMode = false;
}

// Select 10-bit test data mode
void DddAnalyse::setTest10bitDataMode(void)
{
    test10bitDataMode = true;
    test16bitDataMode = false;
}

