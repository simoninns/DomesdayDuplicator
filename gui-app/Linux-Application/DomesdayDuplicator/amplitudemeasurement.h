/************************************************************************

    amplitudemeasurement.h

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler
    Copyright (C) 2022 Matt Perry

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

#ifndef AMPLITUDEMEASUREMENT_H
#define AMPLITUDEMEASUREMENT_H

#include <QVector>
#include <array>
#include <vector>
#include "qcustomplot.h"

class AmplitudeMeasurement : public QCustomPlot
{
    Q_OBJECT

public:
    AmplitudeMeasurement(QWidget *parent = Q_NULLPTR);
    double getMeanAmplitude();

public slots:
    void updateBuffer();
    void plotGraph();

private:
    std::vector<qint16> inputSamples;
    QVector<double> graphXValues, graphYValues;
    std::array<double, 20> rollingAmp;
    QCPGraph *wavePlot;
};

#endif // AMPLITUDEMEASUREMENT_H
