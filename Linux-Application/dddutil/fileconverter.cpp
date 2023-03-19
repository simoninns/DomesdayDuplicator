/************************************************************************

    fileconverter.cpp

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

#include "fileconverter.h"

FileConverter::FileConverter(QObject *parent) : QThread(parent)
{
    // Thread control variables
    restart = false; // Setting this to true starts a conversion
    cancel = false; // Setting this to true cancels the conversion in progress
    abort = false; // Setting this to true ends the thread process
}

FileConverter::~FileConverter()
{
    mutex.lock();
    abort = true;
    condition.wakeOne();
    mutex.unlock();

    wait();
}

// Thread handling methods --------------------------------------------------------------------------------------------

// Convert input file to output file.  This thread wrapper passes the parameters
// to the object and restarts the run() function
void FileConverter::convertInputFileToOutputFile(QString inputFilename, QString outputFilename,
                                                 QTime startTime, QTime endTime,
                                                 bool isInputTenBit, bool isOutputTenBit)
{
    QMutexLocker locker(&mutex);

    // Move all the parameters to be local
    this->inputFilename = inputFilename;
    this->outputFilename = outputFilename;
    this->isInputTenBit = isInputTenBit;
    this->isOutputTenBit = isOutputTenBit;
    this->startTime = startTime;
    this->endTime = endTime;

    // Is the run process already running?
    if (!isRunning()) {
        // Yes, start with low priority
        start(LowPriority);
    } else {
        // No, set the restart condition
        restart = true;
        cancel = false;
        condition.wakeOne();
    }
}

// Primary processing loop for the thread
void FileConverter::run()
{
    qDebug() << "FileConverter::run(): Thread running";

    while(!abort) {
        // Lock and copy all parameters to 'thread-safe' variables
        mutex.lock();
        inputFilenameTs = this->inputFilename;
        outputFilenameTs = this->outputFilename;
        isInputTenBitTs = this->isInputTenBit;
        isOutputTenBitTs = this->isOutputTenBit;
        startTimeTs = this->startTime;
        endTimeTs = this->endTime;
        mutex.unlock();

        // Start the sample conversion
        if (convertSampleStart()) {
            // Process the conversion until completed
            bool notComplete = true;
            qreal percentageCompleteReal = 0;
            qint32 percentageComplete = 0;

            while (notComplete && !cancel) {
                notComplete = convertSampleProcess();

                // Calculate the completion percentage
                percentageCompleteReal = (100 / static_cast<qreal>(samplesToConvertTs)) * static_cast<qreal>(numberOfSampleProcessedTs);
                percentageComplete = static_cast<qint32>(percentageCompleteReal);
                qDebug() << "FileConverter::run(): Processed" << numberOfSampleProcessedTs << "of" <<
                            samplesToConvertTs << "(" << percentageComplete << "%)";

                // Emit a signal showing the progress
                emit percentageProcessed(percentageComplete);
            }

            // Stop the sample conversion
            convertSampleStop();

            // Reset the cancel flag
            if (cancel) qDebug() << "FileConverter::run(): Conversion cancelled";
            cancel = false;
        }

        // Emit a signal showing the conversion is complete
        emit completed();

        // Sleep the thread until we are restarted
        mutex.lock();
        if (!restart && !abort) condition.wait(&mutex);
        restart = false;
        mutex.unlock();
    }
    qDebug() << "FileConverter::run(): Thread aborted";
}

// Function sets the cancel flag (which terminates the conversion if in progress)
void FileConverter::cancelConversion()
{
    qDebug() << "FileConverter::cancelConversion(): Setting cancel conversion flag";
    cancel = true;
}

// Function sets the abort flag (which terminates the run() loop if in progress)
void FileConverter::quit()
{
    qDebug() << "FileConverter::quit(): Setting thread abort flag";
    abort = true;
}

// File conversion methods --------------------------------------------------------------------------------------------

// Open the files and get ready to convert
bool FileConverter::convertSampleStart()
{
    qDebug() << "FileConverter::convertSampleStart(): Saving output file as " << outputFilenameTs;

    if (isInputTenBitTs) qDebug() << "FileConverter::convertSampleStart(): Reading in 10-bit format";
    else qDebug() << "FileConverter::convertSampleStart(): Reading in 16-bit format";

    if (isOutputTenBitTs) qDebug() << "FileConverter::convertSampleStart(): Writing in 10-bit format";
    else qDebug() << "FileConverter::convertSampleStart(): Writing in 16-bit format";

    // Open the input sample
    inputSample = new InputSample(nullptr, inputFilename, isInputTenBit);

    // Is the input sample valid?
    if (!inputSample->isInputSampleValid()) {
        // Opening input sample failed!
        qDebug() << "AnalyseTestData::analyseSampleStart(): Could not open input sample file!";

        // Destroy the input sample object
        inputSample->deleteLater();
        return false;
    }

    // Open the output sample file
    if (!openOutputSample(outputFilenameTs)) {
        // Opening output sample failed!
        qDebug() << "FileConverter::convertSampleStart(): Could not open output sample file!";
        return false;
    }

    // Reset the processed sample counter
    numberOfSampleProcessedTs = 0;

    // Calculate the start and end samples based on the QTime parameters and a sample
    // rate of 40,000,000 samples per second
    qint32 durationSeconds = static_cast<qint32>(inputSample->getNumberOfSamples() / 40000000);
    qint32 startSeconds = QTime(0, 0, 0).secsTo(startTimeTs);
    qint32 endSeconds = QTime(0, 0, 0).secsTo(endTimeTs);

    startSampleTs = static_cast<qint64>(startSeconds) * 40000000;
    endSampleTs = static_cast<qint64>(endSeconds) * 40000000;

    // If the endSeconds is the same as the durationSeconds, set the end sample
    // to the total number of samples in the input file (to prevent clipping due
    // to rounding errors)
    if (endSeconds == durationSeconds) endSampleTs = inputSample->getNumberOfSamples();
    samplesToConvertTs = endSampleTs - startSampleTs;

    qDebug() << "FileConverter::convertSampleStart(): startSeconds =" << startSeconds <<
                "endSeconds =" << endSeconds << "total seconds =" << endSeconds - startSeconds;
    qDebug() << "FileConverter::convertSampleStart(): startSample =" << startSampleTs <<
                "endSample =" << endSampleTs << "samples to convert =" << samplesToConvertTs;

    // Move the sample position to the start sample
    if (startSampleTs != 0) inputSample->seek(startSampleTs);

    // Return success
    return true;
}

// Process a buffer of sample data
bool FileConverter::convertSampleProcess()
{
    // Define a sample buffer for the transfer of data
    QVector<quint16> sampleBuffer;

    // Read the input sample data
    qint32 maximumBufferSize = 102400000;
    if ((numberOfSampleProcessedTs + maximumBufferSize) >= samplesToConvertTs)
        maximumBufferSize = static_cast<qint32>(samplesToConvertTs - numberOfSampleProcessedTs);

    sampleBuffer = inputSample->read(maximumBufferSize);
    numberOfSampleProcessedTs += sampleBuffer.size();

    // Did we get data?
    if (sampleBuffer.size() > 0) {
        // Write the input sample data to the output sample
        writeOutputSample(sampleBuffer, isOutputTenBitTs);
    } else {
        // No sample data left, return false
        qDebug() << "FileConverter::convertSampleProcess(): No more data to convert";
        return false;
    }

    // Have we finished processing all the samples?
    if (numberOfSampleProcessedTs >= samplesToConvertTs) {
        qDebug() << "FileConverter::convertSampleProcess():" << numberOfSampleProcessedTs << "of"
                 << samplesToConvertTs << "converted. Done.";
        return false;
    }

    // Data still available, return true
    return true;
}

// Close the sample files and clean up
void FileConverter::convertSampleStop()
{
    // Destroy the input sample object
    inputSample->deleteLater();

    // Close the output sample file
    closeOutputSample();
}

// Open the output RF sample
// Returns 'true' on success
bool FileConverter::openOutputSample(QString filename)
{
    // Open output sample file for writing
    outputSampleFileHandleTs = new QFile(filename);
    if (!outputSampleFileHandleTs->open(QIODevice::WriteOnly)) {
        // Failed to open output sample file
        qDebug() << "FileConverter::openOutputSample(): Could not open " << filename << "as output sample file";
        return false;
    }

    return true;
}

// Close the output RF sample
void FileConverter::closeOutputSample()
{
    // Is a sample file open?
    if (outputSampleFileHandleTs != nullptr) {
        qDebug() << "FileConverter::closeOutputSample(): Closing output sample file";
        outputSampleFileHandleTs->close();
    }

    // Clear the file handle pointer
    outputSampleFileHandleTs = nullptr;
}

// Write the output sample data from a quint16 vector
bool FileConverter::writeOutputSample(QVector<quint16> sampleBuffer, bool isTenBit)
{
    // Only set this to true if you want huge amounts of debug from this method.
    bool fullDebug = false;

    if (isTenBit) {
        // Prepare the packed sample buffer (which stores the packed 10-bit data byte stream)
        QVector<quint8> packedSampleBuffer;
        packedSampleBuffer.resize(static_cast<qint32>(samplesToTenBitBytes(sampleBuffer.size())));
        qint32 packedSampleBufferPointer = 0;
        if (fullDebug) qDebug() << "FileConverter::writeOutputSample(): Writing " << sampleBuffer.size() <<
                    "samples to 10-bit output sample file as" << packedSampleBuffer.size() << "bytes";

        // Pack the data 4 samples at a time
        //
        // Unpacked:                 Packed:
        // 0: xxxx xx00 0000 0000    0: 0000 0000 0011 1111
        // 1: xxxx xx11 1111 1111    2: 1111 2222 2222 2233
        // 2: xxxx xx22 2222 2222    4: 3333 3333
        // 3: xxxx xx33 3333 3333

        quint16 word0, word1, word2, word3;
        for (qint32 samplePointer = 0; samplePointer < sampleBuffer.size(); samplePointer += 4) {

            word0 = sampleBuffer[samplePointer];
            word1 = sampleBuffer[samplePointer + 1];
            word2 = sampleBuffer[samplePointer + 2];
            word3 = sampleBuffer[samplePointer + 3];

            packedSampleBuffer[packedSampleBufferPointer + 0]  = static_cast<quint8>((word0 & 0x03FC) >> 2);
            packedSampleBuffer[packedSampleBufferPointer + 1]  = static_cast<quint8>(((word0 & 0x0003) << 6) + ((word1 & 0x03F0) >> 4));
            packedSampleBuffer[packedSampleBufferPointer + 2]  = static_cast<quint8>(((word1 & 0x000F) << 4) + ((word2 & 0x03C0) >> 6));
            packedSampleBuffer[packedSampleBufferPointer + 3]  = static_cast<quint8>(((word2 & 0x003F) << 2) + ((word3 & 0x0300) >> 8));
            packedSampleBuffer[packedSampleBufferPointer + 4]  = static_cast<quint8>(word3 & 0x00FF);

            // Increment the packed sample buffer pointer
            packedSampleBufferPointer += 5;
        }

        // Write the packed data to the output sample file
        qint64 writeResult = 0;
        writeResult = outputSampleFileHandleTs->write(reinterpret_cast<char *>(packedSampleBuffer.data()),
                                      packedSampleBuffer.size()
                                      );

        if (writeResult == -1) {
            qDebug() << "FileConverter::writeOutputSample(): Writing to 10-bit output sample file failed!";
            return false;
        }
    } else {
        // Write output sample as 16-bit scaled data
        QVector<qint16> scaledSampleData;
        scaledSampleData.resize(sampleBuffer.size());
        if (fullDebug) qDebug() << "FileConverter::writeOutputSample(): Writing " << sampleBuffer.size() <<
                    "samples to 16-bit output sample file as" << scaledSampleData.size() * 2 << "bytes";


        // Convert sample data
        for (qint32 samplePointer = 0; samplePointer < sampleBuffer.size(); samplePointer++) {
            // -512 from 10-bit data to move centre-point to 0 and then *64 to scale to 16-bit
            scaledSampleData[samplePointer] = static_cast<qint16>((sampleBuffer[samplePointer] - 512) * 64);
        }

        // Write the scaled data to the output sample file
        qint64 writeResult = 0;
        writeResult = outputSampleFileHandleTs->write(reinterpret_cast<char *>(scaledSampleData.data()),
                                      scaledSampleData.size() * static_cast<qint32>(sizeof(qint16))
                                      );

        if (writeResult == -1) {
            qDebug() << "FileConverter::writeOutputSample(): Writing to 16-bit output sample file failed!";
            return false;
        }
    }

    // Return successfully
    return true;
}

// This function takes a number of samples and returns the number
// of bytes required to store the same number of samples as 10-bit
// packed values
qint64 FileConverter::samplesToTenBitBytes(qint64 numberOfSamples)
{
    // Every 4 samples requires 5 bytes
    return (numberOfSamples / 4) * 5;
}
