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
    // Define an rfSample object to handle the file conversion
    RfSample rfSample;

    while(!abort) {
        // Get all the required parameters for the conversion
        mutex.lock();
        QString inputFilename = this->inputFilename;
        QString outputFilename = this->outputFilename;
        bool isInputTenBit = this->isInputTenBit;
        bool isOutputTenBit = this->isOutputTenBit;
        QTime startTime = this->startTime;
        QTime endTime = this->endTime;
        mutex.unlock();

        // Process
        if (rfSample.getInputSampleDetails(inputFilename, isInputTenBit)) {
            qDebug() << "FileConverter::run(): Processing file conversion";
            rfSample.saveOutputSample(inputFilename, outputFilename, startTime, endTime, isOutputTenBit);
        } else qDebug() << "FileConverter::run(): Input file was invalid, cannot convert!";
        qDebug() << "FileConverter::run(): File conversion complete";

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
