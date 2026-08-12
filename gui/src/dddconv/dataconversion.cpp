#include "dataconversion.h"

#include "samplecodec.h"

DataConversion::DataConversion(QString inputFileNameParam, QString outputFileNameParam, bool isPackingParam, QObject *parent) : QObject(parent)
{
    // Store the configuration parameters
    inputFileName = inputFileNameParam;
    outputFileName = outputFileNameParam;
    isPacking = isPackingParam;
}

// Method to process the conversion of the file
bool DataConversion::process()
{
    // Open the input file
    if (!openInputFile()) {
        qCritical("Could not open input file!");
        return false;
    }

    // Open the output file
    if (!openOutputFile()) {
        qCritical("Could not open output file!");
        return false;
    }

    // Packing or unpacking?
    if (isPacking) packFile();
    else unpackFile();

    // Close the input file
    closeInputFile();

    // Close the output file
    closeOutputFile();

    // Exit with success
    return true;
}

// Method to open the input file for reading
bool DataConversion::openInputFile()
{
    // Do we have a file name for the input file?
    if (inputFileName.isEmpty()) {
        // No source input file name was specified, using stdin instead
        qDebug() << "No input filename was provided, using stdin";
        inputFileHandle = new QFile;
        if (!inputFileHandle->open(stdin, QIODevice::ReadOnly)) {
            // Failed to open stdin
            qWarning() << "Could not open stdin as input file";
            return false;
        }
        qDebug() << "Reading input data from stdin";
    } else {
        // Open input file for reading
        inputFileHandle = new QFile(inputFileName);
        if (!inputFileHandle->open(QIODevice::ReadOnly)) {
            // Failed to open source sample file
            qDebug() << "Could not open " << inputFileName << "as input file";
            return false;
        }
        qDebug() << "Input file is" << inputFileName << "and is" << inputFileHandle->size() << "bytes in length";
    }

    // Exit with success
    return true;
}

// Method to close the input file
void DataConversion::closeInputFile()
{
    // Is an input file open?
    if (inputFileHandle != nullptr) {
        inputFileHandle->close();
    }

    // Clear the file handle pointer
    delete inputFileHandle;
    inputFileHandle = nullptr;
}

// Method to open the output file for writing
bool DataConversion::openOutputFile()
{
    // Do we have a file name for the output file?
    if (outputFileName.isEmpty()) {
        // No output file name was specified, using stdin instead
        qDebug() << "No output filename was provided, using stdout";
        outputFileHandle = new QFile;
        if (!outputFileHandle->open(stdout, QIODevice::WriteOnly)) {
            // Failed to open stdout
            qWarning() << "Could not open stdout as output file";
            return false;
        }
        qDebug() << "Writing output data to stdout";
    } else {
        // Open the output file for writing
        outputFileHandle = new QFile(outputFileName);
        if (!outputFileHandle->open(QIODevice::WriteOnly)) {
            // Failed to open output file
            qDebug() << "Could not open " << outputFileName << "as output file";
            return false;
        }
        qDebug() << "Output file is" << outputFileName;
    }

    // Exit with success
    return true;
}

// Method to close the output file
void DataConversion::closeOutputFile()
{
    // Is an output file open?
    if (outputFileHandle != nullptr) {
        outputFileHandle->close();
    }

    // Clear the file handle pointer
    delete outputFileHandle;
    outputFileHandle = nullptr;
}

