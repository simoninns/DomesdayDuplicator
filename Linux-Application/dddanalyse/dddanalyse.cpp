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
    test10bitDataMode = false;

    // Initialise source sample tracking
    totalNumberOfBytesRead = 0;
    locationOfBufferStart = 0;
    dataAvailable = true;

    // Set the maximum buffer sizes (in bytes)
    sourceSampleBufferMaximumSize = 10240;
}

// Public methods -------------------------------------------------------------

// Process the sample
//
// Returns:
//      True on success, false on failure
bool DddAnalyse::processSample(void)
{
    // Test 10-bit test-data for errors?
    if (test10bitDataMode) {
        // Open the source sample file
        if (!openSourceSampleFile()) {
            // Opening source sample file failed
            return false;
        }

        // Verify the 10-bit source sample
        verify10bitTestData();

        // Close the source sample file
        closeSourceSampleFile();
    }

    // Convert 10-bit data to int16?
    if (convertToInt16Mode) {
        // Open the source sample file
        if (!openSourceSampleFile()) {
            // Opening source sample file failed
            return false;
        }

        // Open the target sample file
        if (!openTargetSampleFile()) {
            // Opening target sample file failed
            return false;
        }

        // Convert the sample
        convert10BitToInt16();

        // Close the source sample file
        closeSourceSampleFile();

        // Close the target sample file
        closeSourceSampleFile();
    }

    qDebug() << "Processing completed";

    // Exit with success
    return true;
}

