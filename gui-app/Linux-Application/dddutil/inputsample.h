/************************************************************************

    inputsample.h

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

#ifndef INPUTSAMPLE_H
#define INPUTSAMPLE_H

#include <QObject>
#include <QFile>
#include <QDebug>
#include <QTime>

class InputSample : public QObject
{
    Q_OBJECT

public:
    explicit InputSample(QObject *parent = nullptr, QString fileName = nullptr, bool isTenBit = true);
    ~InputSample();

    QVector<quint16> read(qint32 maximumSamples);
    void seek(qint64 numberOfSamples);

    bool isInputSampleValid();
    qint64 getNumberOfSamples();

signals:

public slots:

private:
    QFile *sampleFileHandle;
    qint64 sizeOnDisc;
    qint64 numberOfSamples;
    bool sampleIsTenBit;
    bool sampleIsValid;

    bool open(QString filename);
    void close();

    qint64 samplesToTenBitBytes(qint64 numberOfSamples);
    qint64 tenBitBytesToSamples(qint64 numberOfBytes);
    qint64 samplesToSixteenBitBytes(qint64 numberOfSamples);
    qint64 sixteenBitBytesToSamples(qint64 numberOfBytes);
};

#endif // INPUTSAMPLE_H
