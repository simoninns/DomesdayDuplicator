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
#include <algorithm>
#include "usbcapture.h"

static constexpr qint32 GRAPH_POINTS = 1028;
static constexpr double MAX_SAMPLE = 32767.0;

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

    // Clear the amplitude history
    std::fill(rollingAmp.begin(), rollingAmp.end(), 0.0);
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
    // Add every 100th point to graphYValues and shift along
    qint32 numSamples = inputSamples.size();
    for (int i = 0; i < numSamples; i += 100) {
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
    qint32 numSamples = inputSamples.size();
    double posSum = 0.0;
    for (qint16 sample: inputSamples) {
        posSum += static_cast<double>(sample) * static_cast<double>(sample);
    }

    std::copy(rollingAmp.begin() + 1, rollingAmp.end(), rollingAmp.begin());
    // Divide by 2 here for a crest factor of sqrt(2) (i.e. assume a sin-ish signal)
    rollingAmp.back() = sqrt(posSum / (MAX_SAMPLE * MAX_SAMPLE * numSamples / 2.0));

    if (std::find(rollingAmp.begin(), rollingAmp.end(), 0.0) != rollingAmp.end()) {
        // rollingAmp (probably) isn't full yet -- return the latest value
        return rollingAmp.back();
    } else {
        double finalAmp = 0.0;
        for (double value: rollingAmp) finalAmp += value;
        return finalAmp / rollingAmp.size();
    }
}
