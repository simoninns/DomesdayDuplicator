/************************************************************************

    rfsample.cpp

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


#include "rfsample.h"

RfSample::RfSample(QObject *parent) : QObject(parent)
{
    // Set default object values
    sizeOnDisc = 0;
    numberOfSamples = 0;
}

// Open the input RF sample
// Returns 'true' on success
bool RfSample::openInputSample(QString filename)
{
    // Open input sample file for reading
    inputSampleFileHandle = new QFile(filename);
    if (!inputSampleFileHandle->open(QIODevice::ReadOnly)) {
        // Failed to open input sample file
        qDebug() << "RfSample::openInputSample(): Could not open " << filename << "as input sample file";
        return false;
    }
    qDebug() << "RfSample::openInputSample(): Sample file is" << filename << "and is" << inputSampleFileHandle->size() << "bytes in length";

    // Determine the size on disc and number of samples (based on 10-bit packed data)
    sizeOnDisc = inputSampleFileHandle->size();
    numberOfSamples = (sizeOnDisc / 5) * 4;

    return true;
}

// Close the input RF sample
void RfSample::closeInputSample(void)
{
    // Is a sample file open?
    if (inputSampleFileHandle != nullptr) {
        qDebug() << "RfSample::closeInputSample(): Closing input sample file";
        inputSampleFileHandle->close();
    }

    // Clear the file handle pointer
    inputSampleFileHandle = nullptr;
}

// Open the output RF sample
// Returns 'true' on success
bool RfSample::openOutputSample(QString filename)
{
    // Open output sample file for writing
    outputSampleFileHandle = new QFile(filename);
    if (!outputSampleFileHandle->open(QIODevice::WriteOnly)) {
        // Failed to open output sample file
        qDebug() << "RfSample::openOutputSample(): Could not open " << filename << "as output sample file";
        return false;
    }
    qDebug() << "RfSample::openOutputSample(): Sample file is" << filename;

    return true;
}

// Close the output RF sample
void RfSample::closeOutputSample(void)
{
    // Is a sample file open?
    if (outputSampleFileHandle != nullptr) {
        qDebug() << "RfSample::closeOutputSample(): Closing output sample file";
        outputSampleFileHandle->close();
    }

    // Clear the file handle pointer
    outputSampleFileHandle = nullptr;
}

// Save a RF sample
bool RfSample::saveOutputSample(QString filename, QTime startTime, QTime endTime, bool isTenBit)
{
    // Note: TO-DO - Save according to start and end time parameters
    // Thread the save process with progess indication

    qDebug() << "RfSample::saveOutputSample(): Saving output file as " << filename;
    if (isTenBit) qDebug() << "RfSample::saveOutputSample(): Saving in 10-bit format";
    else qDebug() << "RfSample::saveOutputSample(): Saving in 16-bit format";

    // Open the output sample file
    if (!openOutputSample(filename)) {
        // Opening output sample failed!
        qDebug() << "RfSample::saveOutputSample(): Could not open output sample file!";
        return false;
    }

    // Define a sample buffer for the transfer of data
    QVector<quint16> sampleBuffer;

    qint64 remainingSamples = numberOfSamples;
    bool isDataAvailable = true;
    while (isDataAvailable) {
        // Read the input sample
        qDebug() << remainingSamples << "samples to be processed";
        sampleBuffer = readInputSample(10240000);
        remainingSamples -= sampleBuffer.size();

        // Did we get data?
        if (sampleBuffer.size() > 0) {
            // Write the input sample data to the output sample
            writeOutputSample(sampleBuffer, isTenBit);
        } else {
            // No sample data left
            isDataAvailable = false;
        }
    }

    // Close the output sample file
    closeOutputSample();

    return true;
}

// Read the input sample data and unpack into a quint16 vector
QVector<quint16> RfSample::readInputSample(qint32 maximumSamples)
{
    // Only set this to true if you want huge amounts of debug from this method.
    bool fullDebug = false;

    // Prepare the sample buffer (which stores the unpacked 10-bit data
    QVector<quint16> sampleBuffer;
    sampleBuffer.resize(maximumSamples);
    if (fullDebug) qDebug() << "RfSample::readInputSample(): Requesting" << maximumSamples << "samples from input file";

    // Prepare the packed sample buffer (which stores the packed 10-bit data byte stream)
    // There are 4 10-bit samples per 5 bytes of data (5 * 8 = 40 bits)
    QVector<quint8> packedSampleBuffer;
    packedSampleBuffer.resize(samplesToTenBitBytes(maximumSamples));
    if (fullDebug) qDebug() << "RfSample::readInputSample(): Requesting" << samplesToTenBitBytes(maximumSamples) << "bytes from input file";

    // Read the input sample file into the packed sample buffer
    qint64 totalReceivedBytes = 0;
    qint64 receivedBytes = 0;
    do {
        receivedBytes = inputSampleFileHandle->read(reinterpret_cast<char *>(packedSampleBuffer.data()),
                                                    samplesToTenBitBytes(maximumSamples));
        if (receivedBytes > 0) totalReceivedBytes += receivedBytes;
    } while (receivedBytes > 0 && totalReceivedBytes < samplesToTenBitBytes(maximumSamples));
    if (fullDebug) qDebug() << "RfSample::readInputSample(): Got a total of" << totalReceivedBytes << "bytes from input file";

    // Did we run out of sample data before filling the buffer?
    if (receivedBytes == 0) {
        // End of file was reached before filling buffer - adjust maximum
        if (fullDebug) qDebug() << "RfSample::readInputSample(): Reached end of file before filling buffer";

        if (totalReceivedBytes == 0) {
            // We didn't get any data at all...
            qDebug() << "RfSample::readInputSample(): Zero data received - nothing to do";
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
        qDebug() << "RfSample::readInputSample(): Unpacking sample data...";
        qDebug() << "RfSample::readInputSample(): PackedSampleBuffer size (qint8) =" << packedSampleBuffer.size();
        qDebug() << "RfSample::readInputSample(): sampleBuffer size (quint16) =" << sampleBuffer.size();
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

    return sampleBuffer;
}

// Write the output sample data from a quint16 vector
bool RfSample::writeOutputSample(QVector<quint16> sampleBuffer, bool isTenBit)
{
    // Only set this to true if you want huge amounts of debug from this method.
    bool fullDebug = false;

    if (isTenBit) {
        // Prepare the packed sample buffer (which stores the packed 10-bit data byte stream)
        QVector<quint8> packedSampleBuffer;
        packedSampleBuffer.resize(samplesToTenBitBytes(sampleBuffer.size()));
        qint32 packedSampleBufferPointer = 0;
        if (fullDebug) qDebug() << "RfSample::writeOutputSample(): Writing " << sampleBuffer.size() <<
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
        writeResult = outputSampleFileHandle->write(reinterpret_cast<char *>(packedSampleBuffer.data()),
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
        if (fullDebug) qDebug() << "RfSample::writeOutputSample(): Writing " << sampleBuffer.size() <<
                    "samples to 16-bit output sample file as" << scaledSampleData.size() * 2 << "bytes";


        // Convert sample data
        for (qint32 samplePointer = 0; samplePointer < sampleBuffer.size(); samplePointer++) {
            // -512 from 10-bit data to move centre-point to 0 and then *64 to scale to 16-bit
            scaledSampleData[samplePointer] = static_cast<qint16>(sampleBuffer[samplePointer] - 512) * 64;
        }

        // Write the scaled data to the output sample file
        qint64 writeResult = 0;
        writeResult = outputSampleFileHandle->write(reinterpret_cast<char *>(scaledSampleData.data()),
                                      scaledSampleData.size() * static_cast<qint32>(sizeof(qint16))
                                      );

        if (writeResult == -1) {
            qDebug() << "RfSample::writeOutputSample(): Writing to 16-bit output sample file failed!";
            return false;
        }
    }

    // Return successfully
    return true;
}

// This function takes a number of samples and returns the number
// of bytes required to store the same number of samples as 10-bit
// packed values
qint32 RfSample::samplesToTenBitBytes(qint32 numberOfSamples)
{
    // Every 4 samples requires 5 bytes
    return (numberOfSamples / 4) * 5;
}

// This function takes a number of bytes and returns the number
// of samples it represents (after 10-bit unpacking)
qint32 RfSample::tenBitBytesToSamples(qint32 numberOfBytes)
{
    // Every 5 bytes equals 4 samples
    return (numberOfBytes / 5) * 4;
}

// Get and set methods ------------------------------------------------------------------------------------------------

// Get the size of the sample file on disc and return as a
// readable string
QString RfSample::getSizeOnDisc(void)
{
    QString sizeText;

    if (sizeOnDisc < 102400) {
        sizeText = QString::number(sizeOnDisc) + " Bytes";
    } else if (sizeOnDisc < 1024 * 1024) {
        sizeText = QString::number(sizeOnDisc / 1024) + " KBytes";
    } else if (sizeOnDisc < 1024 * 1024 * 1024) {
        sizeText = QString::number(sizeOnDisc / 1024 / 1024) + " MBytes";
    } else {
        sizeText = QString::number(sizeOnDisc / 1024 / 1024 / 1024) + " GBytes";
    }

    return sizeText;
}

// Get the number of samples contained in the sample file
qint64 RfSample::getNumberOfSamples(void)
{
    return numberOfSamples;
}

// Get the approximate duration of the sample file and
// return it as the number of seconds
qint32 RfSample::getDurationSeconds(void)
{
    return static_cast<qint32>(numberOfSamples / 40000000);
}

// Get the approximate duration of the sample file and
// return it as a time string "hh:mm:ss"
QString RfSample::getDurationString(void)
{
    // Number of samples / 40 MSPS sampling rate
    qint64 duration = numberOfSamples / 40000000;

    // Return a QString in the format hh:mm:ss
    return QDateTime::fromTime_t(static_cast<quint32>(duration)).toUTC().toString("hh:mm:ss");
}
