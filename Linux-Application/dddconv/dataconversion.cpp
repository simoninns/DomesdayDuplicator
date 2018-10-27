#include "dataconversion.h"

DataConversion::DataConversion(QString inputFileNameParam, QString outputFileNameParam, bool isPackingParam, QObject *parent) : QObject(parent)
{
    // Store the configuration parameters
    inputFileName = inputFileNameParam;
    outputFileName = outputFileNameParam;
    isPacking = isPackingParam;
}

// Method to process the conversion of the file
bool DataConversion::process(void)
{
    // Open the input file
    if (!openInputFile()) {
        qCritical("Could not open input file!");
    }

    // Open the output file
    if (!openOutputFile()) {
        qCritical("Could not open output file!");
    }

    // Packing or unpacking?
    if (isPacking) packFile();
    else unpackFile();

    // Close the input file
    closeInputFile();

    // Close the output file
    closeOutputFile();

    return true;
}

// Method to open the input file for reading
bool DataConversion::openInputFile(void)
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
void DataConversion::closeInputFile(void)
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
bool DataConversion::openOutputFile(void)
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
void DataConversion::closeOutputFile(void)
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
void DataConversion::packFile(void)
{

}

// Method to unpack 10-bit data into 16-bit data
void DataConversion::unpackFile(void)
{

}
