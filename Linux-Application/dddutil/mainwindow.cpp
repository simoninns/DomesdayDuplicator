/************************************************************************

    mainwindow.cpp

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

#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    // Set up the UI
    ui->setupUi(this);

    // Create the UI dialogues
    aboutDialogue = new About(this);
    conversionProgressDialog = new ProgressDialog(this);
    analyseTestDataProgressDialog = new ProgressDialog(this);

    // Create the RF sample object
    sampleDetails = new SampleDetails();

    // Connect to the signals from the file converter thread
    connect(&fileConverter, &FileConverter::percentageProcessed, this, &MainWindow::conversionPercentageProcessedSignalHandler);
    connect(&fileConverter, &FileConverter::completed, this, &MainWindow::conversionCompletedSignalHandler);

    // Connect to the cancelled signal from the progress dialogue
    connect(conversionProgressDialog, &ProgressDialog::cancelled, this, &MainWindow::conversionCancelledSignalHandler);

    // Connect to the signals from the analyse test data thread
    connect(&analyseTestData, &AnalyseTestData::percentageProcessed, this, &MainWindow::analyseTestDataPercentageProcessedSignalHandler);
    connect(&analyseTestData, &AnalyseTestData::completed, this, &MainWindow::analyseTestDataCompletedSignalHandler);
    connect(&analyseTestData, &AnalyseTestData::testFailed, this, &MainWindow::analyseTestDataTestFailedSignalHandler);

    // Connect to the cancelled signal from the progress dialogue
    connect(analyseTestDataProgressDialog, &ProgressDialog::cancelled, this, &MainWindow::analyseTestDataCancelledSignalHandler);

    // Perform no file loaded actions
    noInputFileSpecified();
}

MainWindow::~MainWindow()
{
    // Delete the file converter thread
    fileConverter.deleteLater();

    // Delete the test data analysis thread
    analyseTestData.deleteLater();

    delete ui;
}

// GUI Update functions -----------------------------------------------------------------------------------------------

// Actions on file not loaded
void MainWindow::noInputFileSpecified()
{
    // Menu options
    ui->actionOpen_10_bit_File->setEnabled(true);
    ui->actionOpen_16_bit_File->setEnabled(true);
    ui->actionSave_As_10_bit->setEnabled(false);
    ui->actionSave_As_16_bit->setEnabled(false);
    ui->actionVerify_test_data->setEnabled(false);

    // Input file options
    ui->filenameLineEdit->setText(tr("No file loaded"));
    ui->numberOfSamplesLabel->setText(tr("0"));
    ui->sizeOnDiscLabel->setText(tr("0"));
    ui->durationLabel->setText(tr("00:00:00"));
    ui->dataFormatLabel->setText(tr("Unknown"));

    // Output file options
    ui->startTimeEdit->setEnabled(false);
    ui->endTimeEdit->setEnabled(false);
}

// Actions on file loaded
void MainWindow::inputFileSpecified()
{
    // Menu options
    ui->actionOpen_10_bit_File->setEnabled(true);
    ui->actionOpen_16_bit_File->setEnabled(true);
    ui->actionSave_As_10_bit->setEnabled(true);
    ui->actionSave_As_16_bit->setEnabled(true);
    ui->actionVerify_test_data->setEnabled(true);

    // Input file options
    ui->filenameLineEdit->setText(inputFilename);
    ui->numberOfSamplesLabel->setText(QString::number(sampleDetails->getNumberOfSamples()));
    ui->sizeOnDiscLabel->setText(sampleDetails->getSizeOnDisc());
    ui->durationLabel->setText(sampleDetails->getDurationString());
    if (sampleDetails->getInputFileFormat()) ui->dataFormatLabel->setText(tr("10-bit packed data sample"));
    else ui->dataFormatLabel->setText(tr("16-bit scaled data sample"));

    // Output file options
    ui->startTimeEdit->setEnabled(true);
    ui->endTimeEdit->setEnabled(true);

    // Set the initial time-span for the sample (ensure the duration is a
    // minimum of 1 second.
    ui->startTimeEdit->setTime(QTime(0,0));
    qint32 durationInSeconds = static_cast<qint32>(sampleDetails->getNumberOfSamples() / 40000000);
    if (durationInSeconds < 1) durationInSeconds = 1;
    ui->endTimeEdit->setTime(QTime(0,0).addSecs(durationInSeconds));
}

// Menu bar action functions ------------------------------------------------------------------------------------------

// Menu bar - Open 10-bit file triggered
void MainWindow::on_actionOpen_10_bit_File_triggered()
{
    inputFilename = QFileDialog::getOpenFileName(this,
            tr("Open 10-bit packed LaserDisc RF sample"),
            QDir::homePath()+tr("/ldsample.lds"),
            tr("LaserDisc Sample (*.lds);;All Files (*)"));

    // Was a filename specified?
    if (!inputFilename.isEmpty() && !inputFilename.isNull()) {
        // Get the 10-bit sample details
        if (sampleDetails->getInputSampleDetails(inputFilename, true)) {
            // Ensure that the input file is not empty
            if (sampleDetails->getNumberOfSamples() >= 40000000) {
                // Update the GUI (success)
                inputFileSpecified();
            } else {
                // Input file is too short!

                // Show an error
                QMessageBox messageBox;
                messageBox.critical(this, "Error","Input file must contain at least 1 second of data!");
                messageBox.setFixedSize(500, 200);

                noInputFileSpecified();
            }
        } else {
            // Show an error
            QMessageBox messageBox;
            messageBox.critical(this, "Error","Could not open input sample file!");
            messageBox.setFixedSize(500, 200);

            // Update the GUI (failure)
            noInputFileSpecified();
        }
    } else noInputFileSpecified();
}

// Menu bar - Open 16-bit file triggered
void MainWindow::on_actionOpen_16_bit_File_triggered()
{
    inputFilename = QFileDialog::getOpenFileName(this,
            tr("Open 16-bit signed raw data sample"),
            QDir::homePath()+tr("/ldsample.raw"),
            tr("Raw Data Sample (*.raw);;All Files (*)"));

    // Was a filename specified?
    if (!inputFilename.isEmpty() && !inputFilename.isNull()) {
        // Get the 16-bit sample details
        if (sampleDetails->getInputSampleDetails(inputFilename, false)) {
            // Ensure that the input file is not empty
            if (sampleDetails->getNumberOfSamples() >= 40000000) {
                // Update the GUI (success)
                inputFileSpecified();
            } else {
                // Input file is too short!

                // Show an error
                QMessageBox messageBox;
                messageBox.critical(this, "Error","Input file must contain at least 1 second of data!");
                messageBox.setFixedSize(500, 200);

                noInputFileSpecified();
            }
        } else {
            // Show an error
            QMessageBox messageBox;
            messageBox.critical(this, "Error","Could not open input sample file!");
            messageBox.setFixedSize(500, 200);

            // Update the GUI (failure)
            noInputFileSpecified();
        }
    } else noInputFileSpecified();
}

// Menu bar - Save As 10-bit file triggered
// TODO: check input and output files are not the same!
void MainWindow::on_actionSave_As_10_bit_triggered()
{
    outputFilename = QFileDialog::getSaveFileName(this,
            tr("Save 10-bit packed LaserDisc RF sample"),
            QDir::homePath()+tr("/ldsample_out.lds"),
            tr("LaserDisc Sample .lds (*.lds);;All Files (*)"));

    // Was a filename specified?
    if (!outputFilename.isEmpty() && !outputFilename.isNull()) {
        // Is the output filename different from the input filename?
        if (inputFilename != outputFilename) {
            // Save the file as 10-bit
            conversionProgressDialog->setPercentage(0);
            conversionProgressDialog->setText(tr("Saving sample as 10-bit packed data..."));
            fileConverter.convertInputFileToOutputFile(inputFilename, outputFilename,
                                                       ui->startTimeEdit->time(), ui->endTimeEdit->time(),
                                                       sampleDetails->getInputFileFormat(), true);
            conversionProgressDialog->exec(); // Exec causes the main window to be disabled
        } else {
            // Show an error
            QMessageBox messageBox;
            messageBox.critical(this, "Error","Input and output files cannot be the same!");
            messageBox.setFixedSize(500, 200);
        }
    }
}

// Menu bar - Save As 16-bit file triggered
// TODO: check input and output files are not the same!
void MainWindow::on_actionSave_As_16_bit_triggered()
{
    outputFilename = QFileDialog::getSaveFileName(this,
            tr("Save 16-bit signed raw data sample"),
            QDir::homePath()+tr("/ldsample_out.raw"),
            tr("Raw Data Sample (*.raw);;All Files (*)"));

    // Was a filename specified?
    if (!outputFilename.isEmpty() && !outputFilename.isNull()) {
        if (inputFilename != outputFilename) {
            // Attempt to save the file as 16-bit
            conversionProgressDialog->setPercentage(0);
            conversionProgressDialog->setText(tr("Saving sample as 16-bit signed data..."));
            fileConverter.convertInputFileToOutputFile(inputFilename, outputFilename,
                                                       ui->startTimeEdit->time(), ui->endTimeEdit->time(),
                                                       sampleDetails->getInputFileFormat(), false);
            conversionProgressDialog->exec(); // Exec causes the main window to be disabled
        } else {
            // Show an error
            QMessageBox messageBox;
            messageBox.critical(this, "Error","Input and output files cannot be the same!");
            messageBox.setFixedSize(500, 200);
        }
    }
}

// Menu bar - Quit triggered
void MainWindow::on_actionQuit_triggered()
{
    // Stop the file converter thread
    fileConverter.quit();

    // Quit the application
    qApp->quit();
}

// Menu bar - About triggered
void MainWindow::on_actionAbout_triggered()
{
    // Show the about dialogue
    aboutDialogue->exec();
}

// User changed the sample start time
void MainWindow::on_startTimeEdit_userTimeChanged(const QTime &startTime)
{
    // Get the start and end time in seconds
    qint32 startTimeSec = QTime(0, 0, 0).secsTo(startTime);
    qint32 endTimeSec= QTime(0, 0, 0).secsTo(ui->endTimeEdit->time());

    // Start time should be at least 1 second less than end time
    if (startTimeSec > endTimeSec - 1) {
        startTimeSec = endTimeSec - 1;
        ui->startTimeEdit->setTime(QTime(0, 0, 0).addSecs(startTimeSec));
    }
}

// User changed the sample end time
void MainWindow::on_endTimeEdit_userTimeChanged(const QTime &endTime)
{
    // Get the start and end time in seconds
    qint32 startTimeSec = QTime(0, 0, 0).secsTo(ui->startTimeEdit->time());
    qint32 endTimeSec= QTime(0, 0, 0).secsTo(endTime);

    // endTime should be equal or less than the sample duration
    if (endTimeSec > sampleDetails->getDurationSeconds()) {
        ui->endTimeEdit->setTime(QTime(0, 0, 0).addSecs(sampleDetails->getDurationSeconds()));
    }

    // End time should be at least 1 second greater than start time
    if (endTimeSec < startTimeSec + 1) {
        endTimeSec = startTimeSec + 1;
        ui->endTimeEdit->setTime(QTime(0, 0, 0).addSecs(endTimeSec));
    }
}

// Menu bar - Verify test data triggered
void MainWindow::on_actionVerify_test_data_triggered()
{
    // Verify the input sample test data
    analyseTestDataProgressDialog->setPercentage(0);
    analyseTestDataProgressDialog->setText(tr("Analysing input sample test data..."));
    analyseTestData.analyseInputFile(inputFilename, ui->startTimeEdit->time(), ui->endTimeEdit->time(),
                                     sampleDetails->getInputFileFormat());
    analyseTestDataProgressDialog->exec(); // Exec causes the main window to be disabled
}

// Signal handlers ----------------------------------------------------------------------------------------------------

// FileConverter class:

// Handle the percentage processed signal sent by the file converter thread
void MainWindow::conversionPercentageProcessedSignalHandler(qint32 percentage)
{
    // Update the process dialogue
    conversionProgressDialog->setPercentage(percentage);
}

// Handle the conversion completed signal sent by the file converter thread
void MainWindow::conversionCompletedSignalHandler()
{
    // Hide the process dialogue (re-enables main window)
    conversionProgressDialog->hide();
}

// Handle the progress dialogue cancelled signal sent by the progress dialogue
void MainWindow::conversionCancelledSignalHandler()
{
    // Cancel the conversion in progress
    fileConverter.cancelConversion();
}

// AnalyseTestData class:

// Handle the percentage processed signal sent by the test data analyser thread
void MainWindow::analyseTestDataPercentageProcessedSignalHandler(qint32 percentage)
{
    // Update the process dialogue
    analyseTestDataProgressDialog->setPercentage(percentage);
}

// Handle the conversion completed signal sent by the test data analyser thread
void MainWindow::analyseTestDataCompletedSignalHandler()
{
    // Hide the process dialogue (re-enables main window)
    analyseTestDataProgressDialog->hide();

    // Show a message
    QMessageBox messageBox;
    messageBox.information(this, "Success","Test data check successful!");
    messageBox.setFixedSize(500, 200);
}

// Handle the progress dialogue cancelled signal sent by the progress dialogue
void MainWindow::analyseTestDataCancelledSignalHandler()
{
    // Cancel the analysis in progress
    analyseTestData.cancelAnalysis();
}

// Handle the test failed signal from the test data analyser
void MainWindow::analyseTestDataTestFailedSignalHandler()
{
    // Hide the process dialogue (re-enables main window)
    analyseTestDataProgressDialog->hide();

    // Show an error
    QMessageBox messageBox;
    messageBox.critical(this, "Error","Test data failed integrity check!");
    messageBox.setFixedSize(500, 200);
}

