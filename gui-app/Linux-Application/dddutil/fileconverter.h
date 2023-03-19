/************************************************************************

    fileconverter.h

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

#ifndef FILECONVERTER_H
#define FILECONVERTER_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QTime>
#include <QFile>
#include <QDebug>

#include "inputsample.h"

class FileConverter : public QThread
{
    Q_OBJECT

public:
    explicit FileConverter(QObject *parent = nullptr);
    ~FileConverter() override;

    void convertInputFileToOutputFile(QString inputFilename, QString outputFilename,
                                      QTime startTime, QTime endTime,
                                      bool isInputTenBit, bool isOutputTenBit);

    void cancelConversion();
    void quit();

signals:
    void percentageProcessed(qint32);
    void completed();

protected:
    void run() override;

private:
    // Thread control
    QMutex mutex;
    QWaitCondition condition;
    bool restart;
    bool cancel;
    bool abort;

    // Externally settable variables
    QString inputFilename;
    QString outputFilename;
    bool isInputTenBit;
    bool isOutputTenBit;
    QTime startTime;
    QTime endTime;

    // Thread-safe variables
    InputSample *inputSample;
    QFile *outputSampleFileHandleTs;
    QString inputFilenameTs;
    QString outputFilenameTs;
    bool isInputTenBitTs;
    bool isOutputTenBitTs;
    QTime startTimeTs;
    QTime endTimeTs;
    qint64 numberOfSampleProcessedTs;

    qint64 startSampleTs;
    qint64 endSampleTs;
    qint64 samplesToConvertTs;

    bool convertSampleStart();
    bool convertSampleProcess();
    void convertSampleStop();

    bool writeOutputSample(QVector<quint16> sampleBuffer, bool isTenBit);
    bool openOutputSample(QString filename);
    void closeOutputSample();
    qint64 samplesToTenBitBytes(qint64 numberOfSamples);
};

#endif // FILECONVERTER_H
