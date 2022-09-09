#ifndef AMPLITUDEMEASUREMENT_H
#define AMPLITUDEMEASUREMENT_H
#include <QtCore/QIODevice>
#include <QtCore/QPointF>
#include <QtCore/QVector>
#include <QtCharts/QChartGlobal>
#include <QThread>
#include <QAudioBuffer>
#include <QAudioSource>
#include <QMediaDevices>
#include <QAudioBuffer>
#include "qcustomplot.h"

class AmplitudeMeasurement : public QCustomPlot
{
    Q_OBJECT

public:

   static const int availableSamples = 33554400;
   static const int sampleCount = 2000;
   qreal level() const { return level(); }
   AmplitudeMeasurement(QWidget *parent = Q_NULLPTR);
   static double getMeanAmplitude();
   static QByteArray audioBuffer();

public slots:
   void setBuffer();
   void plot();

private:
   qreal getPeakValue(const QAudioFormat& format);
   QVector<double> samples;
   QCPGraph *wavePlot;
};



#endif // AMPLITUDEMEASUREMENT_H
