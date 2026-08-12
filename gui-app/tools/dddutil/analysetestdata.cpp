/************************************************************************

    analysetestdata.cpp

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

#include "analysetestdata.h"

AnalyseTestData::AnalyseTestData(QObject *parent) : QThread(parent)
{
    // Thread control variables
    restart = false; // Setting this to true starts a conversion
    cancel = false; // Setting this to true cancels the conversion in progress
    abort = false; // Setting this to true ends the thread process
}

AnalyseTestData::~AnalyseTestData()
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
void AnalyseTestData::analyseInputFile(QString inputFilename,
                                       QTime startTime, QTime endTime,
                                       bool isInputTenBit)
{
    QMutexLocker locker(&mutex);

    // Move all the parameters to be local
    this->inputFilename = inputFilename;
    this->isInputTenBit = isInputTenBit;
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
void AnalyseTestData::run()
{
    qDebug() << "AnalyseTestData::run(): Thread running";

    while(!abort) {
        // Lock and copy all parameters to 'thread-safe' variables
        mutex.lock();
        inputFilenameTs = this->inputFilename;
        isInputTenBitTs = this->isInputTenBit;
        startTimeTs = this->startTime;
        endTimeTs = this->endTime;
        mutex.unlock();

        // Start the sample conversion
        if (analyseSampleStart()) {
            // Process the conversion until completed
            bool notComplete = true;
            qreal percentageCompleteReal = 0;
            qint32 percentageComplete = 0;

            while (notComplete && !cancel) {
                notComplete = analyseSampleProcess();

                // Calculate the completion percentage
                percentageCompleteReal = (100 / static_cast<qreal>(samplesToAnalyseTs)) * static_cast<qreal>(numberOfSampleProcessedTs);
                percentageComplete = static_cast<qint32>(percentageCompleteReal);
                qDebug() << "FileConverter::run(): Processed" << numberOfSampleProcessedTs << "of" <<
                            samplesToAnalyseTs << "(" << percentageComplete << "%)";

                // Emit a signal showing the progress
                emit percentageProcessed(percentageComplete);
            }

            // Stop the sample analysis
            analyseSampleStop();

            // Reset the cancel flag
            if (cancel) qDebug() << "AnalyseTestData::run(): Conversion cancelled";
            cancel = false;
        }

        // Emit a signal showing the analysis is complete if the test was successful
        // (otherwise the testFailed signal indicates completion)
        if (testSuccessful) emit completed();

        // Sleep the thread until we are restarted
        mutex.lock();
        if (!restart && !abort) condition.wait(&mutex);
        restart = false;
        mutex.unlock();
    }
    qDebug() << "AnalyseTestData::run(): Thread aborted";
}

// Function sets the cancel flag (which terminates the analysis if in progress)
void AnalyseTestData::cancelAnalysis()
{
    qDebug() << "AnalyseTestData::cancelConversion(): Setting cancel conversion flag";
    cancel = true;
}

// Function sets the abort flag (which terminates the run() loop if in progress)
void AnalyseTestData::quit()
{
    qDebug() << "AnalyseTestData::quit(): Setting thread abort flag";
    abort = true;
}

// File conversion methods --------------------------------------------------------------------------------------------

// Open the files and get ready to convert
bool AnalyseTestData::analyseSampleStart()
{
    if (isInputTenBitTs) qDebug() << "AnalyseTestData::analyseSampleStart(): Reading in 10-bit format";
    else qDebug() << "AnalyseTestData::analyseSampleStart(): Reading in 16-bit format";

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
    samplesToAnalyseTs = endSampleTs - startSampleTs;

    qDebug() << "AnalyseTestData::analyseSampleStart(): startSeconds =" << startSeconds <<
                "endSeconds =" << endSeconds << "total seconds =" << endSeconds - startSeconds;
    qDebug() << "AnalyseTestData::analyseSampleStart(): startSample =" << startSampleTs <<
                "endSample =" << endSampleTs << "samples to analyse =" << samplesToAnalyseTs;

    // Move the sample position to the start sample
    if (startSampleTs != 0) inputSample->seek(startSampleTs);

    // Reset the current expected value
    currentValue = 0;
    testDataMax = 0;
    firstTest = true;
    testSuccessful = true;

    // Return success
    return true;
}

// Process a buffer of sample data
bool AnalyseTestData::analyseSampleProcess()
{
    // Define a sample buffer for the transfer of data
    QVector<quint16> sampleBuffer;

    // Read the input sample data
    qint32 maximumBufferSize = 102400000;
    if ((numberOfSampleProcessedTs + maximumBufferSize) >= samplesToAnalyseTs)
        maximumBufferSize = static_cast<qint32>(samplesToAnalyseTs - numberOfSampleProcessedTs);

    sampleBuffer = inputSample->read(maximumBufferSize);
    numberOfSampleProcessedTs += sampleBuffer.size();

    // Did we get data?
    if (sampleBuffer.size() > 0) {
        // Test the data
        //qDebug() << "AnalyseTestData::analyseSampleProcess(): Checking data integrity...";
        if (!analyseDataIntegrity(sampleBuffer)) {
            // Test failed
            emit testFailed();
            testSuccessful = false;
            return false;
        }

    } else {
        // No sample data left, return false
        qDebug() << "AnalyseTestData::analyseSampleProcess(): No more data to analyse";
        return false;
    }

    // Have we finished processing all the samples?
    if (numberOfSampleProcessedTs >= samplesToAnalyseTs) {
        //qDebug() << "AnalyseTestData::analyseSampleProcess():" << numberOfSampleProcessedTs << "of"
        //         << samplesToAnalyseTs << "analysed. Done.";
        return false;
    }

    // Data still available, return true
    return true;
}

// Close the sample files and clean up
void AnalyseTestData::analyseSampleStop()
{
    // Destroy the input sample object
    inputSample->deleteLater();
}

// Analyse the test data for integrity
bool AnalyseTestData::analyseDataIntegrity(QVector<quint16> sample)
{
    bool result = true;
    qint32 startPointer = 0;

    // If this is the first test, get the start test value
    if (firstTest) {
        currentValue = sample[0];
        firstTest = false;
        startPointer++;
        //qDebug() << "AnalyseTestData::analyseDataIntegrity(): Initial value is" << currentValue;
    }

    // Test the data in the buffer
    for (qint32 pointer = startPointer; pointer < sample.size(); pointer++) {
        // Increment the current value and range check
        if (++currentValue == testDataMax) currentValue = 0;
        //qDebug() << "sample[" << pointer <<"] =" << sample[pointer] << " currentValue=" << currentValue;
        // Check if the data is as expected
        if (sample[pointer] != currentValue) {
            // If this is the first time the test sequence has wrapped around
            // to 0, detect the length of the sequence: either 1021 (newer FPGA
            // firmware) or 1024 (older FPGA firmware).
            if (testDataMax == 0 && sample[pointer] == 0 && (currentValue == 1021 || currentValue == 1024)) {
                testDataMax = currentValue;
                currentValue = 0;
                continue;
            }

            // Bad data
            qDebug() << "AnalyseTestData::analyseDataIntegrity(): Bad data! Expecting" << currentValue
                     << "got" << sample[pointer] << "instead at pointer" << pointer;
            result = false;
            break;
        }
    }

    return result;
}


