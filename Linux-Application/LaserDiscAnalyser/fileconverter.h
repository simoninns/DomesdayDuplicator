/************************************************************************

    fileconverter.h

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

#include "rfsample.h"

class FileConverter : public QThread
{
    Q_OBJECT

public:
    explicit FileConverter(QObject *parent = nullptr);
    ~FileConverter() override;

    void convertInputFileToOutputFile(QString inputFilename, QString outputFilename,
                                      QTime startTime, QTime endTime,
                                      bool isInputTenBit, bool isOutputTenBit);

signals:

protected:
    void run() override;

private:
    QMutex mutex;
    QWaitCondition condition;
    bool restart;
    bool abort;

    QFile *inputSampleFileHandle;
    QFile *outputSampleFileHandle;

    QString inputFilename;
    QString outputFilename;
    bool isInputTenBit;
    bool isOutputTenBit;
    QTime startTime;
    QTime endTime;
};

#endif // FILECONVERTER_H
