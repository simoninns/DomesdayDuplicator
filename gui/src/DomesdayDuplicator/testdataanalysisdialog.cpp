/************************************************************************

    testdataanalysisdialog.cpp

    Capture application for the Domesday Duplicator
    DomesdayDuplicator - LaserDisc RF sampler

    This file is part of the Domesday Duplicator.
    SPDX-FileCopyrightText: 2026 Simon Inns
    SPDX-License-Identifier: GPL-3.0-or-later

************************************************************************/

#include "testdataanalysisdialog.h"
#include "ui_testdataanalysisdialog.h"

#include "capturereader.h"
#include "captureformat.h"

#include <QFileDialog>
#include <QLocale>
#include <QTextStream>

#include <filesystem>

namespace
{
// Samples per read. 4 MSamples is a fraction of a second of capture, which keeps the
// progress bar moving without making the read call itself the bottleneck.
constexpr size_t analysisChunkSamples = 4 * 1024 * 1024;

QString describeResult(const TestDataAnalyser::Result &result, const QString &fileName, bool complete)
{
    QLocale locale;

    if (!result.passed)
    {
        return QObject::tr("FAILED: %1 — the test sequence breaks at sample %2, where %3 was expected but %4 was read.")
            .arg(fileName)
            .arg(locale.toString(static_cast<qulonglong>(result.samplesChecked)))
            .arg(result.expectedValue)
            .arg(result.actualValue);
    }

    if (!complete)
    {
        return QObject::tr("Cancelled after %1 samples, with no break found so far.")
            .arg(locale.toString(static_cast<qulonglong>(result.samplesChecked)));
    }

    if (result.sequenceLength == 0)
    {
        // Worth saying rather than reporting a bare pass: a capture too short to wrap has
        // not exercised the thing this test exists to find.
        return QObject::tr("PASSED: %1 — %2 samples with no break, but the capture is too short for the test "
                           "sequence to have wrapped, so this is weak evidence.")
            .arg(fileName)
            .arg(locale.toString(static_cast<qulonglong>(result.samplesChecked)));
    }

    return QObject::tr("PASSED: %1 — %2 samples checked, no breaks, test sequence length %3.")
        .arg(fileName)
        .arg(locale.toString(static_cast<qulonglong>(result.samplesChecked)))
        .arg(result.sequenceLength);
}
} // namespace

//----------------------------------------------------------------------------------------------------------------------
TestDataAnalysisWorker::TestDataAnalysisWorker(QString filePath, QObject *parent)
    : QThread(parent), filePath(std::move(filePath))
{
}

//----------------------------------------------------------------------------------------------------------------------
void TestDataAnalysisWorker::requestCancel()
{
    cancelRequested = true;
}

//----------------------------------------------------------------------------------------------------------------------
void TestDataAnalysisWorker::run()
{
    const std::filesystem::path path = filePath.toStdString();

    const auto format = CaptureReader::FormatFromExtension(path);
    if (!format.has_value())
    {
        emit finished(false, tr("%1 is not a capture file this application can read. Expected .ldf, .lds or .raw.")
                                 .arg(QString::fromStdString(path.filename().string())));
        return;
    }

    CaptureReader reader;
    std::string errorMessage;
    if (!reader.Open(path, *format, errorMessage))
    {
        emit finished(false, tr("Could not open %1: %2")
                                 .arg(QString::fromStdString(path.filename().string()))
                                 .arg(QString::fromStdString(errorMessage)));
        return;
    }

    const auto totalSamples = reader.GetTotalSamples();

    TestDataAnalyser analyser;
    std::vector<uint16_t> samples;
    bool endOfFile = false;
    int lastPercentage = -1;

    while (!endOfFile && !cancelRequested)
    {
        if (!reader.Read(samples, analysisChunkSamples, endOfFile))
        {
            emit finished(false, tr("Failed to read %1: %2")
                                     .arg(QString::fromStdString(path.filename().string()))
                                     .arg(QString::fromStdString(reader.GetLastError())));
            return;
        }

        if (samples.empty())
        {
            break;
        }

        const bool stillGood = analyser.Feed(samples.data(), samples.size());

        const qint64 checked = static_cast<qint64>(analyser.GetResult().samplesChecked);
        if (totalSamples.has_value() && *totalSamples > 0)
        {
            const int percentage = static_cast<int>((checked * 100) / static_cast<qint64>(*totalSamples));
            if (percentage != lastPercentage)
            {
                lastPercentage = percentage;
                emit progress(percentage, checked);
            }
        }
        else
        {
            emit progress(-1, checked);
        }

        if (!stillGood)
        {
            break;
        }
    }

    const bool complete = endOfFile && !cancelRequested;
    emit finished(!analyser.HasFailed(),
                  describeResult(analyser.GetResult(), QString::fromStdString(path.filename().string()), complete));
}

