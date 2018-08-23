/************************************************************************

    rfsample.h

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

#ifndef RFSAMPLE_H
#define RFSAMPLE_H

#include <QObject>
#include <QFile>
#include <QDebug>
#include <QDateTime>

class RfSample : public QObject
{
    Q_OBJECT
public:
    explicit RfSample(QObject *parent = nullptr);
    bool getInputSampleDetails(QString inputFilename, bool isTenBit);
    bool saveOutputSample(QString inputFilename, QString outputFilename, QTime startTime, QTime endTime, bool isOutputTenBit);

    QString getSizeOnDisc(void);
    qint64 getNumberOfSamples(void);
    qint32 getDurationSeconds(void);
    QString getDurationString(void);
    bool getInputFileFormat(void);

signals:

public slots:

private:
    qint64 sizeOnDisc;
    qint64 numberOfSamples;
    QFile *inputSampleFileHandle;
    QFile *outputSampleFileHandle;
    bool isInputFileTenBit;

    bool openInputSample(QString filename);
    void closeInputSample(void);
    bool openOutputSample(QString filename);
    void closeOutputSample(void);

    QVector<quint16> readInputSample(qint32 maximumSamples);
    bool writeOutputSample(QVector<quint16> sampleBuffer, bool isTenBit);

    qint32 samplesToTenBitBytes(qint32 numberOfSamples);
    qint32 tenBitBytesToSamples(qint32 numberOfBytes);
    qint32 samplesToSixteenBitBytes(qint32 numberOfSamples);
    qint32 sixteenBitBytesToSamples(qint32 numberOfBytes);
};

#endif // RFSAMPLE_H
