#ifndef DATACONVERSION_H
#define DATACONVERSION_H

#include <QObject>
#include <QDebug>
#include <QFile>

class DataConversion : public QObject
{
    Q_OBJECT
public:
    explicit DataConversion(QString inputFileNameParam, QString outputFileNameParam, bool isPackingParam, QObject *parent = nullptr);

    bool process();
signals:

public slots:

private:
    QString inputFileName;
    QString outputFileName;
    bool isPacking;

    QFile *inputFileHandle;
    QFile *outputFileHandle;

    // Private methods
    bool openInputFile();
    void closeInputFile();
    bool openOutputFile();
    void closeOutputFile();
    void packFile();
    void unpackFile();
};

#endif // DATACONVERSION_H
