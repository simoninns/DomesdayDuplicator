/************************************************************************

    amplitudemeasurement.cpp

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

#include "amplitudemeasurement.h"

#include <QList>
#include "usbcapture.h"

static constexpr qint32 GRAPH_POINTS = 1028;
static constexpr double MAX_SAMPLE = 32767.0;

// Fill array with zeroes and backfill as needed
QList<double> rollingAmp({0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0});

AmplitudeMeasurement::AmplitudeMeasurement(QWidget *parent)
    : QCustomPlot(parent)
{
    graphXValues.resize(GRAPH_POINTS);
    for (int i = 0; i < GRAPH_POINTS; i++) graphXValues[i] = i;
    graphYValues.fill(0, GRAPH_POINTS);

    // Initialize QCP graph
    wavePlot = addGraph();
    setMinimumHeight(50);
    xAxis->setTicks(false);
    setBackground(QColor(240, 240, 240, 255));
    yAxis->setRange(QCPRange(-1, 1));
    axisRect()->setAutoMargins(QCP::msNone);
    axisRect()->setMargins(QMargins(25, 7, 0, 6));
    QFont yfont;
    yfont.setPointSize(6);
    yAxis->setTickLabelFont(yfont);
}

// Get a buffer from UsbCapture, and update the statistics
void AmplitudeMeasurement::updateBuffer()
{
    // Get a recent disk buffer from UsbCapture
    const unsigned char *rawData;
    qint32 rawBytes;
    UsbCapture::getAmplitudeBuffer(&rawData, &rawBytes);

    // Convert to 16-bit samples in diskBuffer
    inputSamples.resize(rawBytes / 2);
    for (qint32 inPos = 0, outPos = 0; inPos < rawBytes; inPos += 2, outPos++) {
        // Get the original 10-bit unsigned value
        quint32 originalValue = rawData[inPos] + (rawData[inPos + 1] * 256);

        // Sign and scale the data to 16-bits
        inputSamples[outPos] = static_cast<qint16>(originalValue - 512) * 64;
    }
}

// Draw the graph
void AmplitudeMeasurement::plotGraph()
{
    // Add every millionth point to graphYValues and shift along
    qint32 numSamples = inputSamples.size();
    for (int i = 0; i < numSamples; i += 1000000) {
        graphYValues.append(inputSamples[i] / MAX_SAMPLE);
    }
    if (graphYValues.size() > GRAPH_POINTS) {
        graphYValues.remove(0, graphYValues.size() - GRAPH_POINTS);
    }

    if (wavePlot->dataCount() >= GRAPH_POINTS - 1) {
        wavePlot->data()->clear();
    }
    wavePlot->addData(graphXValues, graphYValues);
    xAxis->setRange(QCPRange(0, GRAPH_POINTS));
    replot();
}

// Calculate mean amplitude
double AmplitudeMeasurement::getMeanAmplitude()
{
    double posSum = 0;
    double avgVal = 0;
    double finalAmp = 0.0;

    qint32 numSamples = inputSamples.size();
    for (int i = 0; i < numSamples; i++){
        avgVal = inputSamples[i] / MAX_SAMPLE;
        posSum += avgVal * avgVal;
    }
    for (int i = 0; i < 19; i++) {
        rollingAmp.move(i + 1, i);
    }
    rollingAmp[19] = sqrt(posSum / (numSamples / 2));
    for (int ra = 0; ra < 19; ra++) {
        finalAmp += rollingAmp[ra];
    }
    if (rollingAmp.contains(0)) {
        return rollingAmp[19];
    } else {
        return finalAmp / 20;
    }
}
