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

#include <QtWidgets>
#include <QtMultimedia>
#include <QAudioBuffer>
#include "usbcapture.h"
#include "amplitudemeasurement.h"
#include <QList>
#include <QAudioDevice>
#include <QAudioSource>

// Note that in signed 16 PCM, one disk buffer (64MB) is 838860 milliseconds

QList<int> m_buffer;
QVector<double> sample;
static int plotpointcount = 0;
int signalPeaks = 0;
qreal peak = 32767;
// Fill array with zeroes and backfill as needed
QList<double> rollingAmp({0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});

// Initialize QCP graph
AmplitudeMeasurement::AmplitudeMeasurement(QWidget *parent)
    : QCustomPlot(parent)
{
    samples.fill(0, 1028);
    wavePlot = addGraph();
    setMinimumHeight(50);
    xAxis->setTicks(false);
    setBackground(QColor(240, 240, 240, 255));
    yAxis->setRange(QCPRange(-1, 1));
    axisRect()->setAutoMargins(QCP::msNone);
    axisRect()->setMargins(QMargins(25,7,0,6));
    QFont yfont;
    yfont.setPointSize(6);
    yAxis->setTickLabelFont(yfont);
}

// Process graph
void AmplitudeMeasurement::plot()
{
    QVector<double> x(samples.size());
    for (int i=0; i<x.size(); i++)
        x[i] = i;
    if (wavePlot->dataCount() >= 1027)
        wavePlot->data()->clear();
    wavePlot->addData(x, samples);
    xAxis->setRange(QCPRange(0, samples.size()));
    replot();
}

//Fill the buffer with data
void AmplitudeMeasurement::setBuffer()
{

    QAudioFormat format;
    format.setSampleRate(40000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    QByteArray sourceAudio = UsbCapture::getBuffer();
    QAudioBuffer ampliBuffer(sourceAudio, format, -1);

    const qint16 *data = ampliBuffer.constData<qint16>();
    int count = ampliBuffer.sampleCount();

    for (int i=0; i<count; i++){
        double val = data[i]/peak;
        i=i+1000000;
        plotpointcount++;
        if (plotpointcount >= 1028) {
            samples.removeFirst();
        }
        samples.append(val);
    }
}

//Calculate mean amplitude (text label)
double AmplitudeMeasurement::getMeanAmplitude()
{
    QAudioFormat format;
    format.setSampleRate(40000);
    format.setChannelCount(1);
    format.setSampleFormat(QAudioFormat::Int16);
    QByteArray sourceAudio = UsbCapture::getBuffer();
    QAudioBuffer ampliBuffer(sourceAudio, format, -1);

    double posSum = 0;
    double avgVal = 0;
    double finalAmp = 0.0;

    const qint16 *ampliData = ampliBuffer.constData<qint16>();
    double ampliCount = ampliBuffer.sampleCount();

    for (int i=0; i<ampliCount; i++){
        const double& data = ampliData[i];
        avgVal = data/peak;
        posSum += avgVal * avgVal;
    }
    for (int i=0; i<19; i++) {
        rollingAmp.move(i+1, i);
    }
    rollingAmp[19] = sqrt(posSum / (ampliCount/2));
    for (int ra=0; ra<19; ra++) {
        finalAmp += rollingAmp[ra];
    }
    if (rollingAmp.contains(0)) {
        return rollingAmp[19];
    } else {
    return ((finalAmp)/20);
    }
}