void DddAnalyse::verify10bitTestData(void)
{
    // Show some information to the user
    std::cout << "dddanalyse - Analysing 10-bit test data sample..." << std::endl;

    // Process the source sample file
    quint32 currentValue = 0;
    bool firstRun = true;
    bool inError = false;

    QVector<quint16> tenBitBuffer;
    tenBitBuffer.resize(4);

    while (dataAvailable && !inError) {
        // Read data from the source sample file into the buffer
        appendSourceSampleData();

        // Process the buffer

        // For 10-bit data we have to process 5 bytes at a time
        for (qint32 pointer = 0; pointer < sourceSampleBuffer_byte.size(); pointer += 5) {
            // Unpack the 5 bytes into 4x 10-bit values

            // Original
            // 0: xxxx xx00 0000 0000
            // 1: xxxx xx11 1111 1111
            // 2: xxxx xx22 2222 2222
            // 3: xxxx xx33 3333 3333
            //
            // Packed:
            // 0: 0000 0000 0011 1111
            // 2: 1111 2222 2222 2233
            // 4: 3333 3333

            tenBitBuffer[0]  = static_cast<quint16>(static_cast<quint16>(sourceSampleBuffer_byte[pointer + 0]) << 2);
            tenBitBuffer[0] += static_cast<quint16>(sourceSampleBuffer_byte[pointer + 1] & 0xC0) >> 6;

            tenBitBuffer[1]  = static_cast<quint16>(static_cast<quint16>(sourceSampleBuffer_byte[pointer + 1] & 0x3F) << 4);
            tenBitBuffer[1] += static_cast<quint16>(sourceSampleBuffer_byte[pointer + 2] & 0xF0) >> 4;

            tenBitBuffer[2]  = static_cast<quint16>(static_cast<quint16>(sourceSampleBuffer_byte[pointer + 2] & 0x0F) << 6);
            tenBitBuffer[2] += static_cast<quint16>(sourceSampleBuffer_byte[pointer + 3] & 0xFC) >> 2;

            tenBitBuffer[3]  = static_cast<quint16>(static_cast<quint16>(sourceSampleBuffer_byte[pointer + 3] & 0x03) << 8);
            tenBitBuffer[3] += static_cast<quint16>(sourceSampleBuffer_byte[pointer + 4]);

            for (qint32 tenBitPointer = 0; tenBitPointer < 4; tenBitPointer++) {
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
        sourceSampleBuffer_byte.resize(0);
    }

    if (!inError) std::cout << "Test data analysis completed successfully." << std::endl;
    else std::cout << "Test data analysis found data errors." << std::endl;
}

// Convert 10-bit sample to scaled 16-bit signed integer sample
void DddAnalyse::convert10BitToInt16(void)
{
    // Show some information to the user
    std::cout << "dddanalyse - Converting 10-bit data sample into int16..." << std::endl;

    while (dataAvailable) {
        // Read data from the source sample file into the buffer
        appendSourceSampleData();

        // Calculate the require target buffer size
        targetSampleBuffer_int16.resize((sourceSampleBuffer_byte.size() / 5) * 4);
        qint32 targetBufferPointer = 0;

        // Process the buffer
        // For 10-bit data we have to process 5 bytes at a time
        for (qint32 pointer = 0; pointer < sourceSampleBuffer_byte.size(); pointer += 5) {
            // Unpack the 5 bytes into 4x 10-bit values

            // Original
            // 0: xxxx xx00 0000 0000
            // 1: xxxx xx11 1111 1111
            // 2: xxxx xx22 2222 2222
            // 3: xxxx xx33 3333 3333
            //
            // Packed:
            // 0: 0000 0000 0011 1111
            // 2: 1111 2222 2222 2233
            // 4: 3333 3333

            targetSampleBuffer_int16[targetBufferPointer + 0]  = static_cast<qint16>(static_cast<quint16>(sourceSampleBuffer_byte[pointer + 0]) << 2);
            targetSampleBuffer_int16[targetBufferPointer + 0] += static_cast<qint16>(sourceSampleBuffer_byte[pointer + 1] & 0xC0) >> 6;

            targetSampleBuffer_int16[targetBufferPointer + 1]  = static_cast<qint16>(static_cast<quint16>(sourceSampleBuffer_byte[pointer + 1] & 0x3F) << 4);
            targetSampleBuffer_int16[targetBufferPointer + 1] += static_cast<qint16>(sourceSampleBuffer_byte[pointer + 2] & 0xF0) >> 4;

            targetSampleBuffer_int16[targetBufferPointer + 2]  = static_cast<qint16>(static_cast<quint16>(sourceSampleBuffer_byte[pointer + 2] & 0x0F) << 6);
            targetSampleBuffer_int16[targetBufferPointer + 2] += static_cast<qint16>(sourceSampleBuffer_byte[pointer + 3] & 0xFC) >> 2;

            targetSampleBuffer_int16[targetBufferPointer + 3]  = static_cast<qint16>(static_cast<quint16>(sourceSampleBuffer_byte[pointer + 3] & 0x03) << 8);
            targetSampleBuffer_int16[targetBufferPointer + 3] += static_cast<qint16>(sourceSampleBuffer_byte[pointer + 4]);

            // Scale the data
            targetSampleBuffer_int16[targetBufferPointer + 0] = (targetSampleBuffer_int16[targetBufferPointer + 0] - 512) * 64;
            targetSampleBuffer_int16[targetBufferPointer + 1] = (targetSampleBuffer_int16[targetBufferPointer + 1] - 512) * 64;
            targetSampleBuffer_int16[targetBufferPointer + 2] = (targetSampleBuffer_int16[targetBufferPointer + 2] - 512) * 64;
            targetSampleBuffer_int16[targetBufferPointer + 3] = (targetSampleBuffer_int16[targetBufferPointer + 3] - 512) * 64;

            // Increment the target buffer pointer
            targetBufferPointer += 4;
        }

        // Save the target buffer to the target file
        saveTargetSampleData();

        // Remove the processed words from the buffer
        sourceSampleBuffer_byte.resize(0);
    }

    std::cout << "Sample data conversion completed successfully." << std::endl;
}

// Open the source sample file
bool DddAnalyse::openSourceSampleFile(void)
{
    // Open the source sample file
    // Do we have a file name for the source sample file?
    if (sourceSampleFilename.isEmpty()) {
        // No source sample file name was specified, using stdin instead
        qDebug() << "No source sample filename was provided, using stdin";
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
        qDebug() << "Source sample file is" << sourceSampleFilename << "and is" << sourceSampleFileHandle->size() << "bytes in length";
    }

    // Initialise source sample tracking
    totalNumberOfBytesRead = 0;
    locationOfBufferStart = 0;
    dataAvailable = true;

    // Exit with success
    return true;
}

// Close the source sample file
void DddAnalyse::closeSourceSampleFile(void)
{
    // Close the source sample file
    // Is a source sample file open?
    if (sourceSampleFileHandle != nullptr) {
            sourceSampleFileHandle->close();
    }

    // Clear the file handle pointer
    sourceSampleFileHandle = nullptr;

    // Flag that data is not available
    dataAvailable = false;
}

// Open the target sample file
bool DddAnalyse::openTargetSampleFile(void)
{
    // Open the target sample file
    // Do we have a file name for the target sample file?
    if (targetSampleFilename.isEmpty()) {
        // No target sample file name was specified, using stdin instead
        qDebug() << "No target sample filename was provided, using stdout";
        targetSampleFileHandle = new QFile;
        if (!targetSampleFileHandle->open(stdout, QIODevice::WriteOnly)) {
            // Failed to open stdout
            qWarning() << "Could not open stdout as target sample file";
            return false;
        }
        qInfo() << "Writing sample data to stdout";
    } else {
        // Open target sample file for writing
        targetSampleFileHandle = new QFile(targetSampleFilename);
        if (!targetSampleFileHandle->open(QIODevice::WriteOnly)) {
            // Failed to open target sample file
            qDebug() << "Could not open " << targetSampleFilename << "as target sample file";
            return false;
        }
        qDebug() << "Target sample file is" << targetSampleFilename;
    }

    // Exit with success
    return true;
}

// Close the target sample file
void DddAnalyse::closeTargetSampleFile(void)
{
    // Close the target sample file
    // Is a target sample file open?
    if (targetSampleFileHandle != nullptr) {
            targetSampleFileHandle->close();
    }

    // Clear the file handle pointer
    targetSampleFileHandle = nullptr;
}

// Read data from the source sample - amount of data specified in words
// returns the number of words read
void DddAnalyse::appendSourceSampleData(void)
{
    // Store the original size of the buffer
    qint64 originalBufferSize = sourceSampleBuffer_byte.size();

    // How many buffer elements do we need to make it full?
    qint64 requiredBytes = sourceSampleBufferMaximumSize - originalBufferSize;

    // Range check...
    if (requiredBytes == 0) {
        qDebug() << "Append source sample data was called, but the buffer was already full";
        return;
    }

    // Resize the buffer to the maximum size allowed
    sourceSampleBuffer_byte.resize(sourceSampleBufferMaximumSize);

    // Attempt to read enough bytes to fill the buffer
    qint64 receivedBytes = 0;

    do {
        receivedBytes = sourceSampleFileHandle->read(reinterpret_cast<char *>(sourceSampleBuffer_byte.data()) +
                                                     originalBufferSize,
                                                     requiredBytes
                                                     );
    } while (receivedBytes != 0 && receivedBytes < requiredBytes);
    //qDebug() << "Received" << receivedBytes << "bytes from source sample file";

    totalNumberOfBytesRead += receivedBytes;
    locationOfBufferStart = totalNumberOfBytesRead - sourceSampleBuffer_byte.size();

    // Did we run out of source data?
    if (receivedBytes < requiredBytes) {
        if (receivedBytes == 0) {
            qDebug() << "Reached the end of the source sample file";
        } else {
            qDebug() << "Reached the end of the source sample file before receiving the required number of bytes";
            qDebug() << "Received bytes =" << receivedBytes << " and required bytes =" << requiredBytes;
        }

        // Resize the buffer correctly
        sourceSampleBuffer_byte.resize(static_cast<qint32>(originalBufferSize + receivedBytes));
        dataAvailable = false;
        return;
    }
}

// Save data from the target sample buffer to the target sample file
void DddAnalyse::saveTargetSampleData(void)
{
    qint64 writeResult = 0;
    targetSampleFileHandle->write(reinterpret_cast<char *>(targetSampleBuffer_int16.data()),
                                  targetSampleBuffer_int16.size() * static_cast<qint32>(sizeof(qint16))
                                  );

    if (writeResult == -1) qDebug() << "Writing to target sample file failed!";
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

// Select 10-bit test data mode
void DddAnalyse::setTest10bitDataMode(void)
{
    test10bitDataMode = true;
}

// Select convert 10-bit to int16 mode
void DddAnalyse::setConvertToInt16Mode(void)
{
    convertToInt16Mode = true;
}