//----------------------------------------------------------------------------------------------------------------------
TestDataAnalysisDialog::TestDataAnalysisDialog(QWidget *parent) : QDialog(parent)
{
    ui.reset(new Ui::TestDataAnalysisDialog());
    ui->setupUi(this);

    connect(ui->cancelPushButton, &QPushButton::clicked, this, &TestDataAnalysisDialog::onCancelClicked);
}

//----------------------------------------------------------------------------------------------------------------------
TestDataAnalysisDialog::~TestDataAnalysisDialog()
{
    if (worker != nullptr)
    {
        worker->requestCancel();
        worker->wait();
    }
}

//----------------------------------------------------------------------------------------------------------------------
void TestDataAnalysisDialog::chooseFileAndAnalyse(const QString &startingDirectory)
{
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Select a test-mode capture"), startingDirectory,
        tr("Capture files (*.ldf *.lds *.raw);;FLAC captures (*.ldf);;All files (*)"));

    if (filePath.isEmpty())
    {
        return;
    }

    startAnalysis(filePath);
    exec();
}

//----------------------------------------------------------------------------------------------------------------------
void TestDataAnalysisDialog::startAnalysis(const QString &filePath)
{
    ui->resultLabel->setText(tr("Analysing %1...").arg(QFileInfo(filePath).fileName()));
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);
    ui->cancelPushButton->setEnabled(true);

    worker = new TestDataAnalysisWorker(filePath, this);
    connect(worker, &TestDataAnalysisWorker::progress, this, &TestDataAnalysisDialog::onProgress);
    connect(worker, &TestDataAnalysisWorker::finished, this, &TestDataAnalysisDialog::onFinished);
    worker->start();
}

//----------------------------------------------------------------------------------------------------------------------
void TestDataAnalysisDialog::onProgress(int percentage, qint64 samplesChecked)
{
    if (percentage < 0)
    {
        // No total sample count — a streamed FLAC whose header was never patched. Show a
        // busy indicator rather than inventing a percentage.
        ui->progressBar->setRange(0, 0);
    }
    else
    {
        ui->progressBar->setRange(0, 100);
        ui->progressBar->setValue(percentage);
    }

    ui->resultLabel->setText(tr("Checked %1 samples...").arg(QLocale().toString(samplesChecked)));
}

//----------------------------------------------------------------------------------------------------------------------
void TestDataAnalysisDialog::onFinished(bool ok, QString message)
{
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(100);
    ui->cancelPushButton->setText(tr("Close"));
    ui->resultLabel->setText(message);

    // Colour rather than an icon, so the verdict is readable across the room at a bench
    ui->resultLabel->setStyleSheet(ok ? "color: green" : "color: red");
}

//----------------------------------------------------------------------------------------------------------------------
void TestDataAnalysisDialog::onCancelClicked()
{
    if (worker != nullptr && worker->isRunning())
    {
        worker->requestCancel();
        return;
    }

    accept();
}

//----------------------------------------------------------------------------------------------------------------------
int TestDataAnalysisDialog::RunHeadless(const QString &filePath)
{
    QTextStream out(stdout);
    QTextStream err(stderr);

    const std::filesystem::path path = filePath.toStdString();

    const auto format = CaptureReader::FormatFromExtension(path);
    if (!format.has_value())
    {
        err << "Error: " << filePath << " is not a capture file (expected .ldf, .lds or .raw)\n";
        return 2;
    }

    CaptureReader reader;
    std::string errorMessage;
    if (!reader.Open(path, *format, errorMessage))
    {
        err << "Error: could not open " << filePath << ": " << QString::fromStdString(errorMessage) << "\n";
        return 2;
    }

    out << "Analysing " << filePath << " as " << CaptureReader::FormatName(*format) << "\n";
    out.flush();

    TestDataAnalyser analyser;
    std::vector<uint16_t> samples;
    bool endOfFile = false;

    while (!endOfFile)
    {
        if (!reader.Read(samples, analysisChunkSamples, endOfFile))
        {
            err << "Error: failed to read " << filePath << ": " << QString::fromStdString(reader.GetLastError()) << "\n";
            return 2;
        }
        if (samples.empty())
        {
            break;
        }
        if (!analyser.Feed(samples.data(), samples.size()))
        {
            break;
        }
    }

    const TestDataAnalyser::Result &result = analyser.GetResult();
    out << describeResult(result, QString::fromStdString(path.filename().string()), endOfFile) << "\n";

    return result.passed ? 0 : 1;
}
