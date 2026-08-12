/************************************************************************

    mainwindow.h

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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QThread>
#include <QMessageBox>

#include "about.h"
#include "sampledetails.h"
#include "fileconverter.h"
#include "analysetestdata.h"
#include "progressdialog.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionOpen_10_bit_File_triggered();
    void on_actionOpen_16_bit_File_triggered();
    void on_actionSave_As_10_bit_triggered();
    void on_actionSave_As_16_bit_triggered();
    void on_actionQuit_triggered();
    void on_actionAbout_triggered();
    void on_actionVerify_test_data_triggered();

    void on_startTimeEdit_userTimeChanged(const QTime &startTime);
    void on_endTimeEdit_userTimeChanged(const QTime &endTime);

    void conversionPercentageProcessedSignalHandler(qint32 percentage);
    void conversionCompletedSignalHandler();
    void conversionCancelledSignalHandler();

    void analyseTestDataPercentageProcessedSignalHandler(qint32 percentage);
    void analyseTestDataCompletedSignalHandler();
    void analyseTestDataCancelledSignalHandler();
    void analyseTestDataTestFailedSignalHandler();

private:
    Ui::MainWindow *ui;

    void noInputFileSpecified();
    void inputFileSpecified();

    QString inputFilename;
    QString outputFilename;
    About *aboutDialogue;
    ProgressDialog *conversionProgressDialog;
    ProgressDialog *analyseTestDataProgressDialog;
    FileConverter fileConverter;
    AnalyseTestData analyseTestData;
    SampleDetails *sampleDetails;
};

#endif // MAINWINDOW_H
