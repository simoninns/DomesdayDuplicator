/************************************************************************

    fileconverter.cpp

    RF Sample analyser for Domesday Duplicator
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

#include "fileconverter.h"

FileConverter::FileConverter(QObject *parent) : QThread(parent)
{
    restart = false;
    abort = false;
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
        convertSampleStart();

        // Process the conversion until completed
        bool notComplete = true;
        while (notComplete) {
            notComplete = convertSampleProcess();
            qDebug() << "FileConverter::run(): Processed samples =" << numberOfSampleProcessedTs;
        }

        // Stop the sample conversion
        convertSampleStop();

        // Sleep the thread until we are restarted
        mutex.lock();
        if (!restart && !abort) condition.wait(&mutex);
        restart = false;
        mutex.unlock();
    }
    qDebug() << "FileConverter::run(): Thread aborted";
}

// Function sets the abort flag (which terminates the run() loop if in progress
void FileConverter::quit()
{
    qDebug() << "FileConverter::quit(): Setting thread abort flag";
    abort = true;
}

// File conversion methods --------------------------------------------------------------------------------------------

// Open the files and get ready to convert
bool FileConverter::convertSampleStart(void)
{
    qDebug() << "FileConverter::convertSampleStart(): Saving output file as " << outputFilenameTs;

    if (isInputTenBitTs) qDebug() << "FileConverter::convertSampleStart(): Reading in 10-bit format";
    else qDebug() << "FileConverter::convertSampleStart(): Reading in 16-bit format";

    if (isOutputTenBitTs) qDebug() << "FileConverter::convertSampleStart(): Writing in 10-bit format";
    else qDebug() << "FileConverter::convertSampleStart(): Writing in 16-bit format";

    // Open the input sample file
    if (!openInputSample(inputFilenameTs)) {
        // Opening input sample failed!
        qDebug() << "FileConverter::convertSampleStart(): Could not open input sample file!";
        return false;
    }

    // Open the output sample file
    if (!openOutputSample(outputFilenameTs)) {
        // Opening output sample failed!
        qDebug() << "FileConverter::convertSampleStart(): Could not open output sample file!";
        return false;
    }

    // Determine the size on disc and number of samples for the input sample
    sizeOnDiscTs = inputSampleFileHandleTs->size();
    if (isInputTenBit) numberOfSamplesInInputFileTs = (sizeOnDiscTs / 5) * 4;
    else numberOfSamplesInInputFileTs = sizeOnDiscTs / 2;

    // Reset the processed sample counter
    numberOfSampleProcessedTs = 0;

    // Return success
    return true;
}

// Process a buffer of sample data
bool FileConverter::convertSampleProcess(void)
{
    // Define a sample buffer for the transfer of data
    QVector<quint16> sampleBuffer;

    // Read the input sample
    sampleBuffer = readInputSample(10240000, isInputTenBitTs);
    numberOfSampleProcessedTs += sampleBuffer.size();
    qDebug() << "FileConverter::convertSampleProcess():" << numberOfSampleProcessedTs << "processed of" << numberOfSamplesInInputFileTs;

    // Did we get data?
    if (sampleBuffer.size() > 0) {
        // Write the input sample data to the output sample
        writeOutputSample(sampleBuffer, isOutputTenBitTs);
    } else {
        // No sample data left, return false
        return false;
    }

    // Data still available, return true
    return true;
}

// Close the sample files and clean up
void FileConverter::convertSampleStop(void)
{
    // Close the input sample file
    closeInputSample();

    // Close the output sample file
    closeOutputSample();
}

// Opens the input RF sample
// Returns 'true' on success
bool FileConverter::openInputSample(QString filename)
{
    // Open input sample file for reading
    inputSampleFileHandleTs = new QFile(filename);
    if (!inputSampleFileHandleTs->open(QIODevice::ReadOnly)) {
        // Failed to open input sample file
        qDebug() << "FileConverter::openinputSample(): Could not open " << filename << "as input sample file";
        return false;
    }

    return true;
}

// Close the input RF sample
void FileConverter::closeInputSample(void)
{
    // Is a sample file open?
    if (inputSampleFileHandleTs != nullptr) {
        qDebug() << "FileConverter::closeInputSample(): Closing input sample file";
        inputSampleFileHandleTs->close();
    }

    // Clear the file handle pointer
    inputSampleFileHandleTs = nullptr;
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
void FileConverter::closeOutputSample(void)
{
    // Is a sample file open?
    if (outputSampleFileHandleTs != nullptr) {
        qDebug() << "FileConverter::closeOutputSample(): Closing output sample file";
        outputSampleFileHandleTs->close();
    }

    // Clear the file handle pointer
    outputSampleFileHandleTs = nullptr;
}

// Read the input sample data and unpack into a quint16 vector
QVector<quint16> FileConverter::readInputSample(qint32 maximumSamples, bool isTenbit)
{
    // Only set this to true if you want huge amounts of debug from this method.
    bool fullDebug = false;

    // Prepare the sample buffer (which stores the unpacked 10-bit data)
    QVector<quint16> sampleBuffer;
    sampleBuffer.resize(maximumSamples);
    if (fullDebug) qDebug() << "FileConverter::readInputSample(): Requesting" << maximumSamples << "samples from input file";

    // Is the input data 10-bit or 16-bit?
    if (isTenbit) {
        // Prepare the packed sample buffer (which stores the packed 10-bit data byte stream)
        // There are 4 10-bit samples per 5 bytes of data (5 * 8 = 40 bits)
        QVector<quint8> packedSampleBuffer;
        packedSampleBuffer.resize(samplesToTenBitBytes(maximumSamples));
        if (fullDebug) qDebug() << "FileConverter::readInputSample(): Requesting" << samplesToTenBitBytes(maximumSamples) << "bytes from input file";

        // Read the input sample file into the packed sample buffer
        qint64 totalReceivedBytes = 0;
        qint64 receivedBytes = 0;
        do {
            receivedBytes = inputSampleFileHandleTs->read(reinterpret_cast<char *>(packedSampleBuffer.data()),
                                                        samplesToTenBitBytes(maximumSamples));
            if (receivedBytes > 0) totalReceivedBytes += receivedBytes;
        } while (receivedBytes > 0 && totalReceivedBytes < samplesToTenBitBytes(maximumSamples));
        if (fullDebug) qDebug() << "FileConverter::readInputSample(): Got a total of" << totalReceivedBytes << "bytes from input file";

        // Did we run out of sample data before filling the buffer?
        if (receivedBytes == 0) {
            // End of file was reached before filling buffer - adjust maximum
            if (fullDebug) qDebug() << "FileConverter::readInputSample(): Reached end of file before filling buffer";

            if (totalReceivedBytes == 0) {
                // We didn't get any data at all...
                qDebug() << "FileConverter::readInputSample(): Zero data received - nothing to do";
                sampleBuffer.resize(0);
                return sampleBuffer;
            }

            // Adjust buffers according to the received number of samples
            packedSampleBuffer.resize(static_cast<qint32>(totalReceivedBytes));
            sampleBuffer.resize(tenBitBytesToSamples(packedSampleBuffer.size()));
        }

        // Unpack the packed sample buffer into the sample buffer
        // Process the buffer 5 bytes at a time
        qint32 sampleBufferPointer = 0;

        if (fullDebug) {
            qDebug() << "FileConverter::readInputSample(): Unpacking 10-bit sample data...";
            qDebug() << "FileConverter::readInputSample(): PackedSampleBuffer size (qint8) =" << packedSampleBuffer.size();
            qDebug() << "FileConverter::readInputSample(): sampleBuffer size (quint16) =" << sampleBuffer.size();
        }
        for (qint32 bytePointer = 0; bytePointer < packedSampleBuffer.size(); bytePointer += 5) {
            // Unpack the 5 bytes into 4x 10-bit values (stored in 16-bit unsigned vector)

            // Unpacked:                 Packed:
            // 0: xxxx xx00 0000 0000    0: 0000 0000 0011 1111
            // 1: xxxx xx11 1111 1111    2: 1111 2222 2222 2233
            // 2: xxxx xx22 2222 2222    4: 3333 3333
            // 3: xxxx xx33 3333 3333

            sampleBuffer[sampleBufferPointer + 0]  = static_cast<quint16>(static_cast<quint16>(packedSampleBuffer[bytePointer + 0]) << 2);
            sampleBuffer[sampleBufferPointer + 0] += static_cast<quint16>(packedSampleBuffer[bytePointer + 1] & 0xC0) >> 6;

            sampleBuffer[sampleBufferPointer + 1]  = static_cast<quint16>(static_cast<quint16>(packedSampleBuffer[bytePointer + 1] & 0x3F) << 4);
            sampleBuffer[sampleBufferPointer + 1] += static_cast<quint16>(packedSampleBuffer[bytePointer + 2] & 0xF0) >> 4;

            sampleBuffer[sampleBufferPointer + 2]  = static_cast<quint16>(static_cast<quint16>(packedSampleBuffer[bytePointer + 2] & 0x0F) << 6);
            sampleBuffer[sampleBufferPointer + 2] += static_cast<quint16>(packedSampleBuffer[bytePointer + 3] & 0xFC) >> 2;

            sampleBuffer[sampleBufferPointer + 3]  = static_cast<quint16>(static_cast<quint16>(packedSampleBuffer[bytePointer + 3] & 0x03) << 8);
            sampleBuffer[sampleBufferPointer + 3] += static_cast<quint16>(packedSampleBuffer[bytePointer + 4]);

            // Increment the sample buffer pointer
            sampleBufferPointer += 4;
        }
    } else {
        // Prepare the input sample buffer (which stores the signed, scaled 16-bit word stream)
        QVector<qint16> signedSampleBuffer;
        signedSampleBuffer.resize(maximumSamples);
        if (fullDebug) qDebug() << "FileConverter::readInputSample(): Requesting" << samplesToSixteenBitBytes(maximumSamples) << "bytes from input file";

        // Read the input sample file into the signed sample buffer
        qint64 totalReceivedBytes = 0;
        qint64 receivedBytes = 0;
        do {
            receivedBytes = inputSampleFileHandleTs->read(reinterpret_cast<char *>(signedSampleBuffer.data()),
                                                        samplesToSixteenBitBytes(maximumSamples));
            if (receivedBytes > 0) totalReceivedBytes += receivedBytes;
        } while (receivedBytes > 0 && totalReceivedBytes < samplesToSixteenBitBytes(maximumSamples));
        if (fullDebug) qDebug() << "RfSample::readInputSample(): Got a total of" << totalReceivedBytes << "bytes from input file";

        // Did we run out of sample data before filling the buffer?
        if (receivedBytes == 0) {
            // End of file was reached before filling buffer - adjust maximum
            if (fullDebug) qDebug() << "FileConverter::readInputSample(): Reached end of file before filling buffer";

            if (totalReceivedBytes == 0) {
                // We didn't get any data at all...
                qDebug() << "FileConverter::readInputSample(): Zero data received - nothing to do";
                sampleBuffer.resize(0);
                return sampleBuffer;
            }

            // Adjust buffers according to the received number of samples
            signedSampleBuffer.resize(sixteenBitBytesToSamples(static_cast<qint32>(totalReceivedBytes)));
            sampleBuffer.resize(signedSampleBuffer.size());
        }

        // Convert the 16-bit signed samples into the sample buffer (into unsigned 10-bit values)
        if (fullDebug) {
            qDebug() << "FileConverter::readInputSample(): Converting 16-bit sample data...";
        }
        for (qint32 samplePointer = 0; samplePointer < signedSampleBuffer.size(); samplePointer++) {
            sampleBuffer[samplePointer] = (signedSampleBuffer[samplePointer] / 64) + 512;
        }
    }

    return sampleBuffer;
}

// Write the output sample data from a quint16 vector
bool FileConverter::writeOutputSample(QVector<quint16> sampleBuffer, bool isTenBit)
{
    // Only set this to true if you want huge amounts of debug from this method.
    bool fullDebug = false;

    if (isTenBit) {
        // Prepare the packed sample buffer (which stores the packed 10-bit data byte stream)
        QVector<quint8> packedSampleBuffer;
        packedSampleBuffer.resize(samplesToTenBitBytes(sampleBuffer.size()));
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

        for (qint32 samplePointer = 0; samplePointer < sampleBuffer.size(); samplePointer += 4) {
            packedSampleBuffer[packedSampleBufferPointer + 0]  = static_cast<quint8>((sampleBuffer[samplePointer + 0] & 0x03FC) >> 2);
            packedSampleBuffer[packedSampleBufferPointer + 1]  = static_cast<quint8>((sampleBuffer[samplePointer + 0] & 0x0003) << 6);
            packedSampleBuffer[packedSampleBufferPointer + 1] += static_cast<quint8>((sampleBuffer[samplePointer + 1] & 0x03F0) >> 4);
            packedSampleBuffer[packedSampleBufferPointer + 2]  = static_cast<quint8>((sampleBuffer[samplePointer + 1] & 0x000F) << 4);
            packedSampleBuffer[packedSampleBufferPointer + 2] += static_cast<quint8>((sampleBuffer[samplePointer + 2] & 0x03C0) >> 6);
            packedSampleBuffer[packedSampleBufferPointer + 3]  = static_cast<quint8>((sampleBuffer[samplePointer + 2] & 0x003F) << 2);
            packedSampleBuffer[packedSampleBufferPointer + 3] += static_cast<quint8>((sampleBuffer[samplePointer + 3] & 0x0300) >> 8);
            packedSampleBuffer[packedSampleBufferPointer + 4]  = static_cast<quint8>((sampleBuffer[samplePointer + 3] & 0x00FF));

            // Increment the packed sample buffer pointer
            packedSampleBufferPointer += 5;
        }

        // Write the packed data to the output sample file
        qint64 writeResult = 0;
        writeResult = outputSampleFileHandleTs->write(reinterpret_cast<char *>(packedSampleBuffer.data()),
                                      packedSampleBuffer.size()
                                      );

        if (writeResult == -1) {
            qDebug() << "RfSample::writeOutputSample(): Writing to 10-bit output sample file failed!";
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
            scaledSampleData[samplePointer] = static_cast<qint16>(sampleBuffer[samplePointer] - 512) * 64;
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

// Conversion methods to make the rest of the code a little more readable ---------------------------------------------

// This function takes a number of samples and returns the number
// of bytes required to store the same number of samples as 10-bit
// packed values
qint32 FileConverter::samplesToTenBitBytes(qint32 numberOfSamples)
{
    // Every 4 samples requires 5 bytes
    return (numberOfSamples / 4) * 5;
}

// This function takes a number of bytes and returns the number
// of samples it represents (after 10-bit unpacking)
qint32 FileConverter::tenBitBytesToSamples(qint32 numberOfBytes)
{
    // Every 5 bytes equals 4 samples
    return (numberOfBytes / 5) * 4;
}

// This function takes a number of samples and returns the number
// of bytes it represents (as 16-bit words)
qint32 FileConverter::samplesToSixteenBitBytes(qint32 numberOfSamples)
{
    // Every sample requires 2 bytes
    return numberOfSamples * 2;
}

// This function takes a number of bytes and returns the number
// of 16-bit samples it represents
qint32 FileConverter::sixteenBitBytesToSamples(qint32 numberOfBytes)
{
    // Every 2 bytes equals 1 sample
    return numberOfBytes / 2;
}
