/************************************************************************

    analysetestdata.h

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

#ifndef ANALYSETESTDATA_H
#define ANALYSETESTDATA_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QString>
#include <QTime>
#include <QFile>
#include <QDebug>

#include "inputsample.h"

class AnalyseTestData : public QThread
{
    Q_OBJECT

public:
    explicit AnalyseTestData(QObject *parent = nullptr);
    ~AnalyseTestData() override;

    void analyseInputFile(QString inputFilename,
                          QTime startTime, QTime endTime,
                          bool isInputTenBit);

    void cancelAnalysis();
    void quit();

signals:
    void percentageProcessed(qint32);
    void completed();
    void testFailed();

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
    bool isInputTenBit;
    QTime startTime;
    QTime endTime;

    // Thread-safe variables
    InputSample *inputSample;
    QString inputFilenameTs;
    bool isInputTenBitTs;
    QTime startTimeTs;
    QTime endTimeTs;
    qint64 numberOfSampleProcessedTs;

    qint64 startSampleTs;
    qint64 endSampleTs;
    qint64 samplesToAnalyseTs;

    quint16 currentValue;
    quint16 testDataMax;
    bool firstTest;
    bool testSuccessful;

    bool analyseSampleStart();
    bool analyseSampleProcess();
    void analyseSampleStop();

    bool analyseDataIntegrity(QVector<quint16> sample);
};

#endif // ANALYSETESTDATA_H
