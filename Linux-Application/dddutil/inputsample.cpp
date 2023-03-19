/************************************************************************

    inputsample.cpp

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

#include "inputsample.h"

InputSample::InputSample(QObject *parent, QString fileName, bool isTenBit) : QObject(parent)
{
    // Set object as invalid
    sampleIsValid = false;

    // Open the file
    if (open(fileName)) {
        // Input sample is valid
        sampleIsValid = true;
    } else return; // File not valid, just return

    // Set object values
    sampleIsTenBit = isTenBit;

    // Calculate the number of samples based on size and format
    if (sampleIsTenBit) numberOfSamples = tenBitBytesToSamples(sizeOnDisc);
    else numberOfSamples = sixteenBitBytesToSamples(sizeOnDisc);
}

InputSample::~InputSample()
{
    // Close the input sample and mark the object invalid
    close();
    sampleIsValid = false;
}

// Opens the input sample
// Returns 'true' on success
bool InputSample::open(QString filename)
{
    // Open input sample file for reading
    sampleFileHandle = new QFile(filename);
    if (!sampleFileHandle->open(QIODevice::ReadOnly)) {
        // Failed to open input sample file
        qDebug() << "InputSample::open(): Could not open " << filename << "as input sample file";
        return false;
    }

    // Get the size on disc in bytes
    sizeOnDisc = sampleFileHandle->size();

    // Successfully opened
    return true;
}

// Close the input sample
void InputSample::close()
{
    // Is a sample file open?
    if (sampleFileHandle != nullptr) {
        qDebug() << "InputSample::close(): Closing input sample file";
        sampleFileHandle->close();
    }

    // Clear the file handle pointer
    sampleFileHandle = nullptr;
}

// Read the input sample data and unpack into a quint16 vector
QVector<quint16> InputSample::read(qint32 maximumSamples)
{
    if (!sampleIsValid) {
        // There is no valid input sample
        qDebug() << "InputSample::read(): Called, but there is no valid input sample file!";
    }

    // Only set this to true if you want huge amounts of debug from this method.
    bool fullDebug = false;

    // Prepare the sample buffer (which stores the unpacked 10-bit data)
    QVector<quint16> sampleBuffer;
    sampleBuffer.resize(maximumSamples);

    if (fullDebug) qDebug() << "InputSample::read(): Requesting" <<
                               maximumSamples << "samples from input file";

    // Is the input data 10-bit or 16-bit?
    if (sampleIsTenBit) {
        // Prepare the packed sample buffer (which stores the packed 10-bit data byte stream)
        // There are 4 10-bit samples per 5 bytes of data (5 * 8 = 40 bits)
        QVector<quint8> packedSampleBuffer;
        packedSampleBuffer.resize(static_cast<qint32>(samplesToTenBitBytes(maximumSamples)));
        if (fullDebug) qDebug() << "InputSample::read(): Requesting" << samplesToTenBitBytes(maximumSamples) << "bytes from input file";

        // Read the input sample file into the packed sample buffer
        qint64 totalReceivedBytes = 0;
        qint64 receivedBytes = 0;
        do {
            receivedBytes = sampleFileHandle->read(reinterpret_cast<char *>(packedSampleBuffer.data()),
                                                        samplesToTenBitBytes(maximumSamples));
            if (receivedBytes > 0) totalReceivedBytes += receivedBytes;
            if (fullDebug) qDebug() << "InputSample::read(): Got" << receivedBytes << "bytes from input file";
        } while (receivedBytes > 0 && totalReceivedBytes < samplesToTenBitBytes(maximumSamples));
        if (fullDebug) qDebug() << "InputSample::read(): Got a total of" << totalReceivedBytes << "bytes from input file";

        // Did we run out of sample data before filling the buffer?
        if (receivedBytes == 0) {
            // End of file was reached before filling buffer - adjust maximum
            if (fullDebug) qDebug() << "InputSample::read(): Reached end of file before filling buffer";

            if (totalReceivedBytes == 0) {
                // We didn't get any data at all...
                qDebug() << "InputSample::read(): Zero data received - nothing to do";
                sampleBuffer.resize(0);
                return sampleBuffer;
            }

            // Adjust buffers according to the received number of samples
            packedSampleBuffer.resize(static_cast<qint32>(totalReceivedBytes));
            sampleBuffer.resize(static_cast<qint32>(tenBitBytesToSamples(packedSampleBuffer.size())));
        }

        // Unpack the packed sample buffer into the sample buffer
        // Process the buffer 5 bytes at a time
        qint32 sampleBufferPointer = 0;

        if (fullDebug) {
            qDebug() << "InputSample::read():Unpacking 10-bit sample data...";
            qDebug() << "InputSample::read(): PackedSampleBuffer size (qint8) =" << packedSampleBuffer.size();
            qDebug() << "InputSample::read(): sampleBuffer size (quint16) =" << sampleBuffer.size();
        }

        quint8 byte1, byte2, byte3;
        for (qint32 bytePointer = 0; bytePointer < packedSampleBuffer.size(); bytePointer += 5) {
            // Unpack the 5 bytes into 4x 10-bit values (stored in 16-bit unsigned vector)

            // Unpacked:                 Packed:
            // 0: xxxx xx00 0000 0000    0: 0000 0000 0011 1111
            // 1: xxxx xx11 1111 1111    2: 1111 2222 2222 2233
            // 2: xxxx xx22 2222 2222    4: 3333 3333
            // 3: xxxx xx33 3333 3333

            // We copy these bytes to cut down on the slow vector accesses
            // only for the bytes that are accessed more than once
            byte1 = (packedSampleBuffer[bytePointer + 1]);
            byte2 = (packedSampleBuffer[bytePointer + 2]);
            byte3 = (packedSampleBuffer[bytePointer + 3]);

            // Use multiplication instead of left-shift to avoid implicit conversion issues
            sampleBuffer[sampleBufferPointer]  = (packedSampleBuffer[bytePointer] * 4) + ((byte1 & 0xC0) >> 6);
            sampleBuffer[sampleBufferPointer + 1]  = ((byte1 & 0x3F) * 16) + ((byte2 & 0xF0) >> 4);
            sampleBuffer[sampleBufferPointer + 2]  = ((byte2 & 0x0F) * 64) + ((byte3 & 0xFC) >> 2);
            sampleBuffer[sampleBufferPointer + 3]  = ((byte3 & 0x03) * 256) + packedSampleBuffer[bytePointer + 4];

            // Increment the sample buffer pointer
            sampleBufferPointer += 4;
        }

    } else {
        // Prepare the sample buffer (which stores the signed, scaled 16-bit word stream)
        QVector<qint16> signedSampleBuffer;
        signedSampleBuffer.resize(maximumSamples);
        if (fullDebug) qDebug() << "InputSample::read(): Requesting" << samplesToSixteenBitBytes(maximumSamples) << "bytes from input file";

        // Read the input sample file into the signed sample buffer
        qint64 totalReceivedBytes = 0;
        qint64 receivedBytes = 0;
        do {
            receivedBytes = sampleFileHandle->read(reinterpret_cast<char *>(signedSampleBuffer.data()),
                                                        samplesToSixteenBitBytes(maximumSamples));
            if (receivedBytes > 0) totalReceivedBytes += receivedBytes;
        } while (receivedBytes > 0 && totalReceivedBytes < samplesToSixteenBitBytes(maximumSamples));
        if (fullDebug) qDebug() << "InputSample::read(): Got a total of" << totalReceivedBytes << "bytes from input file";

        // Did we run out of sample data before filling the buffer?
        if (receivedBytes == 0) {
            // End of file was reached before filling buffer - adjust maximum
            if (fullDebug) qDebug() << "InputSample::read(): Reached end of file before filling buffer";

            if (totalReceivedBytes == 0) {
                // We didn't get any data at all...
                qDebug() << "InputSample::read(): Zero data received - nothing to do";
                sampleBuffer.resize(0);
                return sampleBuffer;
            }

            // Adjust buffers according to the received number of samples
            signedSampleBuffer.resize(static_cast<qint32>(sixteenBitBytesToSamples(static_cast<qint32>(totalReceivedBytes))));
            sampleBuffer.resize(signedSampleBuffer.size());
        }

        // Convert the 16-bit signed samples into the sample buffer (into unsigned 10-bit values)
        if (fullDebug) {
            qDebug() << "InputSample::read(): Converting 16-bit sample data...";
        }
        for (qint32 samplePointer = 0; samplePointer < signedSampleBuffer.size(); samplePointer++) {
            sampleBuffer[samplePointer] = (static_cast<quint16>(signedSampleBuffer[samplePointer] >> 6) + 512);
        }
    }

    return sampleBuffer;
}

// Seek to a sample position in the input file
void InputSample::seek(qint64 numberOfSamples)
{
    if (!sampleIsValid) {
        // There is no valid input sample
        qDebug() << "InputSample::seek(): Called, but there is no valid input sample file!";
    }

    // Seek forwards a number of samples based on the sample format
    if (sampleIsTenBit) sampleFileHandle->seek(samplesToTenBitBytes(numberOfSamples));
    else sampleFileHandle->seek(samplesToSixteenBitBytes(numberOfSamples));
}

// Get and set methods ------------------------------------------------------------------------------------------------

// Determine if input sample is valid
bool InputSample::isInputSampleValid()
{
    return sampleIsValid;
}

// Get the number of samples in the input sample
qint64 InputSample::getNumberOfSamples()
{
    return numberOfSamples;
}

// Conversion methods to make the rest of the code a little more readable ---------------------------------------------

// This function takes a number of samples and returns the number
// of bytes required to store the same number of samples as 10-bit
// packed values
qint64 InputSample::samplesToTenBitBytes(qint64 numberOfSamples)
{
    // Every 4 samples requires 5 bytes
    return (numberOfSamples / 4) * 5;
}

// This function takes a number of bytes and returns the number
// of samples it represents (after 10-bit unpacking)
qint64 InputSample::tenBitBytesToSamples(qint64 numberOfBytes)
{
    // Every 5 bytes equals 4 samples
    return (numberOfBytes / 5) * 4;
}

// This function takes a number of samples and returns the number
// of bytes it represents (as 16-bit words)
qint64 InputSample::samplesToSixteenBitBytes(qint64 numberOfSamples)
{
    // Every sample requires 2 bytes
    return numberOfSamples * 2;
}

// This function takes a number of bytes and returns the number
// of 16-bit samples it represents
qint64 InputSample::sixteenBitBytesToSamples(qint64 numberOfBytes)
{
    // Every 2 bytes equals 1 sample
    return numberOfBytes / 2;
}