// Method to pack 16-bit data into 10-bit data
void DataConversion::packFile()
{
    qDebug() << "DataConversion::packFile(): Packing";
    QByteArray inputBuffer;
    QByteArray outputBuffer;
    bool isComplete = false;

    while(!isComplete) {
        // Input buffer must be divisible by 5 bytes due to 10-bit data format
        qint32 bufferSizeInBytes = (20 * 1024 * 1024); // = 20MiBytes
        inputBuffer.resize(bufferSizeInBytes);

        // Every 4 input words (8 bytes) is 5 output bytes
        outputBuffer.resize((bufferSizeInBytes / 8) * 5);

        // Fill the input buffer with data
        qint64 receivedBytes = 0;
        qint32 totalReceivedBytes = 0;
        do {
            receivedBytes = inputFileHandle->read(reinterpret_cast<char *>(inputBuffer.data() + totalReceivedBytes), inputBuffer.size() - totalReceivedBytes);
            if (receivedBytes > 0) totalReceivedBytes += receivedBytes;
        } while (receivedBytes > 0 && totalReceivedBytes < bufferSizeInBytes);

        // Check for end of file
        if (receivedBytes == 0) isComplete = true;

        if (totalReceivedBytes != 0) {
            // If we didn't fill the input buffer, resize it
            if (bufferSizeInBytes != totalReceivedBytes) {
                inputBuffer.resize(totalReceivedBytes);
                outputBuffer.resize((totalReceivedBytes / 8) * 5);
            }
            qDebug() << "DataConversion::packFile(): Got" << totalReceivedBytes << "bytes from input file";

            qint32 outputBufferPointer = 0;

            const qint16 *input = reinterpret_cast<const qint16 *>(inputBuffer.constData());
            quint8 *output = reinterpret_cast<quint8 *>(outputBuffer.data());

            for (qint32 wordPointer = 0; wordPointer < (totalReceivedBytes / 2); wordPointer += SampleCodec::samplesPerGroup) {
                // The codec itself is in samplecodec.h so it can be unit tested
                SampleCodec::packGroup(&input[wordPointer], &output[outputBufferPointer]);

                // Increment the packed sample buffer pointer
                outputBufferPointer += SampleCodec::bytesPerGroup;
            }

            // Write the output buffer to the output file
            if (!outputFileHandle->write(reinterpret_cast<char *>(outputBuffer.data()),
                                         outputBuffer.size())) {
                // File write failed
                qCritical("Could not write to output file!");
            }
            qDebug() << "DataConversion::packFile(): Wrote" << outputBuffer.size() << "bytes to output file";
        } else {
            // Input file is empty
            qDebug() << "DataConversion::packFile(): Got zero bytes from input file";
            isComplete = true;
        }
    }
}

// Method to unpack 10-bit data into 16-bit data
void DataConversion::unpackFile()
{
    qDebug() << "DataConversion::unpackFile(): Unpacking";
    QByteArray inputBuffer;
    QByteArray outputBuffer;
    bool isComplete = false;

    while(!isComplete) {
        // Input buffer must be divisible by 5 bytes due to 10-bit data format
        qint32 bufferSizeInBytes = (5 * 1024 * 1024) * 4; // 5MiB * 4 = 20MiBytes
        inputBuffer.resize(bufferSizeInBytes);

        // Every 5 input bytes is 4 output words (8 bytes)
        outputBuffer.resize((bufferSizeInBytes / 5) * 8);

        // Fill the input buffer with data
        qint64 receivedBytes = 0;
        qint32 totalReceivedBytes = 0;
        do {
            receivedBytes = inputFileHandle->read(reinterpret_cast<char *>(inputBuffer.data() + totalReceivedBytes), bufferSizeInBytes - totalReceivedBytes);
            if (receivedBytes > 0) totalReceivedBytes += receivedBytes;
        } while (receivedBytes > 0 && totalReceivedBytes < bufferSizeInBytes);

        // Check for end of file
        if (receivedBytes == 0) isComplete = true;

        if (totalReceivedBytes != 0) {
            // If we didn't fill the input buffer, resize it
            if (bufferSizeInBytes != totalReceivedBytes) {
                inputBuffer.resize(totalReceivedBytes);
                outputBuffer.resize((totalReceivedBytes / 5) * 8);
            }
            qDebug() << "DataConversion::unpackFile(): Got" << totalReceivedBytes << "bytes from input file";

            qint32 outputBufferPointer = 0;

            const quint8 *input = reinterpret_cast<const quint8 *>(inputBuffer.constData());
            qint16 *output = reinterpret_cast<qint16 *>(outputBuffer.data());

            for (qint32 bytePointer = 0; bytePointer < totalReceivedBytes; bytePointer += SampleCodec::bytesPerGroup) {
                // Unpack the 5 bytes into 4x 10-bit values.
                // The codec itself is in samplecodec.h so it can be unit tested.
                SampleCodec::unpackGroup(&input[bytePointer], &output[outputBufferPointer]);

                // Increment the sample buffer pointer
                outputBufferPointer += SampleCodec::samplesPerGroup;
            }

            // Write the output buffer to the output file
            if (!outputFileHandle->write(reinterpret_cast<char *>(outputBuffer.data()),
                                         outputBuffer.size())) {
                // File write failed
                qCritical("Could not write to output file!");
            }
            qDebug() << "DataConversion::unpackFile(): Wrote" << outputBuffer.size() << "bytes to output file";
        } else {
            // Input file is empty
            qDebug() << "DataConversion::unpackFile(): Got zero bytes from input file";
            isComplete = true;
        }
    }
}
